#include<iostream>
using namespace std;
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "fcgi_stdio.h"
#include"fcgi_config.h"
#include<mysql.h>
#include<fastcgi.h>
#include <json.h>
#include<string>
#include<token.h>
#include<regex.h>
// 假设您有一个数据库或者用户管理类来验证用户

int authenticateUser1(int search,long *num,char *cmd) {
	MYSQL *conn=mysql_init(NULL);
	if(conn==NULL){
		return -1;
	}
	if(mysql_real_connect(conn," 192.168.250.26","virtual","1","cloud_disk",0,NULL,0)==NULL){
		mysql_close(conn);
		return -1;
	}
	// 这里应该有代码来检查用户的凭据，例如查询数据库
	char query[BUFSIZ];
	if(search==1){
		char *index=strtok(cmd,"=");
		index=strtok(NULL,"");
		sprintf(query,"SELECT count(*) FROM share_file_list WHERE filename LIKE '%%%s%%';",index);
	}
	else{
		sprintf(query,"SELECT count(*) FROM share_file_list");
	}
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
		*num=atoi(row[0]);
	}else{
		mysql_free_result(result);
		mysql_close(conn);
		return -1;
	}

	// 释放结果集内存
	mysql_free_result(result);
	mysql_close(conn);
	return 1;
}

int authenticateUser2(char* cmd,int start,int count) {
	MYSQL *conn=mysql_init(NULL);
	if(conn==NULL){
		return -1;
	}
	if(mysql_real_connect(conn," 192.168.250.26","virtual","1","cloud_disk",0,NULL,0)==NULL){
		mysql_close(conn);
		return -1;
	}
	char *index=strtok(cmd,"=");
	index=strtok(NULL,"=");
	index=strtok(NULL,"");
	char query[BUFSIZ];
	if(index!=NULL){
		sprintf(query,"SELECT share_file_list.user,share_file_list.md5,share_file_list.createtime,share_file_list.filename,share_file_list.pv,file_info.url,file_info.size,file_info.type FROM share_file_list JOIN file_info ON share_file_list.md5=file_info.md5 and share_file_list.filename=file_info.filename where share_file_list.filename Like'%%%s%%' order by share_file_list.pv desc LIMIT %d OFFSET %d;",index,count,start);
	}else{
		sprintf(query,"SELECT share_file_list.user,share_file_list.md5,share_file_list.createtime,share_file_list.filename,share_file_list.pv,file_info.url,file_info.size,file_info.type FROM share_file_list JOIN file_info ON share_file_list.md5=file_info.md5 and share_file_list.filename=file_info.filename order by share_file_list.pv desc LIMIT %d OFFSET %d;",count,start);	
	}
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
	int num_fields=mysql_num_fields(result);
	int num_rows=mysql_num_rows(result);
	if(num_rows>0){
		printf( "Content-type: application/json\r\n\r\n");
		Json::Value root;
		Json::Value filesArray(Json::arrayValue);
		Json::Value file;
		root["code"]="000";
		for(int i=0;i<num_rows;i++){
			MYSQL_ROW row=mysql_fetch_row(result);
			if(row!=NULL){
				// 发送一个包含所有键值对的JSON对象  
				file["user"]=row[0];
				file["md5"]=row[1];
				file["time"]=row[2];
				file["filename"]=row[3];
				file["share_status"]=1;
				file["pv"]=stoi(row[4]);
				file["url"]=row[5];
				file["size"]=stol(row[6]);
				file["type"]=row[7];
				filesArray.append(file);
			}
		}
		root["files"]=filesArray;
		Json::StreamWriterBuilder builder;
		std::string jsonString = Json::writeString(builder, root);
		size_t length=jsonString.length();
		char *charArray=new char[length+1];
		strcpy(charArray,jsonString.c_str());
		printf("%s",charArray);
		delete [] charArray;
	}
	mysql_free_result(result);
	mysql_close(conn);
	return 0;
}
int strcmd(const char *cmd){
	char buf[1024];
	strcpy(buf,cmd);
	char *index=strtok(buf,"=");
	if(strcmp(index,"cnt")==0) return 1;
	else return 0;
}
int main() {
	while (FCGI_Accept() >= 0) {
		// 1. 根据content-length得到post数据块的长度
		char* contentLengthStr = getenv( "CONTENT_LENGTH");
		char* cmd=getenv("QUERY_STRING");

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
		long num;
		int flag;
		int search=strcmd(cmd);
		if(search==1){
			flag=authenticateUser1(search,&num,cmd);
		}
		else{
			if (strcmp(cmd,"cmd=count")==0) {
				flag=authenticateUser1(search,&num,cmd);
			}
			else{
				Json::Value root;
				Json::Reader reader;
				if (!reader.parse(content, root)) {
					printf("Content-type: text/html\r\n\r\n");
					printf( "Error: Invalid JSON.\r\n");
					free(content);
					return -1;
				}
				const int start = root["start"].asInt();
				const int count = root["count"].asInt();
				flag=authenticateUser2(cmd,start,count);
			}
		}
		if(flag==1){
			printf( "Content-type: text/plain\r\n\r\n");
			printf("%ld",num);
		}
		else if(flag!=0){
			printf( "Content-type: application/json\r\n\r\n");
			printf("{\"code\":\"015\"}");
		}
		free(content);
	}
	return 0;
}

