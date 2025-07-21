#include "G_chatctrl.h"
#include "../utils/proto.h"
#include "../utils/IO.h"
#include "G_chat.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
using namespace std;
GroupChatSession::GroupChatSession(int fd, User user) : fd(fd), user(std::move(user)) {}
void GroupChatSession::group(vector<pair<string, User>> &my_friends) const {
    sendMsg(fd, GROUP);
    vector<Group> joinedGroup;
    vector<Group> managedGroup;
    vector<Group> createdGroup;
    GroupChat groupChat(fd, user);
    groupChat.sync(createdGroup, managedGroup, joinedGroup);
    int option;
    while (true) {
        groupChat.groupMenu();
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
        groupChat.sync(createdGroup, managedGroup, joinedGroup);
        if (option == 0) {
            sendMsg(fd, BACK);
            return;
        }
        if (option == 1) {
            groupChat.startChat(joinedGroup);
            continue;
        } else if (option == 2) {
            groupChat.createGroup();
            continue;
        } else if (option == 3) {
            groupChat.joinGroup();
            continue;
        } else if (option == 4) {
            groupChat.groupHistory(joinedGroup);
            continue;
        } else if (option == 5) {
            groupChat.managedGroup(managedGroup);
            continue;
        } else if (option == 6) {
            groupChat.managedCreatedGroup(createdGroup);
            continue;
        } else if (option == 7) {
            groupChat.showMembers(joinedGroup);
            continue;
        } else if (option == 8) {
            groupChat.quit(joinedGroup);
            continue;
        } else if (option == 9) {
            groupChat.showJoinedGroup(joinedGroup);
            continue;
        } else if (option == 10) {
            groupChat.showManagedGroup(managedGroup);
            continue;
        } else if (option == 11) {
            groupChat.showCreatedGroup(createdGroup);
            continue;
        }
    }
} 