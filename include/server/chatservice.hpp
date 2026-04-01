#ifndef CHARSERVICE_H
#define CHARSERVICE_H

#include <unordered_map>
#include <functional>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <mutex>
#include "json.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"

using json = nlohmann::json;
using namespace muduo;
using namespace muduo::net;
using namespace std;
// 表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, const json &js, Timestamp)>;

class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService *Instance();
    // 业务方法
    void login(const TcpConnectionPtr &conn, const json &js, Timestamp time);
    void reg(const TcpConnectionPtr &conn, const json &js, Timestamp time);
    void onechat(const TcpConnectionPtr &conn, const json &js, Timestamp time);
    void addfriend(const TcpConnectionPtr &conn, const json &js, Timestamp time);
    void creategroup(const TcpConnectionPtr &conn, const json &js, Timestamp time);
    void addgroup(const TcpConnectionPtr &conn, const json &js, Timestamp time);
    void groupchat(const TcpConnectionPtr &conn, const json &js, Timestamp time);
    void loginout(const TcpConnectionPtr &conn, const json &js, Timestamp time);
    void handleRedisSubscribeMessage(int userid, string msg);
    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    // 处理服务器异常退出,重置在线用户的在线状态
    void reset();
    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);

private:
    std::unordered_map<int, MsgHandler> _msgHandlerMap;
    ChatService();

    // 存储在线用户的通信连接 需要保证线程安全
    std::unordered_map<int, TcpConnectionPtr> _userConnMap;
    std::mutex _connMutex;
    // 数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    //redis操作对象
    Redis _redis;
};

#endif