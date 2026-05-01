#ifndef GROUP_H
#define GROUP_H
#include "groupUser.hpp"
#include<string>
#include<vector>
using namespace std;

//群组ORM类
class Group
{
public:
    Group(int groupId=-1,string groupName="",string groupDesc="")
    {
        this->groupId=groupId;
        this->groupName=groupName;
        this->groupDesc=groupDesc;
    }
    void setGroupId(int groupId) {this->groupId=groupId;}
    void setGroupName(string groupName) {this->groupName=groupName;}
    void setGroupDesc(string groupDesc) {this->groupDesc=groupDesc;}

    int getGroupId() {return this->groupId;}
    string getGroupName() {return this->groupName;}
    string getGroupDesc() {return this->groupDesc;}
    vector<GroupUser>& getGroupUsers() {return this->users;}
private:
    int groupId;
    string groupName;
    string groupDesc;
    vector<GroupUser> users;
};


#endif