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
int check_file_exists(MYSQL *conn, const char *file_name,const char *md5) {
	// 构建查询语句
	char query[1024];
	sprintf(query,"select * from file_info where md5= '%s' and filename= '%s'",md5,file_name);

	// 执行查询
	if (mysql_query(conn, query)) {
		// 查询失败
		return -1; // 或者你可以记录错误并返回0
	}

	// 获取结果集
	MYSQL_RES *result = mysql_store_result(conn);
	if (result == NULL) {
		// 没有结果集或错误
		return -1; 
	}

	// 检查结果集中的行数
	if (mysql_num_rows(result) > 0) {
		// 文件存在
		mysql_free_result(result); // 释放结果集
		return 1; // 文件存在
	}

	mysql_free_result(result); // 释放结果集
	return 0; // 文件不存在
}

// 检查用户是否拥有文件的函数
int check_user_owns_file(MYSQL *conn, const char *user, const char *file_name,const char *md5) {
	// 构建查询语句
	char query[1024];
	sprintf(query,"select * from user_file_list where md5= '%s' and filename= '%s' and user= '%s'",md5,file_name,user);

	// 执行查询
	if (mysql_query(conn, query)) {
		// 查询失败
		return -1; // 或者你可以记录错误并返回0
	}

	// 获取结果集
	MYSQL_RES *result = mysql_store_result(conn);
	if (result == NULL) {
		// 没有结果集或错误
		return -1; // 用户不拥有文件
	}

	// 检查结果集中的行数
	if (mysql_num_rows(result) > 0) {
		// 用户拥有文件
		mysql_free_result(result); // 释放结果集
		return 2; // 用户拥有文件
	}

	mysql_free_result(result); // 释放结果集
	return 0; // 用户不拥有文件
}

// 为用户添加文件的函数
int add_file_for_user(MYSQL *conn, const char *user, const char *md5,const char *file_name) {
	// 构建插入语句
	char query[1024];
	snprintf(query, sizeof(query), "INSERT INTO user_file_list (user, md5, createtime, filename, shared_status, pv) VALUES ('%s', '%s', CURRENT_TIMESTAMP, '%s', 0, 0)", user, md5, file_name);

	// 执行插入
	if (mysql_query(conn, query)) {
		// 插入失败
		return -1; // 或者你可以记录错误并返回0
	}
	sprintf(query,"update user_file_count set count=count+1 where user ='%s'",user);
	if (mysql_query(conn, query)) {
		// 插入失败
		return -1; // 或者你可以记录错误并返回0
	}
	sprintf(query,"update file_info set count=count+1 where md5 ='%s'",md5);
        if (mysql_query(conn, query)) {
                // 插入失败
                return -1; // 或者你可以记录错误并返回0
        }

	// 插入成功
	return 1; // 插入成功
}
// 假设您有一个数据库或者用户管理类来验证用户
int authenticateUser(const char* username, const char* md5,const char* fileName) {
	MYSQL *conn=mysql_init(NULL);
	if(conn==NULL){
		return -1;
	}
	if(mysql_real_connect(conn,"192.168.182.26","virtual","1","cloud_disk",0,NULL,0)==NULL){
		mysql_close(conn);
		return -1;
	}

	int flag=check_user_owns_file(conn, username, fileName,md5);
	// 这里应该有代码来检查用户的凭据，例如查询数据库
	if(flag==0){
		flag=check_file_exists(conn, fileName,md5);
	}
	if(flag==1){
		flag=add_file_for_user(conn, username, md5,fileName); 
	}
	mysql_close(conn);
	return flag;
}

bool validateToken(const std::string& token) {  
	// 在这里实现你的 token 验证逻辑  
	// 例如，从数据库或缓存中查找 token 是否有效，并与 userId 匹配  
	// ... 
	return true; // 示例：总是返回 true，实际实现中需要替换  
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
		const char* token = root["token"].asCString();
		const char* md5=root["md5"].asCString();
		const char* fileName=root["fileName"].asCString();
		// ... 其他字段
		if(!validateToken(token)){
			printf( "Content-type: application/json\r\n\r\n");
			printf("{\"code\":\"111\"}");
			free(content);
		}
		int flag=authenticateUser(username,md5,fileName);
		printf( "Content-type: application/json\r\n\r\n");
		if(flag==2){
			printf("{\"code\":\"005\"}");
		}
		else if(flag==1){
			printf("{\"code\":\"006\"}");
		}
		else if(flag==0){
			printf("{\"code\":\"007\"}");
		}
		else {
			printf("{\"code\":\"00x\"}");
		}
		free(content);
	}
	return 0;
}

