#include<iostream>
#include<string.h>
#include"fcgi_stdio.h"
#include"fcgi_config.h"
#include<mysql.h>
#include <unistd.h>
#include <sys/wait.h>
#include<fastcgi.h>
#include<json.h>
#include<string.h>
#include <hiredis/hiredis.h>
#include<thread>
#include <log4cplus/logger.h>
#include <log4cplus/initializer.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/helpers/property.h>
#include <iomanip>
#include<memory>
using namespace std;
using namespace log4cplus;

Logger dealfile_logger;

int find_url(MYSQL* conn,const char* filename,const char* md5,char* url){
	char query[BUFSIZ];
	sprintf(query,"SELECT url FROM file_info WHERE filename='%s'and md5='%s';",filename,md5);
	if(mysql_query(conn,query)){
		mysql_close(conn);
		return -1;
	}
	// 如果凭据正确，返回true，否则返回false
	MYSQL_RES *result=mysql_store_result(conn);
	if(result==NULL){
		mysql_close(conn);
		return -1;
	}
	MYSQL_ROW row;
	if ((row = mysql_fetch_row(result)) != NULL) {
		strcpy(url,row[0]);
		url[strlen(url)]='\0';
	}
	LOG4CPLUS_DEBUG(dealfile_logger, "find_url #url=" <<url );
	mysql_free_result(result);
	return 1;
}
void del_fdfs_file(const char *url,int *flag){
	int fd[2];
	int  ret =pipe(fd);
	if(ret==-1){
		*flag=-1;
		exit(0);
	}
	pid_t pid=fork();
	if(pid==0){
		dup2(fd[1],STDOUT_FILENO);
		close(fd[0]);
		execlp("fdfs_delete_file","fdfs_delete_file","/etc/fdfs/client.conf",url,NULL);
		*flag=-1;
	}
	else{
		close(fd[1]);
		char buf[1024];
		read(fd[0],buf,sizeof(buf));
		LOG4CPLUS_DEBUG(dealfile_logger,"del_fdfs_file #stdin="<<buf);
		//此处可以判断是否删除成功
		wait(NULL);
	}
}
int del_redis(const char* md5){
	redisContext* c=redisConnect("127.0.0.1",6379);
	if(c==NULL||c->err){
		if(c){
			redisFree(c);
		}
		return -1;
	}
	char cmd[200];
	snprintf(cmd,sizeof(cmd),"DEL %s",md5);
	if(cmd==NULL){
		redisFree(c);
		return -1;
	}
	redisReply* reply=(redisReply*)redisCommand(c,cmd);
	if(reply==NULL||reply->type==REDIS_REPLY_ERROR){
		if(reply){
			freeReplyObject(reply);
		}
		redisFree(c);
		return -1;
	}
	// 检查是否成功删除了键  
	//if (reply->integer > 0) {  
	//	printf("Key deleted successfully\n");  
	//} else {  
	//	printf("Key does not exist or no keys were deleted\n");  
	//}  	
	if (reply->integer > 0) {  
		LOG4CPLUS_DEBUG(dealfile_logger,"del_redis # successful deleted redis data");
	}
	freeReplyObject(reply);
	redisFree(c);	
	return 1;
}

void thread_runner(const char* md5,int* flag){
	*flag=del_redis(md5);
}	

