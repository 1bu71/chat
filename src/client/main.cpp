#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "json.hpp"
#include "public.hpp"
#include "user.hpp"
#include "group.hpp"
using namespace std;
using json = nlohmann::json;
// 记录当前登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

void mainMenu(int clientfd);
// 显示当前登录成功用户的基本信息
void showCurrentUserDate();


// 接受线程
void readTaskHandler(int clientfd);
// 获取系统时间（聊天信息需要添加时间戳）
string getCurrentTime();

// 控制聊天页面程序
bool isMainMenuRunning = false;
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }
    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }
    // 填写client需要连接的server的信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect error" << endl;
        close(clientfd);
        exit(-1);
    }

    for (;;)
    {
        cout << "=================欢迎进入聊天系统=================" << endl;
        cout << "1.login" << endl;
        cout << "2.register" << endl;
        cout << "3.quit" << endl;
        cout << "================================================" << endl;
        cout << "choice: ";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车
        switch (choice)
        {
        case 1: // login业务
        {
            int id = 0;
            char password[50] = {0};
            cout << "id: ";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "password: ";
            cin.getline(password, 50);
            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = password;
            string request = js.dump();
            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg error" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, sizeof(buffer), 0);
                if (len == -1)
                {
                    cerr << "recv login response error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (responsejs["errno"].get<int>() == 0)
                    {
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);
                        cout << "login success, welcome " << g_currentUser.getName() << " back!" << endl;
                        // 显示当前用户的好友列表和群组列表等信息
                        if (responsejs.contains("friends"))
                        {
                            g_currentUserFriendList.clear();

                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }
                        if (responsejs.contains("groups"))
                        {
                            g_currentUserGroupList.clear();
                            vector<string> vec = responsejs["groups"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                Group group;
                                group.setId(js["groupid"].get<int>());
                                group.setName(js["groupname"]);
                                group.setDesc(js["groupdesc"]);
                                vector<string> vec2 = js["groupusers"];
                                for (string &str2 : vec2)
                                {
                                    json js = json::parse(str2);
                                    GroupUser groupUser;
                                    groupUser.setId(js["id"].get<int>());
                                    groupUser.setName(js["name"]);
                                    groupUser.setState(js["state"]);
                                    groupUser.setRole(js["role"]);
                                    group.getUsers().push_back(groupUser);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }
                        showCurrentUserDate();

                        // 显示当前用户的离线消息 个人聊天消息或者群组消息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> vec = responsejs["offlinemsg"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                if(ONE_CHAT_MSG == js["msgid"].get<int>())
                                {
                                    cout << js["time"] << "[" << js["from"] << "]" << js["name"] << "said:" << js["msg"] << endl;
                                }
                                else if(GROUP_CHAT_MSG == js["msgid"].get<int>())
                                {
                                    cout << js["time"] << "[" << js["from"] << "] in group " << js["groupid"] << " said:" << js["msg"] << endl;
                                }
                            }
                        }
                        static int readThreadNum = 0;
                        if(readThreadNum == 0)
                        {
                            // 登录成功，启动接受线程,只需要启动一次
                            thread readTask(readTaskHandler, clientfd);
                            readTask.detach();
                            readThreadNum++;
                        }

                        isMainMenuRunning = true;
                        // 进入聊天主菜单
                        mainMenu(clientfd);
                    }
                    else
                    {
                        cerr << responsejs["errmsg"] << endl;
                    }
                }
            }
        }
        break;
        case 2: // register业务
        {
            char name[50] = {0};
            char password[50] = {0};
            cout << "name: ";
            cin.getline(name, 50);
            cout << "password: ";
            cin.getline(password, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = password;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, sizeof(buffer), 0);
                if (len == -1)
                {
                    cerr << "recv reg msg error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (responsejs["errno"].get<int>() == 0)
                    {
                        cout << "register success, userid is " << responsejs["id"].get<int>() << ", do not forget it !" << endl;
                    }
                    else
                    {
                        cerr << name << "is already exist, register error!" << endl;
                    }
                }
            }
        }
        break;
        case 3:
            close(clientfd);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }
    return 0;
}

// 显示当前登录成功用户的基本信息
void showCurrentUserDate()
{
    cout << "=================login user=================" << endl;
    cout << "current login user => id: " << g_currentUser.getId() << " name: " << g_currentUser.getName() << endl;
    cout << "=================friend list=================" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "=================group list=================" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &groupUser : group.getUsers())
            {
                cout << groupUser.getId() << " " << groupUser.getName() << " " << groupUser.getState() << " " << groupUser.getRole() << endl;
            }
        }
    }
    cout << "============================================" << endl;
}

void help(int fd = 0, string str = " ");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"loginout", "退出系统,格式loginout"}};

unordered_map<string, function<void(int, string)>> commandHandlerMap =
    {
        {"help", help},
        {"chat", chat},
        {"addfriend", addfriend},
        {"creategroup", creategroup},
        {"addgroup", addgroup},
        {"groupchat", groupchat},
        {"loginout", loginout}};

// 登录成功界面
void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;
        int idx = commandbuf.find(":");
        if (idx == -1)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid command!" << endl;
            continue;
        }
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
    }
}

void help(int, string)
{
    cout << "show command list >>>" << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " :" << p.second << endl;
    }
    cout << endl;
}

void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send addfriend msg error" << request << endl;
    }
}

void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        cerr << "invalid command chat, format is chat:friendid:message" << endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["toid"] = friendid;
    js["name"] = g_currentUser.getName();
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send chat msg error -> " << request << endl;
    }
}
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr << "invalid command creategroup, format is cretegroup:groupname:groupdesc" << endl;
        return ;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr << "send creategroup msg error -> " << request << endl;
    }
}
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr << "send addgroup msg error -> " << request << endl;
    }
}
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr << "invalid command groupchat, format is groupchat:groupid:message" << endl;
        return ;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr << "send groupchat msg error" << request << endl;
    }
}
void loginout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr << "send loginout msg error" << request << endl;
    }else
        isMainMenuRunning = false;
}
// 接受线程
void readTaskHandler(int clientfd)
{
    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        int len = recv(clientfd, buffer, sizeof(buffer), 0);
        if (len == -1)
        {
            cerr << "recv msg error" << endl;
            exit(-1);
        }
        else if (len == 0)
        {
            cerr << "server closed the connection" << endl;
            close(clientfd);
            exit(0);
        }

        json js = json::parse(buffer);
        if (ONE_CHAT_MSG == js["msgid"].get<int>())
        {
            cout << js["time"].get<string>() << "[" << js["id"] << "]"
                 << js["name"].get<string>() << "said:" << js["msg"].get<string>() << endl;
            continue;
        }
        else if (GROUP_CHAT_MSG == js["msgid"].get<int>())  // 添加群消息处理
        {
            cout << js["time"].get<string>() << " group: [" << js["groupid"] << "] "
                 << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
        }
    }
}
// 获取系统时间（聊天信息需要添加时间戳）
string getCurrentTime()
{
    auto now = chrono::system_clock::now();
    time_t time = chrono::system_clock::to_time_t(now);
    string curTime = ctime(&time);
    curTime.pop_back(); // 去掉换行符
    return curTime;
}
