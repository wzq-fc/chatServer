# ifndef GROUPUSER_H
# define GrOUPUSER_H
#include "user.hpp"
#include<string>
using namespace std;

//群聊用户，多了一个role角色信息，从User类直接继承，复用User类的其他信息
class GroupUser : public User
{
public:
    void setGroupRole(string groupRole) {this->groupRole=groupRole;}
    string getGroupRole() {return this->groupRole;}
private:
    string groupRole;
};


#endif