int del_file_info(MYSQL* conn,const char* username,const char* md5,const char* filename){
	char query[BUFSIZ];
	LOG4CPLUS_DEBUG(dealfile_logger,"del_file_info # plan to delete file_info data");
	snprintf(query,sizeof(query),"update file_info set count=count-1 where filename='%s' and md5='%s';",filename,md5);
	if(mysql_query(conn,query)){
		mysql_query(conn,"ROLLBACK;");
		return -1;
	}
	my_ulonglong affected_rows=mysql_affected_rows(conn);
	if(affected_rows<=0){
		return -1;
	}

	snprintf(query,sizeof(query),"delete from file_info where filename='%s' and md5='%s' and count=0;",filename,md5);
	if(mysql_query(conn,query)){
		mysql_query(conn,"ROLLBACK;");
		return -1;
	}
	LOG4CPLUS_DEBUG(dealfile_logger,"del_file_info # successful deleted file_info data");
	return 1;
}
int del_count(MYSQL* conn,const char* username){
	char query[BUFSIZ];
	LOG4CPLUS_DEBUG(dealfile_logger,"del_count # start plan sql user="<<username);
	sprintf(query,"update user_file_count set count=count-1 where user='%s'",username);
	if(mysql_query(conn,query)){
		LOG4CPLUS_DEBUG(dealfile_logger,"MySQL query error: "<< mysql_error(conn)); 
		return -1;
	}
	LOG4CPLUS_DEBUG(dealfile_logger,"del_count # start plan to delete user_count");
	my_ulonglong affected_rows=mysql_affected_rows(conn);
	if(affected_rows>0){
		return 1;
		LOG4CPLUS_DEBUG(dealfile_logger,"del_count # successful deleted user_count");
	}
	else{
		return -1;
	}
}
int del_shared(MYSQL* conn,const char* username,const char* md5,const char* filename){
	char query[BUFSIZ];
	LOG4CPLUS_DEBUG(dealfile_logger,"del_shared # start plan to delete shared data");
	snprintf(query,sizeof(query),"select * from share_file_list where user='%s' and  md5='%s' and filename='%s'",username,md5,filename);
	if(mysql_query(conn,query)){
		return -1;
	}
	MYSQL_RES* result=mysql_store_result(conn);
	if(result==NULL){
		return -1;
	}
	LOG4CPLUS_DEBUG(dealfile_logger,"del_shared # successful find shared data");
	if(mysql_num_rows(result)>0){
		mysql_free_result(result);
		snprintf(query,sizeof(query),"delete from share_file_list where user='%s' and md5='%s' and filename='%s'",username,md5,filename);
		if(mysql_query(conn,query)){
			return -1;
		}
		my_ulonglong affected_rows=mysql_affected_rows(conn);
		if(affected_rows>0){
			LOG4CPLUS_DEBUG(dealfile_logger,"del_shared # successful deleted shared data");
			return 1;
		}
		else{
			return -1;
		}

	}
	else if(mysql_num_rows(result)==0){
		mysql_free_result(result);
		return 0;
	}
	else {
		mysql_free_result(result);
		return -1;
	}
}
int del_user_file(MYSQL* conn,const char* username,const char* md5,const char* filename){
	char query[BUFSIZ];
	snprintf(query,sizeof(query),"delete from user_file_list where user='%s' and md5='%s' and filename='%s'",username,md5,filename);
	if(mysql_query(conn,query)){
		return -1;
	}
	LOG4CPLUS_DEBUG(dealfile_logger,"del_user_file #successful deleted user_file");
	my_ulonglong affected_rows=mysql_affected_rows(conn);
	if(affected_rows>0){
		return 1;
	}
	else return -1;
}

int del(MYSQL* conn,const char* username,const char* md5,const char* filename){
	int flag,ret,key=1;
	char url[512];
	std::thread my_thread(thread_runner,md5,&flag);
	if(mysql_query(conn,"START TRANSACTION;")){
                key=-1;
        }
	if(del_user_file(conn,username,md5,filename)==-1){
		key=-1;
	}
	if(find_url(conn,filename,md5,url)==-1){
		key=-1;
	}
	if(del_count(conn,username)==-1){
		key=-1;
	}
	if(del_file_info(conn,username,md5,filename)==-1){
		key=-1;
	}
	if(del_shared(conn,username,md5,filename)==-1){
		key=-1;
	}
	del_fdfs_file(url,&ret);
	if(ret==-1){
		key=-1;
	}
	if(key==-1){
		mysql_query(conn,"ROLLBACK;");
		return -1;
	}
	if(mysql_query(conn,"COMMIT;")){
                return -1;
        }

	my_thread.join();
	if(flag==-1){
		return -1;
	}
	return key;
}

