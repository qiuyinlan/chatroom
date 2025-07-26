#include "G_chat.h"
#include <stdexcept>
#include <unistd.h>
#include "User.h"
#include "IO.h"
#include "Group.h"
#include "proto.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <thread>
#include <functional>
#include <map>
#include <exception>
#include "Notifications.h"
using namespace std;

void G_chat::groupMenu() {
    cout << "[1]开始聊天                     [2]创建群聊" << endl;
    cout << "[3]加入群聊                     [4]查看群聊历史记录" << endl;
    cout << "[5]管理我的群                   [6]管理我创建的群" << endl;
    cout << "[7]查看群成员                   [8]退出群聊" << endl;
    cout << "[9]查看我加入的群                [10]查看我管理的群" << endl;
    cout << "[11]查看我创建的群" << endl;
    cout << "[0]返回" << endl;
    cout << "请输入您的选择" << endl;
}

void G_chat::groupctrl(vector<pair<string, User>> &my_friends) {
    sendMsg(fd, GROUP);
    vector<Group> joinedGroup;
    vector<Group> managedGroup;
    vector<Group> createdGroup;
    //自动发完，两个同步函数，对应同步
    sync(createdGroup, managedGroup, joinedGroup);
    int option;
    while (true) {
        groupMenu();
        while (!(cin >> option)) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        sendMsg(fd, "11");
        sync(createdGroup, managedGroup, joinedGroup);
        if (option == 0) {
            sendMsg(fd, BACK);
            return;
        }
        if (option == 1) {
           startChat(joinedGroup);
            continue;
        } else if (option == 2) {
            createGroup();
            continue;
        } else if (option == 3) {
            joinGroup();
            continue;
        } else if (option == 4) {
            groupHistory(joinedGroup);
            continue;
        } else if (option == 5) {
            managed_Group(managedGroup);
            continue;
        } else if (option == 6) {
            managedCreatedGroup(createdGroup);
            continue;
        } else if (option == 7) {
            showMembers(joinedGroup);
            continue;
        } else if (option == 8) {
            quit(joinedGroup);
            continue;
        } else if (option == 9) {
            showJoinedGroup(joinedGroup);
            continue;
        } else if (option == 10) {
            showManagedGroup(managedGroup);
            continue;
        } else if (option == 11) {
            showCreatedGroup(createdGroup);
            continue;
        }
    }
} 

G_chat::G_chat(int fd, const User &user) : fd(fd), user(user) {
    joined = "joined" + user.getUID();
    created = "created" + user.getUID();
    managed = "managed" + user.getUID();
}

void G_chat::syncGL(std::vector<Group> &joinedGroup) {
    joinedGroup.clear();
    // 发送群聊列表获取请求
    int ret = sendMsg(fd, SYNCGL);
    if (ret <= 0) {
        cerr << "[ERROR] 发送同步群聊列表请求失败" << endl;
        return;
    }
    string nums;

    //接受群聊数量
    int recv_ret = recvMsg(fd, nums);
    if (recv_ret <= 0) {
        cerr << "[ERROR] 接收群聊数量失败，连接可能断开" << endl;
        return;
    }

    int num = stoi(nums);
    if(num == 0){
        return;
        //如果要同步三个，就不能直接返回了
    }
    string group_info;
    //接收群info

    for (int i = 0; i < num; i++) {
        Group group;

        cout <<"1"<< group_info << endl;
        //接收群聊,如果有群聊,就要json解析，没有直接return
        int ret = recvMsg(fd, group_info);
        if(ret <= 0){
            cerr << "[ERROR] 接收群聊名称失败，连接可能断开" << endl;
            return;
        }
        try {
            // 验证JSON格式
            if (group_info == "0" ) {
                return;
                // cerr << "[ERROR] 接收到无效的JSON格式: " << group_info << endl;
                // throw runtime_error("JSON格式错误");
            }
            
            group.json_parse(group_info);
            joinedGroup.push_back(group);
        } catch (const exception& e) {
            cerr << "[ERROR] 创建的群JSON解析失败: " << e.what() << endl;
            cerr << "[ERROR] 问题JSON: " << group_info << endl;
            throw; // 重新抛出异常，让上层处理
        }

    }

}

