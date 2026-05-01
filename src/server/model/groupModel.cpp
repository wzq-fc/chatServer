#include "groupModel.hpp"
#include "db.h"

//创建群组
bool GroupModel::createGroup(Group& group)
{
    char sql[1024]={0};
    sprintf(sql,"insert into allgroup(groupname,groupdesc) values('%s','%s')",
        group.getGroupName().c_str(),group.getGroupDesc().c_str());

    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            group.setGroupId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

//加入群组
void GroupModel::addGroup(int userId,int groupId,string groupRole)
{
    char sql[1024]={0};
    sprintf(sql,"insert into groupuser values(%d,%d,'%s')",groupId,userId,groupRole.c_str());

    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}

//查询用户所在的群组信息
vector<Group> GroupModel::queryGroups(int userId)
{
    char sql[1024]={0};
    sprintf(sql,"select a.groupid,a.groupname,a.groupdesc,b.grouprole from allgroup a \
            inner join groupuser b on a.groupid=b.groupid where b.userid=%d",userId);

    MySQL mysql;
    vector<Group> groups;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                Group group;
                group.setGroupId(atoi(row[0]));
                group.setGroupName(row[1]);
                group.setGroupDesc(row[2]);
                groups.push_back(group);
            }
        }
        mysql_free_result(res);
    }
    
    //查询群组的用户信息
    for(Group& group : groups)
    {
        sprintf(sql,"select a.userid,a.name,a.state,b.grouprole from user a \
            inner join groupuser b on b.userid=a.userid where b.groupid=%d",group.getGroupId());

        MySQL mysql;
        if(mysql.connect())
        {
            MYSQL_RES* res=mysql.query(sql);
            if(res!=nullptr)
            {
                MYSQL_ROW row;
                while((row=mysql_fetch_row(res))!=nullptr)
                {
                    GroupUser user;
                    user.setId(atoi(row[0]));
                    user.setName(row[1]);
                    user.setState(row[2]);
                    user.setGroupRole(row[3]);
                    group.getGroupUsers().push_back(user);
                }
            }
            mysql_free_result(res);
        }
    }
    return groups;
}

//根据指定的goupId查询群组用户的usrId列表，除了userId自己，主要用于群聊业务给其他群成员群发消息
vector<int> GroupModel::queryGroupUsers(int userId,int groupId)
{
    char sql[1024]={0};
    sprintf(sql,"select userid from groupuser where groupid=%d and userid!=%d",groupId,userId);

    MySQL mysql;
    vector<int> groupUsers;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                groupUsers.push_back(atoi(row[0]));
            }
        }
        mysql_free_result(res);
    }
    return groupUsers;
}