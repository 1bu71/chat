#ifndef FRIENDMODEL_HPP
#define FRIENDMODEL_HPP
#include "user.hpp"
#include <vector>
using namespace std;

class FriendModel
{
public:
    void insert(int userid, int friendid);
    vector<User> query(int userid);
};
#endif