//###下列函数信号协议待更改
void G_chat::sync(vector<Group> &createdGroup, vector<Group> &managedGroup, vector<Group> &joinedGroup) const {
    createdGroup.clear();
    managedGroup.clear();
    joinedGroup.clear();



    string nums;

    //得到群聊数量
    int recv_ret = recvMsg(fd, nums);
    if (recv_ret <= 0) {
        cerr << "[ERROR] 接收创建群数量失败，连接可能断开" << endl;
       
    }

    int num;
    try {
        if (nums.empty()) {
            cerr << "[ERROR] 接收到空的创建群数量" << endl;
            throw runtime_error("接收到空数据");
        }
        num = stoi(nums);
        if (num < 0 || num > 1000) {
            cerr << "[ERROR] 创建群数量异常: " << num << endl;
            throw runtime_error("数据异常");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] 解析创建群数量失败: " << e.what() << ", 内容: '" << nums << "'" << endl;
        throw;
    }
    string group_info;
    for (int i = 0; i < num; i++) {
        Group group;

        recvMsg(fd, group_info);
        try {
            // 验证JSON格式
            if (group_info.empty() || group_info[0] != '{' || group_info.back() != '}') {
                cerr << "[ERROR] 接收到无效的JSON格式: " << group_info << endl;
                throw runtime_error("JSON格式错误");
            }
            group.json_parse(group_info);
            createdGroup.push_back(group);
        } catch (const exception& e) {
            cerr << "[ERROR] 创建的群JSON解析失败: " << e.what() << endl;
            cerr << "[ERROR] 问题JSON: " << group_info << endl;
            throw; // 重新抛出异常，让上层处理
        }
    }
    //收
    recv_ret = recvMsg(fd, nums);
    if (recv_ret <= 0) {
        cerr << "[ERROR] 接收管理群数量失败，连接可能断开" << endl;
        throw runtime_error("网络连接错误");
    }

    try {
        if (nums.empty()) {
            cerr << "[ERROR] 接收到空的管理群数量" << endl;
            throw runtime_error("接收到空数据");
        }
        num = stoi(nums);
        if (num < 0 || num > 1000) {
            cerr << "[ERROR] 管理群数量异常: " << num << endl;
            throw runtime_error("数据异常");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] 解析管理群数量失败: " << e.what() << ", 内容: '" << nums << "'" << endl;
        throw;
    }
    for (int i = 0; i < num; i++) {
        Group group;

        recvMsg(fd, group_info);
        try {
            group.json_parse(group_info);
            managedGroup.push_back(group);
        } catch (const exception& e) {
            cerr << "[ERROR] 管理的群JSON解析失败: " << e.what() << endl;
            cerr << "[ERROR] 问题JSON: " << group_info << endl;
        }
    }
    //收
    recv_ret = recvMsg(fd, nums);
    if (recv_ret <= 0) {
        cerr << "[ERROR] 接收加入群数量失败，连接可能断开" << endl;
        throw runtime_error("网络连接错误");
    }

    try {
        if (nums.empty()) {
            cerr << "[ERROR] 接收到空的加入群数量" << endl;
            throw runtime_error("接收到空数据");
        }
        num = stoi(nums);
        if (num < 0 || num > 1000) {
            cerr << "[ERROR] 加入群数量异常: " << num << endl;
            throw runtime_error("数据异常");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] 解析加入群数量失败: " << e.what() << ", 内容: '" << nums << "'" << endl;
        throw;
    }
    for (int i = 0; i < num; i++) {
        Group group;

        recvMsg(fd, group_info);
        try {
            group.json_parse(group_info);
            joinedGroup.push_back(group);
        } catch (const exception& e) {
            cerr << "[ERROR] 加入的群JSON解析失败: " << e.what() << endl;
            cerr << "[ERROR] 问题JSON: " << group_info << endl;
        }
    }

}

