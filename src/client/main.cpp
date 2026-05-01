#include<iostream>
#include<thread>
#include<chrono>
#include<ctime>
#include<unistd.h>
#include<vector>
#include<string>
#include<cstring>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unordered_map>
#include<functional>
#include "json.hpp"
#include "user.hpp"
#include "group.hpp"
#include "public.hpp"
using json=nlohmann::json;
using namespace std;

//记录当前系统登陆的用户信息
User g_currentUser;
//记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
//记录当前用户的群组列表信息
vector<Group> g_currentUserGroupList;
//记录是否在线，用来决定当前的客户端界面
bool isOnline=false;

//显示当前登录成功的用户的基本信息
void showCurrentUserInfo();
//接收线程
void readTaskHandler(int clientfd);
//获取系统时间
string getCurrentTime();
//主页面聊天程序
void mainMenu(int clientfd);


int main(int argc,char* argv[])
{
    if(argc<3)
    {
        cerr<<"输入的参数个数错误"<<endl;
        exit(-1);
    }
    
    char* ip=argv[1];
    uint16_t port=atoi(argv[2]);

    //创建client端的socket
    int clientfd=socket(AF_INET,SOCK_STREAM,0);
    if(clientfd==-1)
    {
        cerr<<"客户端socket创建失败"<<endl;
        exit(-1);
    }

    //绑定客户端的ip和端口号
    sockaddr_in sockAddr;
    memset(&sockAddr,0,sizeof(sockaddr_in));
    sockAddr.sin_family=AF_INET;
    sockAddr.sin_port=htons(port);
    sockAddr.sin_addr.s_addr=inet_addr(ip);

    //client与server建立连接
    if(connect(clientfd,(sockaddr*)&sockAddr,sizeof(sockaddr_in))==-1)
    {
        cerr<<"client与server建立连接失败"<<endl;
        exit(-1);
    }

    //main线程用于接收用户输入，负责发送数据
    while(true)
    {
        //显示首页面菜单 登录、注册、退出
        cout<<"==========================="<<endl;
        cout<<"1. 登录"<<endl;
        cout<<"2. 注册"<<endl;
        cout<<"3. 退出"<<endl;
        cout<<"==========================="<<endl;
        cout<<"choice: ";
        int choice=0;
        cin>>choice;
        cin.get();
        switch(choice)
        {
            case 1:
            {
                //登录业务
                int userId=0;
                char password[50]={0};
                cout<<"账号：";
                cin>>userId;
                cin.get();
                cout<<"密码：";
                cin.getline(password,50);

                json js;
                js["msgid"]=LOG_MSG;
                js["userid"]=userId;
                js["password"]=password;
                string request=js.dump();

                int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len==-1)
                {
                    cerr<<"发送数据失败"<<endl;   
                }
                else
                {
                    char buffer[1024]={0};
                    len=recv(clientfd,buffer,1024,0);
                    if(len==-1)
                    {
                        cerr<<"接收数据失败"<<endl;
                    }
                    else
                    {
                        json response=json::parse(buffer);
                        if(response["errno"].get<int>()!=0)
                        {
                            cerr<<response["errmsg"]<<endl;
                        }
                        else
                        {
                            //登录成功,记录当前用户的userid和name
                            g_currentUser.setId(response["userid"].get<int>());
                            g_currentUser.setName(response["name"]);

                            //记录当前用户的好友列表信息
                            if(response.contains("friends"))
                            {
                                vector<string> friends=response["friends"];
                                for(string& str : friends)
                                {
                                    js=json::parse(str);
                                    User user;
                                    user.setId(js["userid"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }
                            //记录当前用户的群组列表信息
                            if(response.contains("groups"))
                            {
                                vector<string> groups=response["groups"];
                                for(string& str : groups)
                                {
                                    js=json::parse(str);
                                    Group group;
                                    group.setGroupId(js["groupid"].get<int>());
                                    group.setGroupName(js["groupname"]);
                                    group.setGroupDesc(js["groupdesc"]);
                                    vector<string> groupUsers=js["groupusers"];
                                    for(string& str : groupUsers)
                                    {
                                        js=json::parse(str);
                                        GroupUser groupUser;
                                        groupUser.setId(js["userid"].get<int>());
                                        groupUser.setName(js["name"]);
                                        groupUser.setState(js["state"]);
                                        groupUser.setGroupRole(js["grouprole"]);
                                        group.getGroupUsers().push_back(groupUser);
                                    }
                                    g_currentUserGroupList.push_back(group);
                                }
                            }
                            //显示用户的基本信息
                            showCurrentUserInfo();

                            //显示当前用户的离线消息
                            if(response.contains("offlinemessage"))
                            {
                                vector<string> offlineMessage=response["offlinemessage"];
                                for(string& str : offlineMessage)
                                {
                                    js=json::parse(str);
                                    int messageType=js["msgid"].get<int>();
                                    if(messageType==ONE_CHAT_MSG)
                                    {
                                        cout<<js["time"].get<string>()<<js["name"].get<string>()
                                            <<": "<<js["message"].get<string>()<<endl;
                                    }
                                    else if(messageType==GROUP_CHAT_MSG)
                                    {
                                        cout<<" ["<<js["groupname"]<<"] : "<<js["time"].get<string>()
                                            <<js["name"].get<string>()<<": "<<js["message"].get<string>()<<endl;
                                    }
                                }
                            }
                            //登录成功，启动接收线程负责接收数据,该线程最多只有一个
                            static int threadNum=0;
                            if(threadNum==0)
                            {
                                std:: thread readTask(readTaskHandler,clientfd);
                                readTask.detach();
                                threadNum++;
                            }

                            //进入聊天主界面
                            isOnline=true;
                            mainMenu(clientfd);
                        }
                    }
                }
                break;
            }
            case 2:
            {
                //注册业务
                char name[50]={0};
                char password[50]={0};
                cout<<"用户名：";
                cin.getline(name,50);
                cout<<"密码：";
                cin.getline(password,50);

                json js;
                js["msgid"]=REG_MSG;
                js["name"]=name;
                js["password"]=password;
                string request=js.dump();
                int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len==-1)
                {
                    cerr<<"发送数据失败"<<endl;
                }
                else
                {
                    char buffer[1024]={0};
                    len=recv(clientfd,buffer,1024,0);
                    if(len==-1)
                    {
                        cerr<<"接收数据失败"<<endl;
                    }
                    else
                    {
                        js=json::parse(buffer);
                        if(js["errno"].get<int>()==1)
                        {
                            cerr<<"注册失败"<<endl;
                        }
                        else
                        {
                            //注册成功
                            cout<<"注册成功!"<<endl;
                            cout<<"您的账号: "<<js["id"]<<endl;
                        }
                    }
                }
                break;
            }
            case 3:
            {
                //退出业务
                close(clientfd);
                exit(0);
            }
            default:
                cerr<<"输入错误，请重新输入!"<<endl;
        }
    }
    return 0;
}

//接收线程
void readTaskHandler(int clientfd)
{
    while(true)
    {
        char buffer[1024]={0};
        int len=recv(clientfd,buffer,1024,0);
        if(len==0||len==-1)
        {
            close(clientfd);
            exit(-1);
        }

        json js=json::parse(buffer);
        int messageType=js["msgid"].get<int>();
        if(messageType==ONE_CHAT_MSG)
        {
            cout<<js["time"].get<string>()<<" ["<<js["userid"]<<"] "
                <<js["name"].get<string>()<<": "<<js["message"].get<string>()<<endl;
        }
        else if(messageType==GROUP_CHAT_MSG)
        {
            cout<<" ["<<js["groupname"]<<"] : "<<js["time"].get<string>()<<" ["<<js["userid"]<<"] "
                <<js["name"].get<string>()<<": "<<js["message"].get<string>()<<endl;
        }
    }
}
void help(int,string);
void chat(int,string);
void addfriend(int,string);
void creategroup(int,string);
void addgroup(int,string);
void groupchat(int,string);
void loginout(int,string);

//系统支持的客户端命令列表
unordered_map<string,string> commandMap={
    {"help","显示所有支持的命令,格式help"},
    {"chat","一对一聊天,格式chat:friendid:message"},
    {"addfriend","添加好友,格式addfriend:friendid"},
    {"creategroup","创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup","加入群组,格式addgroup:groupid"},
    {"groupchat","群聊,格式groupchat:groupid:message"},
    {"loginout","退出登录,格式loginout"}
};

//注册系统支持的客户端命令处理
unordered_map<string,function<void(int,string)>> commandHandlerMap={
    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"loginout",loginout}
};

