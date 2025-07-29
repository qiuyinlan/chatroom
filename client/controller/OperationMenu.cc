#include "OperationMenu.h"
#include <unistd.h>
#include "FriendManager.h"
#include "chat.h"
#include "G_chat.h"
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
  
    int send_ret = sendMsg(fd, SYNC);
    if (send_ret <= 0) {     //发送 SYNC
        cout << "服务器连接已断开，无法同步好友列表" << endl;
        return ;
    }
   
    my_friends.clear();               
    string friend_num ;
    // 接收好友个数
    int recv_ret = recvMsg(fd, friend_num);
    if (recv_ret <= 0) { 
         cout << "服务器连接已断开，无法获取好友信息" << endl;
        return ;
    }

    int num;
    try {
        num = stoi(friend_num);

    } catch (const exception& e) {
        cout << "[ERROR] 解析好友数量失败: " << e.what() << ", 内容: '" << friend_num << "'" << endl;
        return ;
    }

    User myfriend;
    string friend_info;
   
    for (int i = 0; i < num; i++) {
         //收好友详细信息
        int recv_ret2 = recvMsg(fd, friend_info);
        if (recv_ret2 <= 0) {
            cout << "服务器连接已断开，好友信息同步中断" << endl;
            return ;
        }
            myfriend.json_parse(friend_info);
            my_friends.emplace_back(my_uid, myfriend);

    }
    return ;
}


void clientOperation(int fd, User &user) {
    string my_uid = user.getUID();
    //好友容器，对
    vector<pair<string, User>> my_friends;
    FriendManager friendManager(fd, user);
    ChatSession chatSession(fd, user);
    G_chat gChat(fd, user);
    FileTransfer fileTransfer(fd, user);
    // 启用通知线程，接收实时通知
    thread work(announce, user.getUID());
    work.detach();

    while (true) {
        operationMenu();
        string option;
        getline(cin, option);
        char *end_ptr;
        if (option.empty()) {
            cout << "输入不能为空" << endl;
            continue;
        }
        if (option.length() > 1) {
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
       
        //字符串类型变成整型，只识别能转换的数字部分
        int opt = (int) strtol(option.c_str(), &end_ptr, 10);
        if (end_ptr == option.c_str() ||   *end_ptr != '\0' || option.find(' ') != std::string::npos) 
        {
            std::cout << "输入格式错误，请输入纯数字选项！" << std::endl;
            continue;
        }
        if ( option.find(' ') != std::string::npos) {
            std::cout << "输入格式错误 请重新输入" << std::endl;
            continue;
        }
        // 同步好友列表，宏定义17
        syncFriends(fd, my_uid, my_friends);

        // if-else分发
        //私聊
        
        if (opt == 1) {
            // 获取群聊列表,宏定义20
            vector<Group> joinedGroup;
                G_chat gChat(fd, user);
                gChat.syncGL(joinedGroup);

                // 调用统一聊天界面
                chatSession.startChat(my_friends, joinedGroup);
            
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
            gChat.groupctrl(my_friends);
        } else if (opt == 8) {
            fileTransfer.sendFile(my_friends);
        } else if (opt == 9) {
            fileTransfer.receiveFile(my_friends);
        } else {
            cout << "没有这个选项，请重新输入" << endl;
        }
    }
}