void G_chat::startChat(vector<Group> &joinedGroup) {
    string temp;
    if (joinedGroup.empty()) {
        cout << "您当前没有加入任何群聊" << endl;
        cout << "按任意键返回" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            return;
        }
        return;
    }
    cout << user.getUsername() << "加入的群聊" << endl;
    cout << "---------------------------------------" << endl;
    for (int i = 0; i < joinedGroup.size(); i++) {
        cout << i + 1 << ". " << joinedGroup[i].getGroupName() << endl;
    }
    cout << "---------------------------------------" << endl;
    int which;
    while (!(cin >> which) || which < 0 || which > joinedGroup.size()) {
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');

    sendMsg(fd, "1");
    which--;

    sendMsg(fd, joinedGroup[which].to_json());
    cout << joinedGroup[which].getGroupName() << endl;
    string nums;

    int ret = recvMsg(fd, nums);
    if (ret == 0) {
        return;
    }
    int num = stoi(nums);
    string msg;
    Message history_message;
    for (int i = 0; i < num; i++) {
        recvMsg(fd, msg);
        try {
            history_message.json_parse(msg);
            cout << "\t\t\t\t" << history_message.getTime() << endl;
            cout << history_message.getUsername() << ": " << history_message.getContent() << endl;
        } catch (const exception& e) {
            // 静默跳过损坏的历史消息
            continue;
        }
    }
    cout << YELLOW << "-----------------------以上为历史消息-----------------------" << RESET << endl;
    Message message(user.getUsername(), user.getUID(), joinedGroup[which].getGroupUid());
    message.setGroupName(joinedGroup[which].getGroupName());
    //开线程接收群成员消息
    thread work(chatReceived, fd, joinedGroup[which].getGroupUid());
    work.detach();
    string m;
    while (true) {
        cin >> m;
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        if (m == "quit") {
            sendMsg(fd, EXIT);
            getchar();
            break;
        }
        message.setContent(m);
        string json_msg;
        json_msg = message.to_json();

        sendMsg(fd, json_msg);
    }
}

