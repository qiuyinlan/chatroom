#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <exception>
#include "chat.h"
#include <unistd.h>
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



// 群聊聊天
void ChatSession::startGroupChat(int groupIndex, const vector<Group>& joinedGroups) {
    if (groupIndex < 0 || groupIndex >= joinedGroups.size()) {
        cout << "输入错误，群聊索引无效" << endl;
        return;
    }
    // 从 joinedGroups 中取出一个群，绑定为只读引用，命名为 selectedGroup，从群列表中读取群（引用）
    const Group& selectedGroup = joinedGroups[groupIndex];

   
    // 发送协议
    sendMsg(fd, GROUPCHAT);
    // 拿一个可以传输的副本（拷贝）
    Group groupCopy = selectedGroup; 

    //发送群相关信息
    sendMsg(fd, groupCopy.to_json());
    
    // 接收历史消息
    string historyNum;
    int recv_ret = recvMsg(fd, historyNum);

    if (recv_ret <= 0) {
        cout << "接收群聊历史消息数量失败，连接可能已断开" << endl;
        return;
    }

    int num;
    
        if (historyNum.empty()) {
            cout << "接收到空的群聊历史消息数量" << endl;
            return;
        }
        num = stoi(historyNum);
        if (num < 0 || num > 10000) {
            cout << "接收到异常的群聊历史消息数量: " << historyNum << endl;
            return;
        }
    
   //接收,打印群聊历史
    for (int i = 0; i < num; i++) {
        string historyMsg;
        Message message;
        recv_ret = recvMsg(fd, historyMsg);
        if (recv_ret <= 0) {
            cout << "接收历史消息失败，停止接收" << endl;
            break;
        }
        
       
        if (historyMsg.empty()) {
            continue;
        }
        message.json_parse(historyMsg);
        if (message.getUsername() == user.getUsername()) {
            cout << "你：" << message.getContent() << endl;
            cout << "\t\t\t\t" << message.getTime() << endl;
            continue;
        }
        cout << message.getUsername() << "  :  " << message.getContent() << endl;
        cout << "\t\t\t\t" << message.getTime() << endl;
        
    }
    cout << YELLOW << "-----------------------以上为历史消息-----------------------" << RESET << endl;
    
    // 创建消息对象
    Message message(user.getUsername(), user.getUID(), selectedGroup.getGroupUid(),"1");
    message.setGroupName(selectedGroup.getGroupName());
    
    // 开启接收线程
    thread receiveThread(chatReceived, fd, selectedGroup.getGroupUid());
    receiveThread.detach();
    
    // 消息发送循环
    string msg;
    return_last();
    while (true) {
        getline(cin, msg);
        
        if (cin.eof()) {
            cout << "输入结束，退出群聊" << endl;
            sendMsg(fd, EXIT);
            return ;
        }
        
        if (msg == "0") {
            sendMsg(fd, EXIT);
            return;
        }
        
        if (msg.empty()) {
            cout << "不能发送空白消息" << endl;
            continue;
        }
        cout << "你：" << msg << endl;
        message.setContent(msg);
        string jsonMsg = message.to_json();
        sendMsg(fd, jsonMsg);

    }
}
ChatSession::ChatSession(int fd, User user) : fd(fd), user(std::move(user)) {}

