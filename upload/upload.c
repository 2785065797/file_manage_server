#include <stdio.h>
#include <stdlib.h>
//#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fdfs_client.h>
#include <sys/wait.h>
//#include<iostredam>
#include "fcgi_stdio.h"
#include <fcntl.h>
#include <mysql.h>
#include <mysqld_error.h>
#include <unistd.h>
//#include <sw/redis++/redis++.h>
#include <hiredis/hiredis.h>
#include<string.h>

void upload_file1(const char *confFile, const char *uploadFile, char *fileID, int size) {
	//1. 创建管道
	int fd[2];
	int ret = pipe(fd);
	if (ret == -1) {
		perror("pipe error");
		exit(0);
	}
	//2. 创建子进程
	pid_t pid = fork();
	//子进程
	if (pid == 0) {
		//3. 标准输出重定向 -> pipe写端
		dup2(fd[1], STDOUT_FILENO);
		//4. 关闭读端
		close(fd[0]);
		execlp("fdfs_upload_file", "fdfs_upload_file", confFile, uploadFile, NULL);
		perror("execlp error");
	}
	else {
		//父进程读管道
		close(fd[1]);
		read(fd[0], fileID, size);
		//回收子进程的PCB
		wait(NULL);
	}

}

// 取出 Content-Disposition 中的键值对的值, 并得到文件内容, 并将内容写入文件
int recv_save_file(char *user, char *filename, char *md5, long *p_size,char *url)
{
	int ret = 0;
	char *file_buf = NULL;
	char *begin = NULL;
	char *p, *q, *k;

	char content_text[512] = {0}; //文件头部信息
	char boundary[512] = {0};     //分界线信息

	//==========> 开辟存放文件的 内存 <===========
	file_buf = (char *)malloc(4096);
	if (file_buf == NULL)
	{
		return -1;
	}

	//从标准输入(web服务器)读取内容
	int len = fread(file_buf, 1, 4096, stdin); 
	if(len == 0)
	{
		ret = -1;
		free(file_buf);
		return ret;
	}

	//===========> 开始处理前端发送过来的post数据格式 <============
	begin = file_buf;    //内存起点
	p = begin;

	/*
	   ------WebKitFormBoundary88asdgewtgewx\r\n
	   Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
	   Content-Type: application/octet-stream\r\n
	   ------WebKitFormBoundary88asdgewtgewx--
	   */

	//get boundary 得到分界线, ------WebKitFormBoundary88asdgewtgewx
	p = strstr(begin, "\r\n");
	if (p == NULL)
	{
		ret = -1;
		free(file_buf);
		return ret;
	}

	//拷贝分界线
	strncpy(boundary, begin, p-begin);
	boundary[p-begin] = '\0';   //字符串结束符
	p += 2; //\r\n
		//已经处理了p-begin的长度
	len -= (p-begin);
	//get content text head
	begin = p;

	//Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
	p = strstr(begin, "\r\n");
	if(p == NULL)
	{
		ret = -1;
		free(file_buf);
		return ret;
	}
	strncpy(content_text, begin, p-begin);
	content_text[p-begin] = '\0';

	p += 2;//\r\n
	len -= (p-begin);

	//========================================获取文件上传者
	//Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
	q = begin;
	q = strstr(begin, "user=");
	q += strlen("user=");
	q++;    //跳过第一个"
	k = strchr(q, '"');
	strncpy(user, q, k-q);  //拷贝用户名
	user[k-q] = '\0';

	//========================================获取文件名字
	//"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
	begin = k;
	q = begin;
	q = strstr(begin, "filename=");
	q += strlen("filename=");
	q++;    //跳过第一个"
	k = strchr(q, '"');
	strncpy(filename, q, k-q);  //拷贝文件名
	filename[k-q] = '\0';

	//========================================获取文件MD5码
	//"; md5="xxxx"; size=10240\r\n
	begin = k;
	q = begin;
	q = strstr(begin, "md5=");
	q += strlen("md5=");
	q++;    //跳过第一个"
	k = strchr(q, '"');
	strncpy(md5, q, k-q);   //拷贝文件名
	md5[k-q] = '\0';

	//========================================获取文件大小
	//"; size=10240\r\n
	begin = k;
	q = begin;
	q = strstr(begin, "size=");
	q += strlen("size=");
	k = strstr(q, "\r\n");
	char tmp[256] = {0};
	strncpy(tmp, q, k-q);   //内容
	tmp[k-q] = '\0';
	*p_size = strtol(tmp, NULL, 10); //字符串转long

	begin = p;
	p = strstr(begin, "\r\n");
	p += 4; //\r\n
	len -= (p-begin);

	//下面才是文件的真正内容
	/*
	   ------WebKitFormBoundary88asdgewtgewx\r\n
	   Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
	   Content-Type: application/octet-stream\r\n
	   真正的文件内容\r\n
	   ------WebKitFormBoundary88asdgewtgewx--
	   */
	// begin指向正文首地址
	begin = p;

	// 将文件内容抠出来
	// 文件内容写如本地磁盘文件
	char temp_file[128];
	sprintf(temp_file,"/home/yyj/temp/%s",filename);
	FILE * fp=fopen(temp_file,"wb");
	fwrite(begin,1,len,fp);
	// 1. 文件已经读完了
	// 2. 文件还没读完
	if(*p_size > len)
	{
		// 读文件 -> 接收post数据
		// fread , read  , 返回值>0== 实际读出的字节数; ==0, 读完了; -1, error
		while( (len = fread(file_buf, 1, 4096, stdin)) > 0 )
		{
			// 读出的数据写文件
			fwrite(file_buf,1,len,fp);
		}
	}
	// 3. 将写入到文件中的分界线删除
	fseek(fp,0,SEEK_SET);
	ftruncate(fileno(fp),*p_size);
	fclose(fp);
	
	//char conf_file[128]="/etc/fdfs/client.conf";
	upload_file1("/etc/fdfs/client.conf", temp_file, url, 512);
	url[strlen(url)-1]='\0';
	free(file_buf);
	remove(temp_file);
	return ret;
}

