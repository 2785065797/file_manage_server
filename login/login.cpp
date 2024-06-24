//#include <stdio.h>
//#include <stdlib.h>
#include<iostream>
using namespace std;
#include <string.h>
#include "fcgi_stdio.h"
#include"fcgi_config.h"
#include<mysql.h>
#include<fastcgi.h>
#include <json.h>
#include<string>
// 假设您有一个数据库或者用户管理类来验证用户
int authenticateUser(const char* username, const char* password) {
	MYSQL *conn=mysql_init(NULL);
	if(conn==NULL){
		return -1;
	}
	if(mysql_real_connect(conn,"192.168.182.26","virtual","1","cloud_disk",0,NULL,0)==NULL){
		mysql_close(conn);
		return -1;
	}
	// 这里应该有代码来检查用户的凭据，例如查询数据库
	char query[BUFSIZ];
	sprintf(query,"select * from user where name= '%s' and password= '%s'",username,password);
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
	if(mysql_num_rows(result)>0){
		mysql_close(conn);
		return 1; // 为了示例，这里总是返回true
	}
	else {	
		mysql_close(conn);
		return -1;
	}
}

int main() {
	while (FCGI_Accept() >= 0) {
		// 1. 根据content-length得到post数据块的长度
		char* contentLengthStr = getenv( "CONTENT_LENGTH");
		if (contentLengthStr == NULL) {
			printf( "Content-type: text/html\r\n\r\n");
			printf("Error: Missing Content-Length header.\r\n");
			return -1;
		}
		int contentLength = atoi(contentLengthStr);

		// 2. 根据长度将post数据块读到内存
		char* content = (char*)malloc(contentLength + 1);
		if (content == NULL) {
			printf("Content-type: text/html\r\n\r\n");
			printf("Error: Failed to allocate memory.\r\n");
			return -1;
		}
		fread(content,1,contentLength,stdin);
		content[contentLength] = '\0';

		// 3. 解析json对象, 得到用户名, 密码, 昵称, 邮箱, 手机号
		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(content, root)) {
			printf("Content-type: text/html\r\n\r\n");
			printf( "Error: Invalid JSON.\r\n");
			free(content);
			return -1;
		}
		const char* username = root["user"].asCString();
		const char* password = root["pwd"].asCString();
		printf( "Content-type: application/json\r\n\r\n");
		if(authenticateUser(username,password)==1){
			printf("{\"code\":\"000\"}");
		}
		else{
			printf("{\"code\":\"001\"}");
		}
		free(content);
	}
	return 0;
}