void help(int,string)
{
    cout<<"菜单>>>"<<endl;
    for(auto& p : commandMap)
    {
        cout<<p.first<<" : "<<p.second<<endl;
    }
    cout<<endl;
}

void addfriend(int clientfd,string str)
{
    int friendid=atoi(str.c_str());
    json js;
    js["msgid"]=ADD_FRIEND_MSG;
    js["userid"]=g_currentUser.getId();
    js["friendid"]=friendid;
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cout<<"您输入的addfriend格式错误!"<<endl;
    }
}

void chat(int clientfd,string str)
{
    int idx=str.find(":");
    if(idx==-1)
    {
        cerr<<"您输入的chat格式错误!"<<endl;
        return;
    }
    int friendid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);
    json js;
    js["msgid"]=ONE_CHAT_MSG;
    js["to"]=friendid;
    js["time"]=getCurrentTime();
    js["userid"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["message"]=message;
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"数据发送失败"<<endl;
    }
}

void creategroup(int clientfd,string str)
{
    int idx=str.find(":");
    if(idx==-1)
    {
        cerr<<"您输入的creategroup格式错误!"<<endl;
        return;
    }
    string groupname=str.substr(0,idx);
    string groupdesc=str.substr(idx+1,str.size()-idx);
    json js;
    js["msgid"]=CREATE_GROUP_MSG;
    js["userid"]=g_currentUser.getId();
    js["groupname"]=groupname;
    js["groupdesc"]=groupdesc;
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"创建失败"<<endl;
    }
}

