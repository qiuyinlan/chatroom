#include "Notifications.h"
#include "../utils/proto.h"
#include "../utils/TCP.h"
#include "../utils/IO.h"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <chrono>

using namespace std;


std::atomic<bool> stopNotify{false};

// 全局状态管理


    void ClientState::enterChat(const string& targetUID) {
        lock_guard<mutex> lock(messageMutex);
        inChat = true;
        currentChatUID = targetUID;
    }
    void ClientState::exitChat() {
        lock_guard<mutex> lock(messageMutex);
        inChat = false;
        currentChatUID = "";
    }

    void ClientState::addOfflineMessage(const string& msg) {
        lock_guard<mutex> lock(messageMutex);
        offlineMessages.push_back(msg);
    }

    void ClientState::printOfflineMessages() {
        lock_guard<mutex> lock(messageMutex);
        
        if (!offlineMessages.empty()) {
            cout << "\n=== 离线消息 ===" << endl;
            for (const auto& msg : offlineMessages) {
                cout << msg << endl;
            }
            offlineMessages.clear();
        }
        //没有的话
        else{
            cout << "没有离线消息" << endl;
        }
        
    }


// 静态成员定义
bool ClientState::inChat = false;
string ClientState::currentChatUID = "";
string ClientState::myUID = "";
vector<string> ClientState::offlineMessages;
mutex ClientState::messageMutex;



//统一消息接收线程（使用独立连接）
void unifiedMessageReceiver(int mainFd, string UID) {
    ClientState::myUID = UID;

    // 创建独立的接收连接
    int receiveFd = Socket();
    Connect(receiveFd, IP, PORT);

    // 发送统一接收协议标识
    sendMsg(receiveFd, UNIFIED_RECEIVER);
    sendMsg(receiveFd, UID);

    string receivedMsg;
    cout << "统一消息接收线程已启动，使用独立连接 fd=" << receiveFd << endl;

    // 重新启用初始化阶段逻辑，用于处理离线消息
    bool initializationPhase = true;
    auto startTime = chrono::steady_clock::now();

    while (true) {
        int ret = recvMsg(receiveFd, receivedMsg);
        if (ret <= 0) {
            cout << "统一接收连接断开，退出消息接收线程" << endl;
            break;
        }

        cout << "[DEBUG] 统一接收线程收到消息: " << receivedMsg << endl;

        // 检查是否还在初始化阶段（前3秒）
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(now - startTime).count();
        if (initializationPhase && elapsed >= 3) {
            initializationPhase = false;
            cout << "[DEBUG] 初始化阶段结束，显示离线消息" << endl;
            // 集中显示所有离线消息
            ClientState::printOfflineMessages();
        }

        // 处理不同类型的消息
        processUnifiedMessage(receivedMsg, initializationPhase);
    }

    close(receiveFd);
}

