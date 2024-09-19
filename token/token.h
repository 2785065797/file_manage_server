#ifndef _TOKEN_H_
#define _TOKEN_H_

#include <iostream>
#include <string>  
using std::string;
//token加入redis

class token{
	public:
		int redis(string& username,string& token,int expire_seconds=3*3600);
		//token从redis中取出
		int get_redis(string& username,string& token);
		//验证token
		int decodeToken(string& token,string& username,string key="1a4s7d2f5g8h3j6k9l");
		//创建token
		std::string createToken(string& username,string key="1a4s7d2f5g8h3j6k9l",int expire_seconds=3*3600);

		//密钥生成器
		std::string generateRandomString(size_t length);
};
#endif
