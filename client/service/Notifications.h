#ifndef CHATROOM_NOTIFICATIONS_H
#define CHATROOM_NOTIFICATIONS_H

#include "User.h"

#pragma once
#include <atomic>
#include <vector>
#include <mutex>
#include <string>

extern std::atomic<bool> stopNotify;


bool isNumericString(const std::string &str);

// 新的统一消息处理
void unifiedMessageReceiver(int fd, string UID);
void processUnifiedMessage(const string& msg, bool isInitializationPhase = false);

// 全局状态管理类完整定义
class ClientState {
public:
    static bool inChat;
    static std::string currentChatUID;
    static std::string myUID;
    static std::vector<std::string> offlineMessages;  // 离线消息缓存
    static std::mutex messageMutex;

    static void enterChat(const std::string& targetUID);
    static void exitChat();
    static void addOfflineMessage(const std::string& msg);
    static void printOfflineMessages();
};



#endif //CHATROOM_NOTIFICATIONS_H
