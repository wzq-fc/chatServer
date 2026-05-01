#include "chatService.hpp"
#include "public.hpp"
#include<muduo/base/Logging.h>
#include<vector>
#include<string>
using namespace std;
using namespace muduo;

//获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service; 
}

//注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOG_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginOut,this,_1,_2,_3)});

    //连接redis服务器
    if(_redis.connect())
    {
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubcribeMessage,this,_1,_2));
    }
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    //记录错误日志，msgid没有对应的事件处理回调
    auto it=_msgHandlerMap.find(msgid);
    if(it==_msgHandlerMap.end())
    {
        //返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr& conn,json& js,Timestamp time){
            LOG_ERROR<<"msgid:"<<msgid<<" can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid=js["userid"].get<int>();
    string password=js["password"];

    User user=_userModel.query(userid);
    if(user.getId()!=-1&&user.getPassword()==password)
    {
        if(user.getState()=="online")
        {
            //该用户已经登录，不允许重复登录
            json response;
            response["msgid"]=LOG_MSG_ACK;
            response["errno"]=2;
            response["errmsg"]="该用户已登录，请重新输入账号";
            conn->send(response.dump());
        }
        else
        {
            //登录成功，记录用户的连接信息
            {
                lock_guard<mutex> lg(_connMutex);
                _userConnMap.insert({userid,conn});
            }

            //向redis订阅channel(userid)
            _redis.subcribe(userid);

            //登录成功,更新用户状态信息 state offline->online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"]=LOG_MSG_ACK;
            response["errno"]=0;
            response["userid"]=user.getId();
            response["name"]=user.getName();

            //查询用户是否有离线信息
            vector<string> vec=_offlineMsgModel.query(userid);
            if(!vec.empty())
            {
                response["offlinemessage"]=vec;
                //读取用户的离线消息之后，把该用户所有的离线消息删除掉
                _offlineMsgModel.remove(userid);
            }

            //查询该用户的好友信息并返回
            vector<User> userVec=_friendModel.query(userid);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(auto& user : userVec)
                {
                    json js;
                    js["userid"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"]=vec2;
            }

            //查询该用户的群聊信息并返回
            vector<Group> groupVec=_groupModel.queryGroups(userid);
            if(!groupVec.empty())
            {
                vector<string> vec3;
                for(Group& group : groupVec)
                {
                    json js;
                    js["groupid"]=group.getGroupId();
                    js["groupname"]=group.getGroupName();
                    js["groupdesc"]=group.getGroupDesc();
                    json groupUserJs;
                    vector<GroupUser> vec4=group.getGroupUsers();
                    vector<string> vec5;
                    for(GroupUser& groupUser : vec4)
                    {
                        groupUserJs["userid"]=groupUser.getId();
                        groupUserJs["name"]=groupUser.getName();
                        groupUserJs["state"]=groupUser.getState();
                        groupUserJs["grouprole"]=groupUser.getGroupRole();
                        vec5.push_back(groupUserJs.dump());
                    }
                    js["groupusers"]=vec5;
                    vec3.push_back(js.dump());
                }
                response["groups"]=vec3;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        //用户名或密码输入错误
        json response;
        response["msgid"]=LOG_MSG_ACK;
        response["errno"]=1;
        response["errmsg"]="用户名或密码输入错误";
        conn->send(response.dump());
    }
}

// 处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name=js["name"];
    string password=js["password"];
    User user;
    user.setName(name);
    user.setPassword(password);
    if(_userModel.insert(user))
    {
        //注册成功
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=0;
        response["id"]=user.getId();
        conn->send(response.dump());
    }
    else
    {
        //注册失败
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=1;
        conn->send(response.dump());
    }
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    User user;
    {
        lock_guard<mutex> lg(_connMutex);
        for(auto it=_userConnMap.begin();it!=_userConnMap.end();it++)
        {
            if(it->second==conn)
            {
                //从map表删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    //取消在redis中订阅channel(userid)
    _redis.unsubcribe(user.getId());
    //更新用户的状态信息
    user.setState("offline");
    _userModel.updateState(user);
}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int toid=js["to"].get<int>();
    {
        lock_guard<mutex> lg(_connMutex);
        auto it=_userConnMap.find(toid);
        if(it!=_userConnMap.end())
        {
            //toid在本服务器上在线，转发消息
            it->second->send(js.dump());
            return;
        }
    }

    //查询toid用户是否为在线状态
    User user=_userModel.query(toid);
    if(user.getState()=="online")
    {
        _redis.publish(toid,js.dump());
        return;
    }

    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid,js.dump());
}

//服务器异常，业务重置方法
void ChatService::reset()
{
    //把online状态的用户，设置成offline
    _userModel.resetState();
}

//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["userid"].get<int>();
    int friendid=js["friendid"].get<int>();

    //存储好友信息
    _friendModel.insert(userid,friendid);
    _friendModel.insert(friendid,userid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["userid"].get<int>();
    string groupname=js["groupname"];
    string groupdesc=js["groupdesc"];
    //存储新创建的群组信息
    Group group(-1,groupname,groupdesc);
    if(_groupModel.createGroup(group))
    {
        //存储群组的创建人信息
        _groupModel.addGroup(userid,group.getGroupId(),"creator");
    }
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["userid"].get<int>();
    int groupid=js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["from"].get<int>();
    int groupid=js["groupid"].get<int>();
    vector<int> otherGroupUsers=_groupModel.queryGroupUsers(userid,groupid);
    lock_guard<mutex> lg(_connMutex);
    for(int id : otherGroupUsers)
    {
        auto it=_userConnMap.find(id);
        if(it!=_userConnMap.end())
        {
            //该用户在本服务器上在线，直接给他发送消息
            it->second->send(js.dump());
            continue;
        }

        //查询该用户是否为在线状态
        User user=_userModel.query(id);
        if(user.getState()=="online")
        {
            _redis.publish(id,js.dump());
            continue;
        }
        //该用户不在线，存储离线消息
        _offlineMsgModel.insert(id,js.dump());
    }
}

//退出登录业务
void ChatService::loginOut(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["userid"].get<int>();

    {
        lock_guard<mutex> lg(_connMutex);
        auto it=_userConnMap.find(userid);
        if(it!=_userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }
    
    //在redis中取消订阅通道
    _redis.unsubcribe(userid);

    //更新用户状态
    User user(userid,"","","offline");
    _userModel.updateState(user);
}

//从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubcribeMessage(int userid,string message)
{
    lock_guard<mutex> lg(_connMutex);
    auto it=_userConnMap.find(userid);
    if(it!=_userConnMap.end())
    {
        it->second->send(message);
        return;
    }
    //存储离线消息
    _offlineMsgModel.insert(userid,message);
}
