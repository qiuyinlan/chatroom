#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <exception>
#include "chat.h"
#include "../utils/proto.h"
#include "../utils/IO.h"
#include "Notifications.h"
#include "../utils/User.h"

using namespace std;

// 颜色定义
#define GREEN "\033[32m"
#define RESET "\033[0m"
#define YELLOW "\033[33m"
#define EXCLAMATION "\033[31m"

ChatSession::ChatSession(int fd, User user) : fd(fd), user(std::move(user)) {}

void ChatSession::startChat(vector<pair<string, User>> &my_friends) {
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
    cout << "-------------------------------------" << endl;

    // 发送LIST_FRIENDS命令查询在线状态
    sendMsg(fd, LIST_FRIENDS);
    sendMsg(fd, to_string(my_friends.size()));

    for (int i = 0; i < my_friends.size(); i++) {
        // 发送好友UID查询在线状态
        sendMsg(fd, my_friends[i].second.getUID());
        string is_online;
        recvMsg(fd, is_online);

        if (is_online == "1") {
            cout << GREEN << i + 1 << ". " << my_friends[i].second.getUsername() << " " << my_friends[i].second.getUID() << " (在线)" << RESET << endl;
        } else {
            cout << i + 1 << ". " << my_friends[i].second.getUsername() << " " << my_friends[i].second.getUID() << " (离线)" << endl;
        }
    }
    cout << "-------------------------------------" << endl;
    int who;
    cout << "请选择聊天对象开始对话" << endl;
    while (!(cin >> who) || who < 0 || who > my_friends.size()) {
        if (cin.eof()) {
            cout << "读到文件bie de结尾" << endl;
            return;
        }
        cout << "输入格式错误" << endl;
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');
    system("clear");
    //发送"4"，服务器进入私聊业务
    sendMsg(fd, START_CHAT);
    who--;
    cout <<  "好友：" << my_friends[who].second.getUsername() << endl;
    string records_index = user.getUID() + my_friends[who].second.getUID();
    //向服务器发送历史聊天记录索引
    sendMsg(fd, records_index);
    Message history;
    string nums;
    //接收历史消息数量
    recvMsg(fd, nums);
    int num = stoi(nums);
    //打印历史消息
    string history_message;
    for (int j = 0; j < num; j++) {
        //接收历史消息
        recvMsg(fd, history_message);
        try {
            history.json_parse(history_message);
            cout << "\t\t\t\t" << history.getTime() << endl;
            cout << history.getUsername() << "  :  " << history.getContent() << endl;
        } catch (const exception& e) {
            // 静默跳过损坏的历史消息
            continue;
        }
    }
    cout << YELLOW << "-------------------以上为历史消息-------------------" << RESET << endl;
    Message message(user.getUsername(), user.getUID(), my_friends[who].second.getUID());
    string friend_UID = my_friends[who].second.getUID();

    sendMsg(fd, friend_UID);
    string isBlocked;

    recvMsg(fd, isBlocked);
    if (isBlocked == "-1") {
        cout << EXCLAMATION << "! " << my_friends[who].second.getUsername()
             << "开启了朋友验证，你还不是他（她）朋友。请先发送朋友验证请求，对方验证通过后，才能聊天。" << RESET << endl;
        cout << "按任意键返回" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "\n检测到输入结束 (Ctrl+D)，退出聊天" << endl;
            return;
        }
    }
    //开线程接收私聊好友对方的消息
    thread work(chatReceived, fd, friend_UID);
    work.detach();
    string msg, json;

    //####有好友，开始聊天
    return_last();
    while (true) {
        getline(cin,msg);
        if (cin.eof()) {
            cout << "\n检测到输入结束 (Ctrl+D)，退出聊天" << endl;
    // 清除 EOF 状态，以便用户下次可以重新进入聊天
             cin.clear();
            return;
        }
        if (msg == "0") {
            //从聊天状态中清除用户
            sendMsg(fd, EXIT);
            //确认数据完整性，防止文件系统数据丢失？？？
            system("sync");
            break;
        }
        else if(msg.empty()){
            cout << "不能发送空白消息" << endl;
            continue;
        }
        message.setContent(msg);
        json = message.to_json();

        //发送聊天消息
        sendMsg(fd, json);
        cout << "你：" << msg << endl;
    }
}

void ChatSession::findHistory(vector<pair<string, User>> &my_friends)
{
    string temp;
    if (my_friends.empty()) {
        cout << "您当前没有好友... 请按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "\n检测到输入结束 (Ctrl+D)，退出聊天" << endl;
            return;
        }
        return;
    }
    sendMsg(fd, HISTORY);
    cout << "好友列表" << endl;
    cout << "-----------------------------------" << endl;
    for (int i = 0; i < my_friends.size(); ++i) {
        cout << i + 1 << ":" << my_friends[i].second.getUsername() << endl;
    }
    cout << "-----------------------------------" << endl;

    //输入要选第几个好友
    int who;
    while (!(cin >> who) || who <= 0 || who > my_friends.size()) {
        if (cin.eof()) {
            cout << "\n检测到输入结束 (Ctrl+D)，退出聊天" << endl;
            return;
        }
        cout << "输入格式错误" << endl;
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');
    //发送想要查看的历史记录的好友的UID
    who--;
    sendMsg(fd, my_friends[who].second.getUID());
    string nums;
    //接收服务器发来的历史记录数量
    recvMsg(fd, nums);
    int num = stoi(nums);
    if (num == 0) {
        cout << "你还没有与他聊天" << endl;
    } else {
        Message message;
        string history_message;
        for (int i = 0; i < num; i++) {
            //接收服务器发送的历史消息记录
            recvMsg(fd, history_message);
            message.json_parse(history_message);
            cout << "\t\t\t\t" << message.getTime() << endl;
            cout << message.getUsername() << ": " << message.getContent() << endl;
        }
    }
    cout << "请按任意键退出" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "\n检测到输入结束 (Ctrl+D)，退出聊天" << endl;
        return;
    }
} 