void processUnifiedMessage(const string& msg, bool isInitializationPhase) {
    if (msg == "END" || msg == EXIT) {
        if (msg == "END" && isInitializationPhase) {
            // 如果还在初始化阶段收到END，立即显示离线消息
            cout << "[DEBUG] 收到END标记，提前结束初始化阶段" << endl;
            ClientState::printOfflineMessages();
        }
        return;
    }

    // 处理特殊系统响应
    if (msg == "FRIEND_VERIFICATION_NEEDED" ||
        msg.find("VERIFICATION_NEEDED") != string::npos ||
        msg.find("ND_VERIFICATION_NEEDED") != string::npos) {
        string notifyMsg = "系统提示：对方开启了好友验证，你还不是他（她）朋友，请先发送朋友验证请求，对方验证通过后，才能聊天。";
        if (ClientState::inChat) {
            ClientState::addOfflineMessage(notifyMsg);
        } else {
            cout << YELLOW << notifyMsg << RESET << endl;
        }
        return;
    }

    if (msg == "BLOCKED_MESSAGE") {
        string notifyMsg = "系统提示：消息已发出，但被对方拒收了。";
        if (ClientState::inChat) {
            ClientState::addOfflineMessage(notifyMsg);
        } else {
            cout << YELLOW << notifyMsg << RESET << endl;
        }
        return;
    }

    if (msg == "MESSAGE_SENT") {
        // 消息发送成功，不需要特殊处理
        return;
    }

    // 处理通知消息
    if (msg == REQUEST_NOTIFICATION) {
        string notifyMsg = "你收到一条好友添加申请";
        // 所有通知都在主菜单显示，不在聊天中显示
        cout << notifyMsg << endl;
    }
    else if (msg == GROUP_REQUEST) {
        string notifyMsg = "你收到一条群聊添加申请";
        if (ClientState::inChat) {
            ClientState::addOfflineMessage(notifyMsg);
        } else {
            cout << notifyMsg << endl;
        }
    }
    else if (msg.find("DELETED:") == 0) {
        string notifyMsg = "你已经被" + msg.substr(8) + "删除";
        if (ClientState::inChat) {
            ClientState::addOfflineMessage(notifyMsg);
        } else {
            cout << notifyMsg << endl;
        }
    }
    else if (msg.find("ADMIN_ADD:") == 0) {
        string notifyMsg = "你已经被设为" + msg.substr(10) + "的管理员";
        if (ClientState::inChat) {
            ClientState::addOfflineMessage(notifyMsg);
        } else {
            cout << notifyMsg << endl;
        }
    }
    else if (msg.find("ADMIN_REMOVE:") == 0) {
        string notifyMsg = "你已被取消" + msg.substr(13) + "的管理权限";
        if (ClientState::inChat) {
            ClientState::addOfflineMessage(notifyMsg);
        } else {
            cout << notifyMsg << endl;
        }
    }
    else if (msg.find("MESSAGE:") == 0) {
        string senderInfo = msg.substr(8);
        string notifyMsg;

        // 检查是否包含消息数量
        if (senderInfo.find("(") != string::npos) {
            // 格式：用户名(N条)
            notifyMsg = senderInfo + "消息";
        } else {
            // 格式：用户名
            notifyMsg = senderInfo + "给你发来一条消息";
        }

        if (isInitializationPhase) {
            // 初始化阶段的通知添加到离线消息集合
            ClientState::addOfflineMessage(notifyMsg);
            cout << "[DEBUG] 离线消息通知已添加: " << notifyMsg << endl;
        } else {
            // 正常的实时通知
            cout << notifyMsg << endl;
        }
    }
    else if (msg.find("FILE:") == 0) {
        string sender = msg.substr(5);
        string notifyMsg = sender + "给你发了一个文件";

        if (isInitializationPhase) {
            // 初始化阶段的通知添加到离线消息集合
            ClientState::addOfflineMessage(notifyMsg);
            cout << "[DEBUG] 离线文件通知已添加: " << notifyMsg << endl;
        } else {
            // 正常的实时通知
            cout << notifyMsg << endl;
        }
    }
    else if (msg[0] == '{') {
        // JSON格式的聊天消息
        try {
            Message message;
            message.json_parse(msg);

            // 判断是私聊还是群聊
            if (message.getGroupName() == "1") {
                // 私聊消息
                if (ClientState::inChat && message.getUidFrom() == ClientState::currentChatUID) {
                    // 当前私聊窗口的消息，直接显示
                    cout << message.getUsername() << ": " << message.getContent() << endl;
                }
                // 注意：不在当前聊天窗口的消息不在这里处理，由MESSAGE:通知处理
            } else {
                // 群聊消息
                if (ClientState::inChat && message.getUidTo() == ClientState::currentChatUID) {
                    // 当前群聊窗口的消息，直接显示
                    cout << "[" << message.getGroupName() << "] "
                         << message.getUsername() << ": "
                         << message.getContent() << endl;
                }
                // 注意：不在当前群聊窗口的消息不在这里处理，由MESSAGE:通知处理
            }
        } catch (const exception& e) {
            // JSON解析失败，忽略
            cout << "消息解析失败，跳过" << endl;
        }
    }
}

// 真正的心跳检测（检测连接是否还活着）
void heartbeatChecker(int fd) {
    while (true) {
        this_thread::sleep_for(chrono::seconds(30));  // 30秒检测一次

        // 发送心跳包
        if (sendMsg(fd, "HEARTBEAT") <= 0) {
            cout << "心跳检测失败，连接可能断开" << endl;
            break;
        }
    }
}

bool isNumericString(const std::string &str) {
    for (char c: str) {
        if (!std::isdigit(c)) {
            return false;
        }
    }
    return true;
}