void addgroup(int clientfd,string str)
{
    json js;
    js["msgid"]=ADD_GROUP_MSG;
    js["userid"]=g_currentUser.getId();
    js["groupid"]=atoi(str.c_str());
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"添加失败"<<endl;
    }
}

void groupchat(int clientfd,string str)
{
    int idx=str.find(":");
    if(idx==-1)
    {
        cerr<<"您输入的groupchat格式错误!"<<endl;
        return;
    }
    int groupid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);
    json js;
    js["msgid"]=GROUP_CHAT_MSG;
    js["from"]=g_currentUser.getId();
    js["groupid"]=groupid;
    js["time"]=getCurrentTime();
    js["name"]=g_currentUser.getName();
    js["message"]=message;
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"数据发送失败"<<endl;
    }
}

void loginout(int clientfd,string)
{
    json js;
    js["msgid"]=LOGINOUT_MSG;
    js["userid"]=g_currentUser.getId();
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cout<<"退出登录失败!"<<endl;
    }
    isOnline=false;
    exit(0);
}

//主页面聊天程序
void mainMenu(int clientfd)
{
    help(0,"");
    char buffer[1024]={0};
    while(isOnline)
    {
        cin.getline(buffer,1024);
        string commandBuffer(buffer);
        string command;
        int idx=commandBuffer.find(":");
        if(idx==-1)
        {
            command=commandBuffer;
        }
        else
        {
            command=commandBuffer.substr(0,idx);
        }
        auto it=commandHandlerMap.find(command);
        if(it==commandHandlerMap.end())
        {
            cerr<<"输入错误，请重新输入!"<<endl;
            continue;
        }
        else
        {
            it->second(clientfd,commandBuffer.substr(idx+1,commandBuffer.size()-idx));
        }
    }
}

//获取系统时间
string getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

//显示当前登录成功的用户的基本信息
void showCurrentUserInfo()
{
    cout<<"=================主页================="<<endl;
    cout<<"用户名： "<<g_currentUser.getName()<<endl;
    cout<<"账号: "<<g_currentUser.getId()<<endl;
    cout<<"=============好友列表============="<<endl;
    if(!g_currentUserFriendList.empty())
    {
        for(User& user : g_currentUserFriendList)
        {
            cout<<"---"<<user.getName()<<"---"<<user.getState()<<"---"<<endl;
        }
    }
    cout<<"=============群聊列表============="<<endl;
    if(!g_currentUserGroupList.empty())
    {
        for(Group& group : g_currentUserGroupList)
        {
            cout<<"---"<<group.getGroupName()<<"---"<<endl;
            for(GroupUser& groupUser : group.getGroupUsers())
            {
                cout<<groupUser.getName()<<" "<<groupUser.getGroupRole()<<endl;
            }
        }
    }
    cout<<"=============离线消息============="<<endl;
    cout<<"======================================"<<endl;
}