#include "OperationMenu.h"
#include "FriendManager.h"
#include "chat.h"
#include "G_chatctrl.h"
#include "FileTransfer.h"
#include "Notifications.h"
#include <functional>
#include <map>
#include <iostream>
#include <thread>
#include <iomanip>
#include "../utils/proto.h"
#include "../utils/IO.h"

using namespace std;

void operationMenu() {
    cout << "[1]开始聊天                  [2]历史记录" << endl;
    cout << "[3]查看好友                  [4]添加好友" << endl;
    cout << "[5]查看添加好友请求          [6]删除好友" << endl;
    cout << "[7]屏蔽好友                  [8]解除屏蔽" << endl;
    cout << "[9]群聊                      [10]发送文件" << endl;
    cout << "[11]接收文件                 [12]查看我的个人信息" << endl;
    cout << "按[0]退出登陆" << endl;
    cout << "请输入您的选择" << endl;
}

void syncFriends(int fd, string my_uid, vector<pair<string, User>> &my_friends) {
    sendMsg(fd, SYNC);                // 1. 发送 SYNC
    my_friends.clear();               // 2. 清空本地 my_friends
    string friend_num;
    recvMsg(fd, friend_num);          // 3. 接收好友个数
    int num = stoi(friend_num);
    User _friend;
    string friend_info;
    for (int i = 0; i < num; i++) {   // 4. 循环接收并解析
        recvMsg(fd, friend_info);
        _friend.json_parse(friend_info);
        my_friends.emplace_back(my_uid, _friend);
    }
}


void clientOperation(int fd, User &user) {
    string my_uid = user.getUID();
    User _friend;
    string friend_num;
    //接收好友个数
    recvMsg(fd, friend_num);
    int num = stoi(friend_num);
    cout << "您的好友个数为:" << num << endl;
    //存储了uid和用户的关系
    vector<pair<string, User>> my_friends;
    string friend_info;
    for (int i = 0; i < num; i++) {
        recvMsg(fd, friend_info);
        _friend.json_parse(friend_info);
        my_friends.emplace_back(my_uid, _friend);
    }
    FriendManager friendManager(fd, user);
    ChatSession chatSession(fd, user);
    GroupChatSession groupChatSession(fd, user);
    FileTransfer fileTransfer(fd, user);
    thread work(announce, user.getUID());
    work.detach();

    while (true) {
        operationMenu();
        string option;
        getline(cin, option);
        if (option.empty()) {
            cout << "输入为空" << endl;
            return;
        }
        if (option.length() > 4) {
            cout << "输入错误" << endl;
            continue;
        }
        if (option == "0") {
            sendMsg(fd, BACK);
            cout << "退出成功" << endl;
            break;
        }
        char *end_ptr;
        int opt = (int) strtol(option.c_str(), &end_ptr, 10);
        if (opt == 0 || option.find(' ') != std::string::npos) {
            std::cout << "输入格式错误 请重新输入" << std::endl;
            continue;
        }
        syncFriends(fd, my_uid, my_friends);
        // 传统if-else分发
        if (opt == 1) {
            chatSession.startChat(my_friends);
        } else if (opt == 2) {
            chatSession.findHistory(my_friends);
        } else if (opt == 3) {
            friendManager.listFriends(my_friends);
        } else if (opt == 4) {
            friendManager.addFriend(my_friends);
        } else if (opt == 5) {
            friendManager.findRequest(my_friends);
        } else if (opt == 6) {
            friendManager.delFriend(my_friends);
        } else if (opt == 7) {
            friendManager.blockedLists(my_friends);
        } else if (opt == 8) {
            friendManager.unblocked(my_friends);
        } else if (opt == 9) {
            groupChatSession.group(my_friends);
        } else if (opt == 10) {
            fileTransfer.sendFile(my_friends);
        } else if (opt == 11) {
            fileTransfer.receiveFile(my_friends);
        } else {
            cout << "没有这个选项，请重新输入" << endl;
        }
    }
}

