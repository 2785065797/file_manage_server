#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <json.h>
#include <mysql.h>
#include <mysqld_error.h>
#include "fcgi_stdio.h"

//std::string CtoS(const char* ch, MYSQL* conn) {
//	std::string str;
//	str.resize(strlen(ch) * 2 + 1);
//	if (mysql_real_escape_string(conn, &str[0], ch, strlen(ch)) != 0) {
//		str.clear(); // 清理可能因为错误而设置的空字符串
//	}
//	return str;
//}

int authenticateUser(const char* username, const char* nickname, const char* password, const char* phone, const char* email) {
	MYSQL* conn = mysql_init(NULL);
	if (conn == NULL) {
		return -1; // 分配内存失败
	}
	if (mysql_real_connect(conn, " 192.168.250.26", "virtual", "1", "cloud_disk", 0, NULL, 0) == NULL) {
		mysql_close(conn);
		return -1; // 连接数据库失败
	}

	// 这里应该有代码来检查用户的凭据，例如查询数据库
	//std::string query = "INSERT INTO user (id, name, nickname, password, phone, createtime, email) VALUES (NULL, '"+
	//	CtoS(username, conn)+"','"+
	//	CtoS(nickname, conn)+"','"+
	//	CtoS(password, conn)+"','"+
	//	CtoS(phone,conn)+"', CURRENT_TIMESTAMP, '"+
	//	CtoS(email, conn)+"')";
	char query[BUFSIZ];
	sprintf(query, "INSERT INTO user(id, name, nickname, password, phone, createtime, email) VALUES(NULL, '%s','%s','%s','%s',CURRENT_TIMESTAMP,'%s')", username,nickname,password,phone,email);
	if (mysql_query(conn, query)) {
		unsigned int error_code = mysql_errno(conn);
		if (error_code == ER_DUP_ENTRY) {
			mysql_close(conn);
			return 0; // 用户名或昵称已存在
		}
		else {
			mysql_close(conn);
			return -1; // 插入数据失败
		}
	}
	sprintf(query, "INSERT INTO user_file_count(user,count) VALUES('%s',0)", username);
        if (mysql_query(conn, query)) {
                unsigned int error_code = mysql_errno(conn);
                if (error_code == ER_DUP_ENTRY) {
                        mysql_close(conn);
                        return 0; // 用户名或昵称已存在
                }
                else {
                        mysql_close(conn);
                        return -1; // 插入数据失败
                }
        }
	mysql_close(conn);
	return 1; // 插入数据成功
}

int main() {
	while (FCGI_Accept() >= 0) {
		char* contentLengthStr = getenv("CONTENT_LENGTH");
		if (contentLengthStr == NULL) {
			printf("Content-type: text/html\r\n\r\n");
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
		fread(content, 1, contentLength, stdin);
		content[contentLength] = '\0';

		// 解析JSON对象
		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(content, root)) {
			printf("Content-type: application/json\r\n\r\n");
			printf("{\"code\":\"001\"}");
			free(content);
			continue; // 跳过当前请求，继续接受下一个请求
		}
		const char* username = root["userName"].asCString();
		const char* nickname = root["nickName"].asCString();
		const char* password = root["firstPwd"].asCString();
		const char* phone = root["phone"].asCString();
		const char* email = root["email"].asCString();

		// 用户验证和数据插入逻辑
		int flag = authenticateUser(username, nickname, password, phone, email);
		printf("{\"code\":\"002\"}");
		switch (flag) {
			case 1:
				// 成功
				printf("{\"code\":\"002\"}");
				break;
			case 0:
				// 用户名或昵称已存在
				printf("{\"code\":\"003\"}");
				break;
			case -1:
				// 错误
				printf("{\"code\":\"004\"}");
				break;
		}
		free(content);
	}
	return 0;
}

