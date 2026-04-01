#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;

// 处理服务器ctrl+c退出时，重置user的在线状态
void resetHandler(int sig)
{
    ChatService::Instance()->reset();
    exit(0);
}
int main(int argc, char** argv)
{
    if(argc < 3)
    {
        cerr << "command invalid! example: ./ChatSerer 127.0.0.1 6000" << endl;
        exit(0);
    }
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    signal(SIGINT, resetHandler);
    EventLoop loop;
    InetAddress listenAddr(port);
    ChatServer server(&loop, listenAddr, "ChatServer");
    server.start();
    loop.loop();
    return 0;
}