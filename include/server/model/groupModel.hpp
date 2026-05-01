#ifndef GROUPMODEL_H
#define GROUPMODEL_H
#include "group.hpp"

//维护群组信息的操作接口方法
class GroupModel
{
public:
    //创建群组
    bool createGroup(Group& group);
    //加入群组
    void addGroup(int useId,int groupId,string role);
    //查询用户所在的群组信息
    vector<Group> queryGroups(int userId);
    //根据指定的groupId查询群组用户的userId列表，除了userId自己，主要用于群聊业务给其他群成员群发消息
    vector<int> queryGroupUsers(int userId,int groupId);
};


#endif