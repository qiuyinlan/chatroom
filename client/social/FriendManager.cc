#include "FriendManager.h"
#include "../utils/proto.h"
#include "../utils/IO.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
using namespace std;

FriendManager::FriendManager(int fd, User user) : fd(fd), user(std::move(user)) {}

// 迁移自Telegram.cc
void FriendManager::listFriends(vector<pair<string, User>> &my_friends) {
    string temp;
    if (my_friends.empty()) {
        cout << "您当前没有好友捏... 请按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    sendMsg(fd, LIST_FRIENDS);
    string is_online;
    cout << user.getUsername() << "的好友列表" << endl;
    cout << "-----------------------------------------" << endl;
    //发送好友数量
    sendMsg(fd, to_string(my_friends.size()));
    for (int i = 0; i < my_friends.size(); ++i) {
        //循环发送好友的UID来查询信息
        sendMsg(fd, my_friends[i].second.getUID());
        //接收好友是否在线的信息
        recvMsg(fd, is_online);
        if (is_online == "1") {
            cout << GREEN << i + 1 << ". " << my_friends[i].second.getUsername() << RESET << endl;
        } else {
            cout << i + 1 << ". " << my_friends[i].second.getUsername() << endl;
        }
    }
    cout << "-----------------------------------------" << endl;
    cout << "按任意键退出" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void FriendManager::addFriend(vector<pair<string, User>> &my_friends) const {
    sendMsg(fd, ADD_FRIEND);
    string username;
    while (true) {
        cout << "请输入你要添加好友的用户名：" << endl;
        getline(cin, username);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        if (username.find(' ') != string::npos) {
            cout << "用户名不允许出现空格" << endl;
            continue;
        }
        sendMsg(fd, username);
        string temp;
        recvMsg(fd, temp);
        if (temp == "-1") {
            cout << "该用户不存在" << endl;
            return;
        } else if (temp == "-2") {
            cout << "该用户已经是你的好友，无法多次添加" << endl;
            return;
        } else if (temp == "-3") {
            cout << "你不能添加自己为好友！" << endl;
            return;
        }
        break;
    }
    User her;
    string user_info;
    recvMsg(fd, user_info);
    her.json_parse(user_info);
    cout << "您已成功发出好友申请，等待" << her.getUsername() << "的同意" << endl;
}

void FriendManager::findRequest(vector<pair<string, User>> &my_friends) const {
    sendMsg(fd, FIND_REQUEST);
    string nums;
    recvMsg(fd, nums);
    int num = stoi(nums);
    if (num == 0) {
        string temp;
        cout << "目前没有好友申请，按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << "你收到" << num << "条好友申请" << endl;
    string friendRequestName;
    for (int i = 0; i < num; i++) {
        recvMsg(fd, friendRequestName);
        cout << "收到" << friendRequestName << "的好友申请 [1]同意 [0]拒绝" << endl;
        int choice;
        while (!(cin >> choice) || (choice != 0 && choice != 1)) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        string reply;
        if (choice == 0) {
            reply = "REFUSED";
        } else {
            reply = "ACCEPTED";
        }
        sendMsg(fd, reply);
        string request_info;
        if (choice == 1) {
            cout << "好友添加成功" << endl;
            User newFriend;
            recvMsg(fd, request_info);
            newFriend.json_parse(request_info);
            my_friends.emplace_back(request_info, newFriend);
        } else {
            cout << "您拒绝了" << friendRequestName << "的请求" << endl;
        }
    }
}

void FriendManager::delFriend(vector<pair<string, User>> &my_friends) {
    string temp;
    if (my_friends.empty()) {
        cout << "您当前没有好友捏... 请按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << "     " << user.getUsername() << "的好友列表" << endl;
    cout << "----------------------------------------" << endl;
    for (int i = 0; i < my_friends.size(); ++i) {
        cout << i + 1 << "." << my_friends[i].second.getUsername() << endl;
    }
    cout << "----------------------------------------" << endl;
    while (true) {
        cout << "请输入要删除的好友" << endl;
        string del;
        getline(cin, del);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        int who;
        while (!(cin >> who) || who < 0 || who > my_friends.size()) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        //向服务器发送删除好友的信号
        sendMsg(fd, DEL_FRIEND);
        //发送删除好友的UID
        who--;
        sendMsg(fd, my_friends[who].second.getUID());
    }
}

void FriendManager::blockedLists(vector<pair<string, User>> &my_friends) const {
    string temp;
    if (my_friends.empty()) {
        cout << "您当前没有好友捏... 请按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << "好友列表" << endl;
    cout << "-----------------------------------------" << endl;
    for (int i = 0; i < my_friends.size(); ++i) {
        cout << i + 1 << ": " << my_friends[i].second.getUsername() << endl;
    }
    cout << "-----------------------------------------" << endl;
    cout << "请输入你要屏蔽的好友的序号" << endl;
    int who;
    while (!(cin >> who) || who < 0 || who > my_friends.size()) {
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        cout << "输入格式错误" << endl;
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');
    sendMsg(fd, BLOCKED_LISTS);
    who--;
    sendMsg(fd, my_friends[who].second.getUID());
    cout << "您已成功屏蔽" << my_friends[who].second.getUsername() << ", 按任意键退出" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void FriendManager::unblocked(vector<pair<string, User>> &my_friends) const {
    sendMsg(fd, UNBLOCKED);
    cout << "已屏蔽好友" << endl;
    string nums;
    recvMsg(fd, nums);
    int num = stoi(nums);
    if (num == 0) {
        string temp;
        cout << "您的屏蔽列表为空" << endl;
        cout << "请按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    string blocked_info;
    User blocked_user;
    vector<User> blocked_users;
    for (int i = 0; i < num; i++) {
        //循环接收屏蔽名单信息
        recvMsg(fd, blocked_info);
        blocked_user.json_parse(blocked_info);
        blocked_users.push_back(blocked_user);
        cout << i + 1 << ". " << blocked_user.getUsername() << endl;
    }
    cout << "请输入你要解除屏蔽的好友序号" << endl;
    int who;
    while (!(cin >> who) || who < 0 || who > num) {
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        cout << "输入格式错误" << endl;
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');
    //向服务器发送解除屏蔽的UID
    who--;
    sendMsg(fd, blocked_users[who].getUID());
    cout << "您已经成功解除了对" << blocked_users[who].getUsername() << "的屏蔽，请按任意键退出" << endl;
    getline(cin, blocked_info);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
} 