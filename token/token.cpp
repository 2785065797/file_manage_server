#include <iostream>
#include <jwt-cpp/jwt.h>
#include <random>  
#include <string>  
#include <ctime>  
#include <hiredis/hiredis.h>
#include "token.h"
using namespace std;

//token加入redis
int token::redis(string& username,string& token,int expire_seconds){
	redisContext *c = redisConnect("127.0.0.1", 6379);
	if (c == NULL || c->err) {
		if (c) {
			redisFree(c);
		}
		return -1;
	}
	redisReply *reply;	
	reply = static_cast<redisReply*>(redisCommand(c, "SET %s %s",username.c_str(),token.c_str()));
	if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
		freeReplyObject(reply);
		redisFree(c);
		return -1;
	}
    freeReplyObject(reply);
    reply = static_cast<redisReply*>(redisCommand(c,"expire %s %d",username.c_str(),expire_seconds));
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
		freeReplyObject(reply);
		redisFree(c);
		return -1;
	}
	freeReplyObject(reply);
	redisFree(c);
	return 0;
}

//token从redis中取出
int token::get_redis(string& username,string& token){
	redisContext *c = redisConnect("127.0.0.1", 6379);
	if (c == NULL || c->err) {
		if (c) {
			redisFree(c);
		}
		return -1;
	}
	redisReply *reply;	
	reply = static_cast<redisReply*>(redisCommand(c, "GET %s",username.c_str()));
	if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
		freeReplyObject(reply);
		redisFree(c);
		return -1;
	}
    if (reply->type == REDIS_REPLY_STRING) {  
        token = std::string(reply->str, reply->len); // 设置 token  
        freeReplyObject(reply); // 释放 reply 对象  
        redisFree(c); // 断开 Redis 连接  
        return 1; // 找到 token  
    } else {  
        freeReplyObject(reply); // 即使没有找到 token，也要释放 reply 对象  
        redisFree(c); // 断开 Redis 连接  
        return 0; // 没有找到 token  
    } 
}

//验证token
int token::decodeToken(string& token,string& username,string key){
    // 验证token
    try {
        auto decoded=jwt::decode(token);
        auto verifier = jwt::verify()
            .with_issuer("yyj") // 设置期望的发行者
            .with_subject(username)     // 设置期望的主题
            .allow_algorithm(jwt::algorithm::hs256{key}); // 允许的算法和密钥
        verifier.verify(decoded); // 验证token

    } catch (const std::exception& e) {
        return 0;
    }
    return 1;
}

//创建token
std::string token::createToken(string& username,string key,int expire_seconds){
    // 创建一个JWT token
    auto token = jwt::create()
        .set_type("JWT")
        .set_issuer("yyj") // 设置发行者
        .set_subject(username)     // 设置主题（通常是用户名）
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{expire_seconds})
        .sign(jwt::algorithm::hs256{key}); // 使用HS256算法和密钥签名

    return token;
}

//密钥生成器
std::string token:: generateRandomString(size_t length) {  
    static const std::string chars =   
        "0123456789"  
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  
        "abcdefghijklmnopqrstuvwxyz";  
    std::random_device rd;  // 真正的随机数生成器  
    std::mt19937 gen(rd()); // 以random_device作为种子的Mersenne Twister生成器  
    std::uniform_int_distribution<> dis(0, chars.size() - 1);  
  
    std::string str(length, ' ');  
    for (size_t i = 0; i < length; ++i) {  
        str[i] = chars[dis(gen)];  
    }  
    return str;  
}  
