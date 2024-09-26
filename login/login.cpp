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
#include<token.h>
#include<string>
// 假设您有一个数据库或者用户管理类来验证用户
int authenticateUser(const char* username, const char* password) {

	MprpcApplication::Init();
	
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());
    //这一步操作后面会讲，这里就当是实例化UserServiceRpc_Stub对象吧。UserServiceRpc_Stub是由user.proto生成的类，我们之前在user.proto中注册了Login方法，
    
    fixbug::LoginRequest request;
    request.set_name(username);
    request.set_pwd(password);
    
    fixbug::LoginResponse response;
    // callee的Login函数返回LoginResponse数据结构数据
    
    stub.Login(nullptr, &request, &response, nullptr); 
    //caller发起远端调用，将Login的参数request发过去，callee返回的结果放在response中。
	if (0 == response.result().errcode()){
		return 1;
	}
	else{
		return -1;
	}
}
string Token(string& username,int length) {
	//密钥生成器
	std::string key= generateRandomString(length);
	//创建token
	return createToken(string& username,key);
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
			string token=Token(username,18);
			printf("{\"code\":\"000\",\"token\":\"%s\"}\n", token.c_str());
		}
		else{
			printf("{\"code\":\"001\"}");
		}
		free(content);
	}
	return 0;
}