void G_chat::createGroup() {
    sendMsg(fd, "2");
    string groupName;
    while (true) {
        cout << "您要创建的群聊名称是什么" << endl;
        getline(cin, groupName);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        if (groupName.find(' ') != string::npos) {
            cout << "群聊名称不能出现空格" << endl;
            continue;
        }
        break;
    }
    Group group(groupName, user.getUID());

    sendMsg(fd, group.to_json());
    cout << "创建成功，该群的群号为：" << group.getGroupUid() << endl;
    string temp;
    cout << "按任意键返回" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::joinGroup() const {
    sendMsg(fd, "3");
    string group_name;
    while (true) {
        cout << "输入你要加入的群聊名称" << endl;
        getline(cin, group_name);
        if (cin.eof()) {
            cout << "文件读到结尾" << endl;
            return;
        }
        if (group_name.empty()) {
            cout << "群聊名称不能为空" << endl;
            continue;
        }
        if (group_name.find(' ') != string::npos) {
            cout << "群聊名称不能包含空格" << endl;
            continue;
        }
        break;
    }

    sendMsg(fd, group_name);
    string response;

    recvMsg(fd, response);
    if (response == "-1") {
        cout << "该群不存在" << endl;
    } else if (response == "-2") {
        cout << "您已经是该群成员" << endl;
    }
    cout << "请求已经发出，等待同意" << endl;
    string temp;
    cout << "按任意键返回" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}



void G_chat::groupHistory(const std::vector<Group> &joinedGroup) {
    string temp;
    if (joinedGroup.empty()) {
        cout << "当前没有加入的群... 按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << user.getUsername() << "加入的群聊" << endl;
    cout << "----------------------------------" << endl;
    for (int i = 0; i < joinedGroup.size(); i++) {
        cout << i + 1 << ". " << joinedGroup[i].getGroupName() << endl;
    }
    cout << "----------------------------------" << endl;
    cout << "输入要查看的群" << endl;
    int which;
    while (!(cin >> which) || which < 0 || which > joinedGroup.size()) {
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        cout << "输入格式错误" << endl;
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');

    sendMsg(fd, "4");
    which--;

    sendMsg(fd, joinedGroup[which].getGroupUid());
    Message message;
    string nums;

    recvMsg(fd, nums);
    int num = stoi(nums);
    if (num == 0) {
        cout << "该群暂未有人发言" << endl;
        return;
    }
    string history_msg;
    for (int i = 0; i < num; i++) {

        recvMsg(fd, history_msg);
        message.json_parse(history_msg);
        cout << message.getTime() << message.getUsername() << ": " << message.getContent() << endl;
    }
    cout << "按任意键退出" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::managedMenu() {
    cout << "[1]处理入群申请" << endl;
    cout << "[2]处理群用户" << endl;
    cout << "[0]返回" << endl;
}

void G_chat::managed_Group(vector<Group> &managedGroup) const {
    string temp;
    if (managedGroup.empty()) {
        cout << "您当前还没有可以管理的群... 按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << "-----------------------------------" << endl;
    for (int i = 0; i < managedGroup.size(); i++) {
        string groupName = managedGroup[i].getGroupName();
        cout << i + 1 << ". " << groupName << endl;
    }

    
    cout << "-----------------------------------" << endl;
    cout << "选择你要管理的群（输入1-" << managedGroup.size() << "）" << endl;
    int which;
    while (!(cin >> which) || which < 1 || which > managedGroup.size()) {
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        cout << "输入格式错误，请输入1-" << managedGroup.size() << "之间的数字" << endl;
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');
    sendMsg(fd, "5");
    which--;

    sendMsg(fd, managedGroup[which].to_json());
    while (true) {
        managedMenu();
        int choice;
        while (!(cin >> choice) || (choice != 1 && choice != 2 && choice != 0)) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        if (choice == 0) {

            sendMsg(fd, BACK);
            break;
        }
        if (choice == 1) {
            approve();
            break;
        } else if (choice == 2) {
            remove(managedGroup[which]);
            break;
        }

    }
}

void G_chat::approve() const {
    sendMsg(fd, "1");
    string nums;

    recvMsg(fd, nums);
    int num = stoi(nums);
    if (num == 0) {
        cout << "暂无入群申请" << endl;
        return;
    }
    string buf;
    for (int i = 0; i < num; i++) {

        recvMsg(fd, buf);
        cout << "收到" << buf << "的入群申请" << endl;
        cout << "[y]YES,[n]NO" << endl;
        string choice;
        getline(cin, choice);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        if (choice != "y" && choice != "n") {
            cout << "输入格式错误" << endl;
            continue;
        }

        sendMsg(fd, choice);
        if (choice == "y") {
            cout << "添加成功" << endl;
        } else {
            cout << "添加失败" << endl;
        }
    }
    cout << "按任意键返回" << endl;
    getline(cin, buf);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::remove(Group &group) const {
    sendMsg(fd, "2");
    string buf;
    User member;
    vector<User> arr;
    //接收服务器发送的群员数量
    recvMsg(fd, buf);
    int num = stoi(buf);
    for (int i = 0; i < num; i++) {
        //接收服务器发送的群员信息
        recvMsg(fd, buf);
        member.json_parse(buf);
        arr.push_back(member);
        if (member.getUID() == group.getOwnerUid()) {
            cout << i + 1 << "." << member.getUsername() << "(群主)" << endl;
        } else {
            cout << i + 1 << "." << member.getUsername() << endl;
        }
    }
    while (true) {
        cout << "你要踢谁（输入1-" << arr.size() << "），按[0]返回" << endl;
        int who;
        while (!(cin >> who) || who < 0 || who > (int)arr.size()) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误，请输入0-" << arr.size() << "之间的数字" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        if (who == 0) {
            sendMsg(fd, "0");
            return;
        }
        who--;
        if (arr[who].getUID() == group.getOwnerUid()) {
            cout << "该用户是群主，你不能踢！" << endl;
            continue;
        }

        sendMsg(fd, arr[who].to_json());
        cout << "删除成功，按任意键返回" << endl;
        getline(cin, buf);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
    }
}

void G_chat::ownerMenu() {
    cout << "[1]设置管理员" << endl;
    cout << "[2]撤销管理员" << endl;
    cout << "[3]解散群聊" << endl;
    cout << "[0]返回" << endl;
}

void G_chat::managedCreatedGroup(vector<Group> &createdGroup) {
    string temp;
    if (createdGroup.empty()) {
        cout << "您当前还没有创建群... 按任意键退出" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << user.getUsername() << "的群" << endl;
    cout << "------------------------------" << endl;
    for (int i = 0; i < createdGroup.size(); i++) {
        cout << i + 1 << ". " << createdGroup[i].getGroupName() << endl;
    }
    cout << "------------------------------" << endl;
    cout << "您要整哪个群" << endl;
    int which;
    while (!(cin >> which) || which < 0 || which > createdGroup.size()) {
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        cout << "输入格式错误" << endl;
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');

    sendMsg(fd, "6");
    which--;

    sendMsg(fd, createdGroup[which].to_json());
    G_chat gc;
    while (true) {
        ownerMenu();
        int choice;
        while (!(cin >> choice) || choice < 0 || choice > 3) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        //！！！待检查
        if (choice == 0) {
            sendMsg(fd, "0");
            return;
        }
        if (choice == 1) {
            appointAdmin(createdGroup[which]); // 直接调用成员函数
        } else if (choice == 2) {
            revokeAdmin(createdGroup[which]); // 直接调用成员函数
        } else if (choice == 3) {
            deleteGroup(createdGroup[which]); // 直接调用成员函数
        }
    }

}

void G_chat::appointAdmin(Group &createdGroup) const {
    sendMsg(fd, "1");
    vector<User> arr;
    string nums;
    User member;

    recvMsg(fd, nums);
    int num = stoi(nums);
    string member_info;
    for (int i = 0; i < num; i++) {

        recvMsg(fd, member_info);
        member.json_parse(member_info);
        arr.push_back(member);
        if (member.getUID() == createdGroup.getOwnerUid()) {
            cout << i + 1 << ". " << member.getUsername() << "群主" << endl;
        } else {
            cout << i + 1 << ". " << member.getUsername() << endl;
        }
    }
    int who;
    while (true) {
        cout << "您要任命谁为管理员" << endl;
        while (!(cin >> who) || who < 0 || who > arr.size()) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        who--;
        if (arr[who].getUID() == createdGroup.getOwnerUid()) {
            cout << "该用户为群主" << endl;
            continue;
        }
        break;
    }

    sendMsg(fd, arr[who].to_json());
    string reply;

    recvMsg(fd, reply);
    if (reply == "-1") {
        cout << "该用户为管理员，无法多次设置" << endl;
        return;
    }
    cout << "按任意键退出" << endl;
    string temp;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::revokeAdmin(Group &createdGroup) const {
    sendMsg(fd, "2");
    string nums;

    recvMsg(fd, nums);
    int num = stoi(nums);
    string admin_info;
    User admin;
    vector<User> arr;
    for (int i = 0; i < num; i++) {

        recvMsg(fd, admin_info);
        admin.json_parse(admin_info);
        arr.push_back(admin);
        if (admin.getUID() == createdGroup.getOwnerUid()) {
            cout << i + 1 << admin.getUsername() << "群主" << endl;
        } else {
            cout << i + 1 << admin.getUsername() << endl;
        }
    }
    int who;
    while (true) {
        cout << "选择你要取消的人" << endl;
        while (!(cin >> who) || who < 0 || who > arr.size()) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        who--;
        if (arr[who].getUID() == createdGroup.getOwnerUid()) {
            cout << "不能取消群主的管理权限" << endl;
            continue;
        }
        break;
    }

    sendMsg(fd, arr[who].to_json());
    cout << "撤销成功，按任意键返回" << endl;
    string temp;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::deleteGroup(Group &createdGroup) const {
    sendMsg(fd, "3");
    cout << "解散成功，按任意键返回" << endl;
    string temp;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::showMembers(std::vector<Group> &group) {
    cout << user.getUsername() << endl;
    cout << "--------------------------" << endl;
    for (int i = 0; i < group.size(); i++) {
        cout << i + 1 << ". " << group[i].getGroupName() << endl;
    }
    cout << "--------------------------" << endl;
    cout << "你要查看哪个群" << endl;
    int which;
    while (!(cin >> which) || which < 0 || which > group.size()) {
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        cout << "输入格式错误" << endl;
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');

    sendMsg(fd, "7");
    which--;
    cout << group[which].getGroupName() << endl;

    sendMsg(fd, group[which].to_json());
    string nums;

    recvMsg(fd, nums);
    int num = stoi(nums);
    string member_info;
    for (int i = 0; i < num; i++) {

        recvMsg(fd, member_info);
        cout << i + 1 << ". " << member_info << endl;
    }
    cout << "按任意键返回" << endl;
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::quit(vector<Group> &joinedGroup) {
    string temp;
    if (joinedGroup.empty()) {
        cout << "您当前没有加入任何群聊" << endl;
        cout << "按任意键返回" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            return;
        }
        return;
    }
    cout << user.getUsername() << endl;
    cout << "-------------------------------------------" << endl;
    for (int i = 0; i < joinedGroup.size(); i++) {
        if (joinedGroup[i].getOwnerUid() == user.getUID()) {
            cout << i + 1 << ". " << joinedGroup[i].getGroupName() << "(您是群主)" << endl;
        } else {
            cout << i + 1 << ". " << joinedGroup[i].getGroupName() << endl;
        }
    }
    cout << "-------------------------------------------" << endl;
    int which;
    while (true) {
        cout << "输入你要退出的群" << endl;
        while (!(cin >> which) || which < 0 || which > joinedGroup.size()) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        which--;
        if (joinedGroup[which].getOwnerUid() == user.getUID()) {
            cout << "您是该群群主，不能退出" << endl;
            continue;
        }
        break;
    }
    //退群是"8"
    sendMsg(fd, "8");

    sendMsg(fd, joinedGroup[which].to_json());
    cout << "您已退出该群，按任意键退出" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}


void G_chat::showJoinedGroup(const std::vector<Group> &joinedGroup) {
    if (joinedGroup.empty()) {
        cout << "您未加入任何群聊" << endl;
        cout << "按任意键返回" << endl;
        string temp;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << user.getUsername() << "加入的群聊" << endl;
    cout << "------------------------------------" << endl;
    for (int i = 0; i < joinedGroup.size(); ++i) {
        cout << i + 1 << ". " << joinedGroup[i].getGroupName() << "(" << joinedGroup[i].getGroupUid() << ")" << endl;
    }
    cout << "------------------------------------" << endl;
    cout << "按任意键返回" << endl;
    string temp;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::showManagedGroup(vector<Group> &managedGroup) {
    string temp;
    if (managedGroup.empty()) {
        cout << "您未管理任何群" << endl;
        cout << "按任意键返回" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << user.getUsername() << "管理的群" << endl;
    cout << "----------------------------------" << endl;
    for (int i = 0; i < managedGroup.size(); ++i) {
        cout << i + 1 << ". " << managedGroup[i].getGroupName() << "(" << managedGroup[i].getGroupUid() << ")" << endl;
    }
    cout << "----------------------------------" << endl;
    cout << "按任意键退出" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void G_chat::showCreatedGroup(std::vector<Group> &createdGroup) {
    string temp;
    if (createdGroup.empty()) {
        cout << "您未创建任何群" << endl;
        cout << "按任意键返回" << endl;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }
    cout << user.getUsername() << "创建的群" << endl;
    cout << "----------------------------------" << endl;
    for (int i = 0; i < createdGroup.size(); ++i) {
        cout << i + 1 << ". " << createdGroup[i].getGroupName() << "(" << createdGroup[i].getGroupUid() << ")" << endl;
    }
    cout << "----------------------------------" << endl;
    cout << "按任意键退出" << endl;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}
