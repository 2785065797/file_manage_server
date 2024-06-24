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

Logger dealsharefile_logger;

int is_exist(MYSQL* conn,const char* username,const char* filename,const char* md5){
        char query[BUFSIZ];
        sprintf(query,"SELECT * FROM user_file_list WHERE user='%s' and filename='%s'and md5='%s';",username,filename,md5);
        if(mysql_query(conn,query)){
                return -1;
        }
        // 如果凭据正确，返回true，否则返回false
        MYSQL_RES *result=mysql_store_result(conn);
        if(result==NULL){
        	LOG4CPLUS_DEBUG(dealsharefile_logger, "is_exist #mysql_result_error" );
                return -1;
        }
	if(mysql_num_rows(result)>0){
		mysql_free_result(result);
		return 2;
	}
	else if(mysql_num_rows(result)==0){
        	mysql_free_result(result);
        	return 0;
	}
	else{
		mysql_free_result(result);
		return -1;
	}
}
// 为用户添加文件的函数
int add_file_for_user(MYSQL *conn, const char *user, const char *md5,const char *filename) {
        // 构建插入语句
        char query[1024];
	int len=sizeof(query);
        snprintf(query, len, "INSERT INTO user_file_list (user, md5, createtime, filename, shared_status, pv) VALUES ('%s', '%s', CURRENT_TIMESTAMP, '%s', 0, 0)", user, md5, filename);
        // 执行插入
        if (mysql_query(conn, query)) {
                // 插入失败
                return -1; // 或者你可以记录错误并返回0
        }
        snprintf(query,len,"update user_file_count set count=count+1 where user ='%s'",user);
        if (mysql_query(conn, query)) {
                // 插入失败
                return -1; // 或者你可以记录错误并返回0
        }
        snprintf(query,len,"update file_info set count=count+1 where md5 ='%s' and filename='%s'",md5,filename);
        if (mysql_query(conn, query)) {
                // 插入失败
                return -1; // 或者你可以记录错误并返回0
        }
        snprintf(query,len,"update share_file_list set pv=pv+1 where md5 ='%s' and filename='%s'",md5,filename);
        if (mysql_query(conn, query)) {
                // 插入失败
                return -1; // 或者你可以记录错误并返回0
        }

        // 插入成功
        return 1; // 插入成功
}


int del_shared_file(MYSQL* conn,const char* username,const char* md5,const char* filename){
        char query[BUFSIZ];
        snprintf(query,sizeof(query),"delete from share_file_list where user='%s' and md5='%s' and filename='%s'",username,md5,filename);
        if(mysql_query(conn,query)){
                return -1;
        }
        LOG4CPLUS_DEBUG(dealsharefile_logger,"del_shared_file #successful deleted shared_file");
        my_ulonglong affected_rows=mysql_affected_rows(conn);
        if(affected_rows>0){
                return 1;
        }
        else return -1;
}

int cancel_shared(MYSQL* conn,const char* username,const char* md5,const char* filename){
        char query[BUFSIZ];
        LOG4CPLUS_DEBUG(dealsharefile_logger,"cancel_shared #plan to update shared_status=0");
        snprintf(query,sizeof(query),"update user_file_list set shared_status=0 where user='%s' and md5='%s' and filename='%s'",username,md5,filename);
        if(mysql_query(conn,query)){
                return -1;
        }
        LOG4CPLUS_DEBUG(dealsharefile_logger,"cancel_shared #successful to update shared_status=0");
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
	if(mysql_query(conn,"START TRANSACTION;")){
		return -1;
	}
	if(strcmp(cmd,"cmd=cancel")==0){
		int flag=cancel_shared(conn,username,md5,filename);
        	if(-1== del_shared_file(conn,username,md5,filename)){
			flag==-1;
		}
		if(flag==1){
			printf("{\"code\":\"018\"}");
		}
		else{
			printf("{\"code\":\"019\"}");
		}
	}
	else if(strcmp(cmd,"cmd=save")==0){
		int flag;
		flag=is_exist(conn,username,filename,md5);
		if(flag==0){
			flag=add_file_for_user(conn,username,md5,filename);
		}

		if(flag==2){
			printf("{\"code\":\"021\"}");
		}
		else if(flag==1){
			printf("{\"code\":\"020\"}");
		}
		else{
			printf("{\"code\":\"022\"}");
		}
	}
	else{
		if(mysql_query(conn,"COMMIT;")) return -1;
		mysql_close(conn);
		return -1;
	}
	if(mysql_query(conn,"COMMIT;")){
		return -1;
	}
	mysql_close(conn);
	return 1;
}

int main(){
	//日志初始化
	log4cplus::Initializer initializer;

	/* step 1: Instantiate an appender object */
	SharedAppenderPtr  _append(new RollingFileAppender("dealsharefile.log", 300 * 1024, 5));
	_append->setName("dealsharefile_appender");

	/* step 2: Instantiate a layout object */
	/* step 3: Attach the layout object to the appender */
	std::string pattern = "%D{%m/%d/%y %H:%M:%S,%q} [%-5t] [%-5p] - %m%n";
	_append->setLayout(std::unique_ptr<Layout>(new PatternLayout(pattern)));

	/* step 4: Instantiate a logger object */
	dealsharefile_logger = Logger::getInstance("dealsharefile_logger");

	/* step 5: Attach the appender object to the logger  */
	dealsharefile_logger.addAppender(_append);

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
		const char* md5=root["md5"].asCString();
		const char* filename=root["filename"].asCString();
		printf( "Content-type: application/json\r\n\r\n");
		if(authenticateUser(username,md5,filename,cmd)!=1){
			printf("{\"code\":\"xxx\"}");
		}
		free(content);
	}
	return 0;
}