int redis(const char* md5,const char* url){
	redisContext *c = redisConnect("127.0.0.1", 6379);
	if (c == NULL || c->err) {
		if (c) {
			redisFree(c);
		}
		return -1;
	}
	redisReply *reply;	
	reply = redisCommand(c, "SET %s %s",md5,url);
	if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
		freeReplyObject(reply);
		redisFree(c);
		return -1;
	}
	freeReplyObject(reply);
	redisFree(c);
	return 0;
}

int authenticateUser(const char* user,const char* fileName,const char* md5,long size,const char* url){
	MYSQL *conn=mysql_init(NULL);
	if(conn==NULL){
		//printf("Content-type: application/json\r\n\r\n");
		//printf("{\"code1\":\"001\"}");
		return -1;
	}
	if(mysql_real_connect(conn,"192.168.182.26","virtual","1","cloud_disk",0,NULL,0)==NULL){
		mysql_close(conn);
		return -1;
	}
	// 这里应该有代码来检查用户的凭据，例如查询数据库
	char query[BUFSIZ];
	sprintf(query, "INSERT INTO user_file_list(user, md5, createtime, filename, shared_status, pv) VALUES('%s','%s',CURRENT_TIMESTAMP,'%s',0,0)", user,md5,fileName);
	if(mysql_query(conn,query)){
		unsigned int error_code = mysql_errno(conn);
		if (error_code == ER_DUP_ENTRY) {
			mysql_close(conn);
			return 0; // 文件已存在
		}
		else {
			mysql_close(conn);
			return -1; // 插入数据失败
		}
	}
	sprintf(query,"UPDATE user_file_count SET count = count + 1  WHERE user = '%s'",user);
	if(mysql_query(conn,query)){
		mysql_close(conn);
		return -1; // 添加数据失败
	}
	sprintf(query, "INSERT INTO file_info(md5, filename, url, size, type, count) VALUES('%s', '%s' ,'%s','%ld','type',1)", md5,fileName,url,size);
	if(mysql_query(conn,query)){
		unsigned int error_code = mysql_errno(conn);
		if (error_code == ER_DUP_ENTRY) {
			mysql_close(conn);
			return 0; // 文件已存在
		}
		else {
			mysql_close(conn);
			return -1; // 插入数据失败
		}
	}
	mysql_close(conn);
	return 1;
}
int main()
{
	while(FCGI_Accept() >= 0)
	{
		char user[24];
		char fileName[128];
		char md5[200];
		long size;
		char url[512]={};
		if(recv_save_file(user, fileName, md5, &size,url)==-1){
			printf("Content-type: text/plain\r\n\r\n");
			printf("{\"code\":\"009\"}");
		}
		if(redis(md5,url)==-1){
			printf("Content-type: text/plain\r\n\r\n");
			printf("{\"code\":\"009\"}");
		}
		int flag=authenticateUser(user,fileName,md5,size,url);
		if(flag==1){	
			printf("Content-type: text/plain\r\n\r\n");
			printf("{\"code\":\"008\"}");
		}
		else{
			printf("Content-type: text/plain\r\n\r\n");
			printf("{\"code\":\"009\"}");
		}
	}
	return 0;
}
