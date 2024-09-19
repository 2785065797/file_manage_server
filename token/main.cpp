#include <iostream>
#include <string>  
#include "token.h"
using namespace std;

int main() {
    //生成token
    token t;
    std::string username = "史宇";
    int expire_seconds=3*3600;
    std::string key = t.generateRandomString(32); // 生成32个字符的随机字符串作为密钥
    string token=t.createToken(username,key,expire_seconds);
    std::cout<<"token="<<token<<endl;

    //直接解密验证token
    if(t.decodeToken(token,username,key)==1){
        cout<<"token验证成功"<<endl;
    }
    else{
        cout<<"token验证失败"<<endl;
    }

    //token加入redis
    if(t.redis(username,token,expire_seconds)==0){
        cout<<"token成功添加到redis"<<endl;
    }
    else{
        cout<<"token添加到redis失败"<<endl;
    }

    //token从redis中取出
    string decode_token;
    int flag=t.get_redis(username,decode_token);
    if((flag==1)&&decode_token==token){
        cout<<"redis认证token成功"<<endl;
    }
    else if(flag==0){
        cout<<"redis认证token过期"<<endl;
    }
    else if(flag==1&&decode_token!=token){
        cout<<"redis认证token身份异常"<<endl;
    }
    else{
        cout<<"redis认证出错"<<endl;
    }

    return 0;
}