int isShared(MYSQL* conn,const char* md5,const char* filename){
	char query[BUFSIZ];
	snprintf(query,sizeof(query),"select * from share_file_list where md5='%s' and filename='%s'",md5,filename);
	if(mysql_query(conn,query)){
		return -1;
	}
	MYSQL_RES* result=mysql_store_result(conn);
	if(result==NULL){
		return -1;
	}
	if(mysql_num_rows(result)>0){
		mysql_free_result(result);
		return 1;
	}
	else if(mysql_num_rows(result)==0){
		mysql_free_result(result);
		return 0;
	}
	else {
		mysql_free_result(result);
		return -1;
	}
}
int share(MYSQL* conn,const int flag,const char* username,const char* md5,const char* filename){
	char query[BUFSIZ];
	if(flag==1){
		return 2;
	}
	else if(flag==0){
		snprintf(query,sizeof(query),"INSERT INTO share_file_list (user, md5, createtime, filename, pv) VALUES ('%s', '%s', CURRENT_TIMESTAMP, '%s', 0)",username,md5,filename);
		if(mysql_query(conn,query)){
			return -1;
		}
		my_ulonglong affected_rows=mysql_affected_rows(conn);
		if(affected_rows>0){
			sprintf(query,"update user_file_list set shared_status=1 where user ='%s'and md5='%s'and filename='%s'",username,md5,filename);
			if(mysql_query(conn,query)){
				return -1;
			}
			affected_rows=mysql_affected_rows(conn);
			if(affected_rows>0){
				return 1;
			}
			else{
				return -1;	
			}
		}
		else{
			return -1;
		}
	}
	else{
		return -1;
	}
}
int pv(MYSQL* conn,const char* username,const char* md5,const char* filename){
	char query[BUFSIZ];
	snprintf(query,sizeof(query),"update user_file_list set pv=pv+1 where user='%s' and md5='%s' and filename='%s'",username,md5,filename);
	if(mysql_query(conn,query)){
		return -1;
	}
	my_ulonglong affected_rows=mysql_affected_rows(conn);
	if(affected_rows>0){
		return 1;
	}
	else{
		return -1;
	}
}
int authenticateUser(const char* username,const char* md5,const char* filename,const char* cmd){
	MYSQL *conn=mysql_init(NULL);
	if(conn==NULL){
		return -1;
	}
	if(mysql_real_connect(conn,"192.168.182.26","virtual","1","cloud_disk",0,NULL,0)==NULL){
		mysql_close(conn);
		return -1;
	}
	if(strcmp(cmd,"cmd=pv")==0){	
		int flag=pv(conn,username,md5,filename);
		if(flag==1){
			printf("{\"code\":\"016\"}");
		}
		else{
			printf("{\"code\":\"017\"}");
		}
	}
	else if(strcmp(cmd,"cmd=share")==0){
		int flag=isShared(conn,md5,filename);
		int ret=share(conn,flag,username,md5,filename);
		if(ret==2){
			printf("{\"code\":\"012\"}");
		}
		else if(ret==1){
			printf("{\"code\":\"010\"}");
		}
		else{
			printf("{\"code\":\"011\"}");
		}
	}
	else if(strcmp(cmd,"cmd=del")==0){
		if(del(conn,username,md5,filename)==1){
			printf("{\"code\":\"013\"}");
		}
		else{
			printf("{\"code\":\"014\"}");
		}
	}
	else{
		mysql_close(conn);
		return -1;
	}
	mysql_close(conn);
	return 1;
}

bool validateToken(const std::string& token) {
	// 在这里实现你的 token 验证逻辑  
	// 例如，从数据库或缓存中查找 token 是否有效，并与 userId 匹配  
	// ...  
	return true; // 示例：总是返回 true，实际实现中需要替换  
}

int main(){
	//日志初始化
	log4cplus::Initializer initializer;

	/* step 1: Instantiate an appender object */
	SharedAppenderPtr  _append(new RollingFileAppender("dealfile.log", 300 * 1024, 5));
	_append->setName("dealfile_appender");

	/* step 2: Instantiate a layout object */
	/* step 3: Attach the layout object to the appender */
	std::string pattern = "%D{%m/%d/%y %H:%M:%S,%q} [%-5t] [%-5p] - %m%n";
	_append->setLayout(std::unique_ptr<Layout>(new PatternLayout(pattern)));

	/* step 4: Instantiate a logger object */
	dealfile_logger = Logger::getInstance("dealfile_logger");

	/* step 5: Attach the appender object to the logger  */
	dealfile_logger.addAppender(_append);

	//开始接听
	while(FCGI_Accept()>=0){
		char* contentLengthStr=getenv("CONTENT_LENGTH");
		char* cmd=getenv("QUERY_STRING");
		if(contentLengthStr==NULL){
			printf("Content-type: text/html\r\n\r\n");
			printf("Error:Missing Content-Length header.\r\n");
			return -1;
		}
		int contentLength=atoi(contentLengthStr);
		char* content=(char*)malloc(contentLength+1);
		if(content==NULL){
			printf("Content-type: text/html\r\n\r\n");
			printf("Error: Faild to allocate memory.\r\n");
			return -1;
		}
		fread(content,1,contentLength,stdin);
		content[contentLength]='\0';
		Json::Value root;
		Json::Reader reader;
		if(!reader.parse(content,root)){
			printf("Content-type: text/html\r\n\r\n");
			printf("Error: Invalid JSON.\r\n");
			free(content);
			return -1;
		}
		const char* username=root["user"].asCString();
		const char* token=root["token"].asCString();
		const char* md5=root["md5"].asCString();
		const char* filename=root["filename"].asCString();
		if(!validateToken(token)){
			printf( "Content-type: application/json\r\n\r\n");
			printf("{\"code\":\"111\"}");
			free(content);
		}
		printf( "Content-type: application/json\r\n\r\n");
		if(authenticateUser(username,md5,filename,cmd)!=1){
			printf("{\"code\":\"xxx\"}");	
		}
		free(content);
	}
	return 0;
}