void ChatSession::startChat(vector<pair<string, User>> &my_friends,vector<Group> &joinedGroup) {
    string temp;

    cout << "==============开始聊天================" << endl;
    cout << "【好友聊天】" << endl;

    if (my_friends.empty()) {
        cout << "你当前没有好友，快去添加吧" << endl;
    }
    //调用函数
    else{
        sendMsg(fd, LIST_FRIENDS);
        //发好友个数
        sendMsg(fd, to_string(my_friends.size()));

        for (int i = 0; i < my_friends.size(); i++) {
            
            //发
            sendMsg(fd, my_friends[i].second.getUID());
            string is_online;

            //收
            int recv_ret = recvMsg(fd, is_online);
            if (is_online == "1") {
                cout << GREEN << i + 1 << ". " << my_friends[i].second.getUsername() <<  " (在线)" << RESET << endl;
            } else {
                cout << i + 1 << ". " << my_friends[i].second.getUsername() << " " << my_friends[i].second.getUID() << " (离线)" << endl;
            }
        }
    }
    cout << endl ;
    cout << "【群聊聊天】" << endl;
    if (joinedGroup.empty()) {
        cout << "当前没有加入的群,快去添加吧" << endl;
    }
    else{
        //群聊打印
        for (int i = 0; i < joinedGroup.size(); i++) {
            cout << my_friends.size() + i + 1 << ". "  << joinedGroup[i].getGroupName()  << endl;
        }
    }

    cout << "--------------------------------------" << endl;
    int totalOptions = my_friends.size() + joinedGroup.size();
    
    if (totalOptions == 0) {
        cout << "没有可聊天的对象，按任意键返回" << endl;
        string temp;
        getline(cin, temp);
        return;
    }
    
    cout << "请选择聊天对象" << endl;
    return_last();
    
    int who;
    while (true) {
        if (!(cin >> who)) {
            // 输入错误处理
            cout << "输入格式错误，请输入数字" << endl;
            cin.clear();  // 清除错误状态
            cin.ignore(INT32_MAX, '\n');  // 忽略错误输入
            continue;
        }

        if (who == 0) {
            //不需要发，直接return即可
            cin.ignore(INT32_MAX, '\n');
            return;  // 用户选择退出
        }
        else if (who < 1 || who > totalOptions) {
            cout << "输入格式错误，请输入1-" << totalOptions << "之间的数字" << endl;
            continue;
        }
        
        // 有效选择，退出循环
        break;
    }

    // 清理输入缓冲区，确保没有残留字符
    cin.ignore(INT32_MAX, '\n');

    // 根据选择分发到不同的聊天函数
    //私聊
    if (who <= my_friends.size()) {
        cout << "--------------------------------------" << endl;
        cout << "【好友：" << my_friends[who-1].second.getUsername() << "】"<< endl;
        // 发送好友聊天协议
        sendMsg(fd, START_CHAT);
        string records_index = user.getUID() + my_friends[who-1].second.getUID();
        //发索引
        sendMsg(fd, records_index);

        Message history;
        string nums;
        //收数量
        int recv_ret = recvMsg(fd, nums);
        if (recv_ret <= 0) {
            cout << "接收历史消息数量失败，连接可能已断开" << endl;
            return;
        }

        int num;
        try {
            if (nums.empty()) {
                cout << "接收到空的消息数量字符串" << endl;
                return;
            }
            num = stoi(nums);
            if (num < 0 || num > 10000) {
                cout << "接收到异常的消息数量: " << nums << endl;
                return;
            }
        } catch (const exception& e) {
            cout << "解析消息数量失败: '" << nums << "', 错误: " << e.what() << endl;
            return;
        }


        string history_message;
        for (int j = 0; j < num; j++) {
            //循环收
            int msg_ret = recvMsg(fd, history_message);
            if (msg_ret <= 0) {
                cout << "接收历史消息失败，停止接收" << endl;
                break;
            }
                //###待完善：空的消息，可以发和接收，但是不打印在历史记录里面
                if (history_message.empty()) {
                    continue;
                }

                history.json_parse(history_message);
                if (history.getUsername() == user.getUsername()) {
                    cout << "你：" << history.getContent() << endl;
                    cout << "\t\t\t\t" << history.getTime() << endl;
                    continue;
                }
                cout << history.getUsername() << "  :  " << history.getContent() << endl;
                cout << "\t\t\t\t" << history.getTime() << endl;
                
        }
        cout << YELLOW << "-------------------以上为历史消息-------------------" << RESET << endl;

        
        Message message(user.getUsername(), user.getUID(), my_friends[who-1].second.getUID(),"1");
        string friend_UID = my_friends[who-1].second.getUID();

        
        sendMsg(fd, friend_UID);

        //实时接收对方消息
        thread work(chatReceived, fd, friend_UID);

        // 立即detach，避免线程管理问题
        work.detach();
        string msg, json;

        //真正开始聊天
        return_last();
        while (true) {
            getline(cin,msg);
            if (cin.eof()) {
                cout << "\n检测到输入结束 (Ctrl+D)，退出聊天" << endl;
                cin.clear();
                sendMsg(fd, EXIT);
                return;
            }
            if (msg == "0") {
                sendMsg(fd, EXIT);
                return;
            }
            else if(msg.empty()){
                cout << "不能发送空白消息" << endl;
                continue;
            }
            message.setContent(msg);
            json = message.to_json();

            sendMsg(fd, json);
            cout << "你：" << msg << endl;
        }
    } else {
        // 选择的是群聊聊天
        int groupIndex = who - my_friends.size() - 1;
        startGroupChat(groupIndex, joinedGroup);
    }
}

