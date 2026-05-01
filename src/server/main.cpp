#include "chatServer.hpp"
#include "chatService.hpp"
#include <iostream>
#include<signal.h>
using namespace std;

//服务器ctrl+c关闭后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc,char* argv[])
{
    if(argc<3)
    {
        cerr<<"传入的参数个数不对"<<endl;
        exit(-1);
    }
    char* ip=argv[1];
    uint16_t port=atoi(argv[2]);

    signal(SIGINT,resetHandler);

    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    return 0;
}