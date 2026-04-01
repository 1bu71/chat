#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis()
{
    _publish_context = nullptr;
    _subscribe_context = nullptr;
}

Redis::~Redis()
{
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }
    if (_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}
// 连接redis服务器
bool Redis::connect()
{
    _publish_context = redisConnect("127.0.0.1", 6379);
    if(_publish_context == nullptr)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if(_subscribe_context == nullptr)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    //在单独的线程中，监听通道上的时间，有消息给业务层上报  因为subscribe是一个阻塞的操作，所以要在单独的线程中监听通道上的事件
    thread t([&](){
        observer_channel_message();
    });
    t.detach();
    cout << "connect redis-server success!" << endl;
    return true;
}
// 发布消息
bool Redis::publish(int channal, string message)
{
    redisReply* reply = (redisReply*)redisCommand(_publish_context, "PUBLISH %d %s", channal, message.c_str());
    if(reply == nullptr)
    {
        cerr << "publish failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}
// 订阅消息
bool Redis::subscribe(int channal)
{
    //subsrribe命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接受通道消息
    //所以在单独的线程中监听通道上的事件
    //只负责发送命令，不阻塞接受redis server消息，否则和notifyMsg线程抢占资源
    if(REDIS_ERR == redisAppendCommand(_subscribe_context, "SUBSCRIBE %d", channal))
    {
        cerr << "subscribe failed!" <<endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区，知道缓冲区数据发送完毕
    int done = 0;
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "subscribe failed!" << endl;
            return false;
        }
    }
    //redisGetReply函数会阻塞等待服务器返回消息，收到消息后会自动将订阅的消息上报给业务层
    return true;
}
// 取消订阅
bool Redis::unsubscribe(int channal)
{
    if(REDIS_ERR == redisAppendCommand(_subscribe_context, "UNSUBSCRIBE %d", channal))
    {
        cerr << "unsubscribe failed!" << endl;
        return false;
    }
    int done = 0;
    while (!done)
    {
        if(REDIS_ERR ==redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "unsubscribe failed!" << endl;
            return false;
        }
    }
    
    return true;
}
// 在独立线程中接受订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply* reply = nullptr;
    while(REDIS_OK == redisGetReply(this->_subscribe_context, (void**)&reply))
    {
        //订收到的消息是一个带三元组的数组
        if(reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str); //element[1]是通道号，element[2]是消息内容
        }
    }
    freeReplyObject(reply);
    cerr << ">>>>>>>>>>>>>>>> observer_channel_message <<<<<<<<<<<<<<<<<<<<" << endl;
}
// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;
}
