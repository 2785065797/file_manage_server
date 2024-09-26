#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
class UserService : public fixbug::UserServiceRpc // 使用在rpc服务发布端（rpc服务提供者）
{
public:
    bool Login(std::string name, std::string pwd)
    {
        MYSQL *conn=mysql_init(NULL);
	    if(conn==NULL){
		    return false;
    	}
	    if(mysql_real_connect(conn,"192.168.250.26","virtual","1","cloud_disk",0,NULL,0)==NULL){
		    mysql_close(conn);
    		return false;
	    }
    	// 这里应该有代码来检查用户的凭据，例如查询数据库
	    char query[BUFSIZ];
    	sprintf(query,"select * from user where name= '%s' and password= '%s'",name,pwd);
	    if(mysql_query(conn,query)){
		    mysql_close(conn);
		    return false;
	    }

    	// 如果凭据正确，返回true，否则返回false
	    MYSQL_RES *result=mysql_store_result(conn);
    	if(result==NULL){
	    	mysql_close(conn);
		    return false;
    	}
	    if(mysql_num_rows(result)>0){
		    mysql_close(conn);
    		return true; 
	    }
	    else {	
		    mysql_close(conn);
		    return false;
	    }
    }
    void Login(::google::protobuf::RpcController* controller,
                       const ::fixbug::LoginRequest* request,
                       ::fixbug::LoginResponse* response,
                       ::google::protobuf::Closure* done)
    {
		/**** callee要继承UserServiceRpc并重写它的Login函数 ****/
        
        std::string name = request->name();
        std::string pwd = request->pwd();
        //request存着caller发来的Login函数需要的参数
        
        bool login_result = Login(name, pwd);
        //处理Login函数的逻辑，这部分逻辑单独写了一个函数。处于简化目的，就只是打印一下name和pwd。
        
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_sucess(login_result);
        //将逻辑处理结果写入到response中。
        
        done->Run();
        //将结果发送回去
    }
};

int main(int argc, char **argv)
{
    MprpcApplication::Init(argc, argv);
	//想要用rpc框架就要先初始化
    
    RpcProvider provider;
    // provider是一个rpc对象。它的作用是将UserService对象发布到rpc节点上，暂时不理解没关系！！
    
    provider.NotifyService(new UserService());
    // 将UserService服务及其中的方法Login发布出去，供远端调用。
    // 注意我们的UserService是继承自UserServiceRpc的。远端想要请求UserServiceRpc服务其实请求的就是UserService服务。而UserServiceRpc只是一个虚类而已。

    provider.Run();
	// 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    return 0;
}