#include "groupmodel.hpp"
#include "db.h"
#include <iostream>
using namespace std;
// 创建群组
bool GroupModel::createGroup(Group &group)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s', '%s')", group.getName().c_str(), group.getDesc().c_str());
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}
// 加入群组
void GroupModel::addGroup(int userid, int groupid, string role)
{   
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d, %d, '%s')", groupid, userid, role.c_str());
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}
// 查询用户所在群组
vector<Group> GroupModel::queryGroups(int userid)
{
    /*
    1.先根据userid在groupuser表中查询出该用户所属的群组id和在群组中的角色
    2.再根据群组id查询出群组的详细信息
    */
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.groupname, a.groupdesc from allgroup a inner join groupuser b on a.id = b.groupid where b.userid = %d", userid);
    MySQL mysql;
    vector<Group> groupVec;
    if(mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }
    //查询出组员的信息 group有vector<GroupUser> users;成员变量
    for(Group &group : groupVec)
    {
        sprintf(sql,"select a.id, a.name, a.state, b.grouprole from user a inner join groupuser b on a.id = b.userid where b.groupid = %d", group.getId());
        MySQL mysql;
        if(mysql.connect())
        {
            MYSQL_RES *res = mysql.query(sql);
            if(res != nullptr)
            {
                MYSQL_ROW row;
                while((row = mysql_fetch_row(res)) != nullptr)
                {
                    GroupUser user;
                    user.setId(atoi(row[0]));
                    user.setName(row[1]);
                    user.setState(row[2]);
                    user.setRole(row[3]);
                    group.getUsers().push_back(user); 
                }
                mysql_free_result(res);
            }
        }
    }
    return groupVec;
}
// 根据指定的groupid查询群组用户的id列表，除了userid自己，主要用户群聊业务给其他成员发消息
vector<int> GroupModel::queryGroupsUsers(int userid, int groupid)
{
    vector<int> idVec;
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", groupid, userid);
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                idVec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return idVec;
}