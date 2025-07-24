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
    cout << "[1]开始聊天                  [2]添加好友" << endl;
    cout << "[3]查看添加好友请求          [4]删除好友" << endl;
    cout << "[5]屏蔽好友                  [6]解除屏蔽" << endl;
    cout << "[7]群聊                      [8]发送文件" << endl;
    cout << "[9]接收文件" << endl;
    cout << "按[0]退出登陆" << endl;
    cout << "请输入你的选择" << endl;
}

void syncFriends(int fd, string my_uid, vector<pair<string, User>> &my_friends) {
    if (sendMsg(fd, SYNC) <= 0) {     // 1. 发送 SYNC，检查连接
        cout << "服务器连接已断开，无法同步好友列表" << endl;
        return;
    }

    my_friends.clear();               // 2. 清空本地 my_friends
    string friend_num;
    if (recvMsg(fd, friend_num) <= 0) { // 3. 接收好友个数，检查连接
        cout << "服务器连接已断开，无法获取好友信息" << endl;
        return;
    }

    int num = stoi(friend_num);
    User _friend;
    string friend_info;
    for (int i = 0; i < num; i++) {   // 4. 循环接收并解析
        if (recvMsg(fd, friend_info) <= 0) {
            cout << "服务器连接已断开，好友信息同步中断" << endl;
            return;
        }
        //接收好友信息
        _friend.json_parse(friend_info);
        my_friends.emplace_back(my_uid, _friend);
    }
}


void clientOperation(int fd, User &user) {
    string my_uid = user.getUID();
    vector<pair<string, User>> my_friends;
    FriendManager friendManager(fd, user);
    ChatSession chatSession(fd, user);
    GroupChatSession groupChatSession(fd, user);
    FileTransfer fileTransfer(fd, user);
    // announce线程，提供实时通知
    thread work(announce, user.getUID());
    work.detach();

    while (true) {
        operationMenu();
        string option;
        getline(cin, option);
        if (option.empty()) {
            cout << "输入为空" << endl;
            continue;
        }
        if (option.length() > 4) {
            cout << "输入错误" << endl;
            continue;
        }
        if (option == "0") {
            if (sendMsg(fd, BACK) <= 0) {
                cout << "服务器连接已断开，客户端退出" << endl;
            } else {
                cout << "退出成功" << endl;
            }
            return;
        }
        char *end_ptr;
        int opt = (int) strtol(option.c_str(), &end_ptr, 10);
        if ( option.find(' ') != std::string::npos) {
            std::cout << "输入格式错误 请重新输入" << std::endl;
            continue;
        }
        syncFriends(fd, my_uid, my_friends);
        // if-else分发
        if (opt == 1) {
            chatSession.startChat(my_friends);
        } else if (opt == 2) {
            friendManager.addFriend(my_friends);
        } else if (opt == 3) {
            friendManager.findRequest(my_friends);
        } else if (opt == 4) {
            friendManager.delFriend(my_friends);
        } else if (opt == 5) {
            friendManager.blockedLists(my_friends);
        } else if (opt == 6) {
            friendManager.unblocked(my_friends);
        } else if (opt == 7) {
            groupChatSession.group(my_friends);
        } else if (opt == 8) {
            fileTransfer.sendFile(my_friends);
        } else if (opt == 9) {
            fileTransfer.receiveFile(my_friends);
        } else {
            cout << "没有这个选项，请重新输入" << endl;
        }
    }
}

