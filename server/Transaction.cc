#include "Transaction.h"
#include "Redis.h"
#include "../utils/IO.h"
#include "../utils/proto.h"
#include <iostream>
#include "group_chat.h"
#include <functional>
#include <map>
#include <sys/stat.h>
#include <filesystem>
#include <vector>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <nlohmann/json.hpp>


using namespace std;
using json = nlohmann::json;




//同步好友 
void synchronize(int fd, User &user) {
    Redis redis;
    redis.connect();
    string friend_info;
    int num = redis.scard(user.getUID());

    redisReply **arr = redis.smembers(user.getUID());

    // 先过滤有效好友，收集有效的好友信息
    vector<string> validFriendInfos;
    if (arr != nullptr) {
        for (int i = 0; i < num; i++) {
            cout << "[DEBUG] 检查好友 " << (i+1) << "/" << num << ", UID: " << arr[i]->str << endl;
            friend_info = redis.hget("user_info", arr[i]->str);

            if (friend_info.empty() || friend_info.length() < 10) {
                cout << "[ERROR] 好友信息无效，跳过 UID: " << arr[i]->str << endl;
            } else {
                validFriendInfos.push_back(friend_info);
                cout << "[DEBUG] 有效好友信息: " << friend_info.substr(0, 50) << "..." << endl;
            }
            freeReplyObject(arr[i]);
        }
    }

    // 发送有效好友数量
    int validCount = validFriendInfos.size();
    cout << "[DEBUG] 发送有效好友数量: " << validCount << " (原数量: " << num << ")" << endl;
    sendMsg(fd, to_string(validCount));

    // 发送有效好友详细信息
    for (int i = 0; i < validCount; i++) {
        cout << "[DEBUG] 发送好友 " << (i+1) << "/" << validCount << endl;
        sendMsg(fd, validFriendInfos[i]);
    }
}

void start_chat(int fd, User &user) {
    Redis redis;
    redis.connect();
    redis.sadd("is_chat", user.getUID());
    string records_index;
    //收历史记录索引
    recvMsg(fd, records_index);
    int num = redis.llen(records_index);
    //发
    sendMsg(fd, to_string(num));



    redisReply **arr = redis.lrange(records_index, "0", to_string(num - 1));
    //先发最新的消息，所以要倒序遍历
    for (int i = num - 1; i >= 0; i--) {
        string msg_content = arr[i]->str;
        try {
            json test_json = json::parse(msg_content);
            //循环发信息
            sendMsg(fd, msg_content);
        } catch (const exception& e) {
            continue;
        }
        freeReplyObject(arr[i]);
    }
    string friend_uid;
    //接收客户端发送的想要聊天的好友的UID
    recvMsg(fd, friend_uid);
    cout << "[DEBUG] 收到消息: " <<  friend_uid << endl;
    //###先进入聊天，再检查删除和屏蔽，待完善
    
    string msg;
    while (true) {
        int ret = recvMsg(fd, msg);
        if (ret <= 0) {
            // 连接断开或接收错误
            redis.srem("is_chat", user.getUID());
            return;
        }

        //客户端退出聊天
        if (msg == EXIT) {
            redis.srem("is_chat", user.getUID());
            return;
        }

 //发文件
        if (msg == SENDFILE_F) {
            sendFile_Friend(fd, user);
            continue;
        }

        if (msg == RECVFILE_F) {
            cout << "[DEBUG] 收到 RECVFILE_F 协议，调用 recvFile_Friend" << endl;
            recvFile_Friend(fd, user);
            continue;
        }

        // 群聊文件传输
        if (msg == SENDFILE_G) {
            cout << "[DEBUG] 收到 SENDFILE_G 协议，调用 sendFile_Group" << endl;
            sendFile_Group(fd, user);
            continue;
        }

        if (msg == RECVFILE_G) {
            cout << "[DEBUG] 收到 RECVFILE_G 协议，调用 recvFile_Group" << endl;
            recvFile_Group(fd, user);
            continue;
        }

        //正常消息
        cout << "[DEBUG] 尝试解析为JSON消息: " << msg << endl;
        Message message;
        try {
            message.json_parse(msg);
        } catch (const exception& e) {
            // JSON解析失败，跳过这条消息
            cout << "[DEBUG] JSON解析失败，跳过消息: " << msg << endl;
            continue;
        }
        string UID = message.getUidTo();
        
        // 检查是否被对方删除,删除和屏蔽的逻辑就是==对方发消息给我，且我在聊天框  UID是对方的uid,我的uid是user.getUID()
        if (!redis.sismember(UID, user.getUID())) {
            // 消息保存到发送者的历史记录中
            string me = user.getUID() + UID;
            redis.lpush(me, msg);

            string receiver_fd_str = redis.hget("unified_receiver", user.getUID());
            int receiver_fd = stoi(receiver_fd_str);
            sendMsg(receiver_fd, "FRIEND_VERIFICATION_NEEDED");
            continue; 
        }
        
        // 检查对方是否屏蔽了我
        if (redis.sismember("blocked" + UID, user.getUID())) {
            string me = user.getUID() + UID;
            redis.lpush(me, msg);

            string receiver_fd_str = redis.hget("unified_receiver", user.getUID());//是给原客户端的通知线程发，不是UID,是GETUID()
            int receiver_fd = stoi(receiver_fd_str);
            sendMsg(receiver_fd, "BLOCKED_MESSAGE");
            continue;
        }
        
        // 检查对方是否已注销
        if (redis.sismember("deactivated_users", UID)) {
            string me = user.getUID() + UID;
            redis.lpush(me, msg);

            string receiver_fd_str = redis.hget("unified_receiver", user.getUID());
            int receiver_fd = stoi(receiver_fd_str);
            sendMsg(receiver_fd, "DEACTIVATED_MESSAGE");
            continue;
        }
        

        //对方不在线
        if (!redis.hexists("is_online", UID)) {
            //历史
            
            string me = message.getUidFrom() + message.getUidTo();
            string her = message.getUidTo() + message.getUidFrom();
            redis.lpush(me, msg);
            redis.lpush(her, msg);

            // 离线
            redis.lpush("off_msg" + UID, message.getUsername());
            cout << "[DEBUG] 用户 " << UID << " 离线，保存消息通知: " << message.getUsername() << endl;
            continue;
        }


        //下列都是在线
        
        // 检查我是否屏蔽了对方（我屏蔽对方，对方的消息我收不到）
        if (!redis.sismember("blocked" + user.getUID(), UID)) {
            // 检查接收方是否在与发送方的聊天中
            bool receiverInChatWithSender = redis.sismember("is_chat", UID);

            cout << "[DEBUG] 尝试发送实时消息给UID: " << UID << ", 接收方在聊天中: " << receiverInChatWithSender << endl;
            if (redis.hexists("unified_receiver", UID)) {
                string receiver_fd_str = redis.hget("unified_receiver", UID);
                cout << "[DEBUG] 找到统一接收连接 fd: " << receiver_fd_str << endl;
                try {
                    int receiver_fd = stoi(receiver_fd_str);

                    if (receiverInChatWithSender) {
                        // 接收方在聊天中，发送JSON消息
                        json test_json = json::parse(msg);
                        sendMsg(receiver_fd, msg);
                        cout << "[DEBUG] JSON消息已发送到聊天窗口" << endl;
                    } else {
                        // 接收方不在聊天中，保存到离线消息并发送通知
                        redis.lpush("off_msg" + UID, message.getUsername());
                        sendMsg(receiver_fd, "MESSAGE:" + message.getUsername());
                        cout << "[DEBUG] 消息已保存到离线消息，通知已发送到主菜单" << endl;
                    }
                } catch (const exception& e) {
                    cout << "[ERROR] 消息格式错误或统一接收连接无效，跳过发送: " << e.what() << endl;
                }
            } else {
                cout << "[DEBUG] 未找到UID " << UID << " 的统一接收连接" << endl;
            }
        }
        
        // 保存消息到历史记录（无论是否发送成功）
        string me = message.getUidFrom() + message.getUidTo();
        string her = message.getUidTo() + message.getUidFrom();
        redis.lpush(me, msg);
        redis.lpush(her, msg);
    }
}



void list_friend(int fd, User &user) {
    Redis redis;
    redis.connect();
    string temp;
    cout << "[DEBUG] listfriend 开始"  << endl;
    //收
    recvMsg(fd, temp);
    int num = stoi(temp);
    string friend_uid;
    for (int i = 0; i < num; ++i) {
        //收
        recvMsg(fd, friend_uid);
        if (redis.hexists("is_online", friend_uid)) {
            //在线发送"1"
            sendMsg(fd, "1");
        } else
            //不在线发送"0"
            sendMsg(fd, "0");
    }
}

void add_friend(int fd, User &user) {
    Redis redis;
    redis.connect();
    string username;

    recvMsg(fd, username);

    if (username == "0") {
        return;
    }
    cout << "[DEBUG] 添加好友请求: " << user.getUsername() << " 想添加 " << username << endl;

    if (!redis.hexists("username_to_uid", username)) {
        cout << "[DEBUG] 用户名不存在于username_to_uid: " << username << endl;
        sendMsg(fd, "-1"); // 用户不存在
        return;
    }
    string UID = redis.hget("username_to_uid", username);
    if (!redis.hexists("user_info", UID)) {
        sendMsg(fd, "-1");
        return;
    } else if (redis.sismember(user.getUID(), UID) && redis.sismember(UID, user.getUID())) {
        sendMsg(fd, "-2"); // 双方都互为好友才算已经是好友
        return;
    } else if (UID == user.getUID()) {
        sendMsg(fd, "-3");
        return;
    }

    // 检查对方是否已注销
    if (redis.sismember("deactivated_users", UID)) {
        sendMsg(fd, "-6"); // 对方已注销，无法添加
        return;
    }

    // 检查是否已经发送过好友申请
    if (redis.sismember(UID + "add_friend", user.getUID())) {
        sendMsg(fd, "-5"); // 已经发送过申请，等待对方处理
        return;
    }

    sendMsg(fd, "1");
    // redis.sadd("add_friend", UID);之前的好友实时通知集合
    redis.sadd(UID + "add_friend", user.getUID());

    // 不管在不在，先设置好友申请通知标记，在的话直接推
    redis.hset("friend_request_notify", UID, "1");


    // 有对应的接收线程在，就是在，如果目标用户在线，立即推送好友申请通知
    if (redis.hexists("unified_receiver", UID)) {
        string receiver_fd_str = redis.hget("unified_receiver", UID);
        int receiver_fd = stoi(receiver_fd_str);
        sendMsg(receiver_fd, REQUEST_NOTIFICATION);
     
        // 清除通知标记，因为已经推送了
        redis.hdel("friend_request_notify", UID);
    }

    string user_info = redis.hget("user_info", UID);
    sendMsg(fd, user_info);
}

void findRequest(int fd, User &user) {
    Redis redis;
    redis.connect();
    //只是一个好友申请缓冲区
    int num = redis.scard(user.getUID() + "add_friend");
    //发送缓冲区申请数量
    sendMsg(fd, to_string(num));
    if (num == 0) {
        return;
    }
    redisReply **arr = redis.smembers(user.getUID() + "add_friend");
    if (arr != nullptr) {
        string request_info;
        User friendRequest;
        for (int i = 0; i < num; i++) {
            request_info = redis.hget("user_info", arr[i]->str);
            friendRequest.json_parse(request_info);

            sendMsg(fd, friendRequest.getUsername());
            string reply;

            recvMsg(fd, reply);
            if (reply == "REFUSED") {
                redis.srem(user.getUID() + "add_friend", arr[i]->str);
                freeReplyObject(arr[i]);
                continue; // 继续处理下一个申请，而不是直接return
            }
            //这里才是真正的好友列表
            //将对方加到我的好友列表中
            redis.sadd(user.getUID(), arr[i]->str);
            //将我加到对方的好友列表中
            redis.sadd(arr[i]->str, user.getUID());
            //将好友申请从缓冲区删除
            redis.srem(user.getUID() + "add_friend", arr[i]->str);

            sendMsg(fd, request_info);
            freeReplyObject(arr[i]);
        }
    }
}

void del_friend(int fd, User &user) {
    Redis redis;
    redis.connect();
    string UID;

    recvMsg(fd, UID);
    //从我的好友列表删除对方（单向删除）
    redis.srem(user.getUID(), UID);
    //删除我对他的历史记录
    redis.del(user.getUID() + UID);
    //删除我对他的屏蔽关系
    redis.srem("blocked" + user.getUID(), UID);
    // 移除通知机制 - 不再发送删除通知
    // redis.sadd(UID + "del", user.getUsername());
}

void blockedLists(int fd, User &user) {
    Redis redis;
    redis.connect();
    User blocked;
    string blocked_uid;
    //接收客户端发送的要屏蔽用户的UID
    recvMsg(fd, blocked_uid);
    redis.sadd("blocked" + user.getUID(), blocked_uid);
}

void unblocked(int fd, User &user) {
    Redis redis;
    redis.connect();
    int num = redis.scard("blocked" + user.getUID());
    //发送屏蔽名单数量
    sendMsg(fd, to_string(num));
    if (num == 0) {
        return;
    }
    redisReply **arr = redis.smembers("blocked" + user.getUID());
    if (arr != nullptr) {
        string blocked_info;
        for (int i = 0; i < num; ++i) {
            blocked_info = redis.hget("user_info", arr[i]->str);
            //循环发送屏蔽名单信息
            sendMsg(fd, blocked_info);
            freeReplyObject(arr[i]);
        }
    }
    //接收解除屏蔽的信息
    string UID;
    recvMsg(fd, UID);
    redis.srem("blocked" + user.getUID(), UID);
}


 void sendFile_Friend(int fd, User &user) {
        Redis redis;
        redis.connect();
        string friend_info;
        //接收发送文件的对象
        User _friend;
        int ret = recvMsg(fd, friend_info);
        if (ret <= 0) {
            cout << "[ERROR] 接收目标用户信息失败" << endl;
            redis.hdel("is_online", user.getUID());
            return;
        }

        cout << "[DEBUG] 接收到目标用户信息: " << friend_info << endl;
        _friend.json_parse(friend_info);
        string filePath, fileName;

        ret = recvMsg(fd, filePath);
        if (ret <= 0) {
            cout << "[ERROR] 接收文件路径失败" << endl;
            redis.hdel("is_online", user.getUID());
            return;
        }

        cout << "[DEBUG] 接收到文件路径: " << filePath << endl;

        // 检查是否取消发送
        if (filePath == "0") {
            cout << "[DEBUG] 用户取消发送文件" << endl;
            return;
        }

        recvMsg(fd, fileName);
        cout << "传输文件名: " << fileName << endl;
        filePath = "./fileBuffer_send/" + fileName;
        //最后一个groupName填了文件名，接收的时候会显示
        Message message(user.getUsername(), user.getUID(), _friend.getUID(), fileName);
        message.setContent(filePath);
        if (!filesystem::exists("./fileBuffer_send")) {
            filesystem::create_directories("./fileBuffer_send");
        }
        ofstream ofs(filePath, ios::binary);
        if (!ofs.is_open()) {
            cerr << "Can't open file" << endl;
            return;
        }
        string ssize;

        recvMsg(fd, ssize);
        off_t size = stoll(ssize);
        off_t originalSize = size;  // 保存原始文件大小
        off_t sum = 0;
        int n;
        char buf[BUFSIZ];
        int progressCounter = 0;  // 进度计数器
        while (size > 0) {
            if (size > sizeof(buf)) {
                n = read_n(fd, buf, sizeof(buf));
            } else {
                n = read_n(fd, buf, size);
            }
            if (n < 0) {
                continue;
            }
            // cout << "剩余文件大小: " << size << endl;  // 调试信息已注释
            size -= n;
            sum += n;
            ofs.write(buf, n);
        }
        //文件发送实时通知缓冲区
        redis.sadd("file" + _friend.getUID(), user.getUsername());
        // 注意：不保存原始消息，因为content是文件路径，稍后会保存格式化的文件消息

        // 创建文件消息并保存到聊天历史记录
        Message fileMessage;
        fileMessage.setUidFrom(user.getUID());
        fileMessage.setUidTo(_friend.getUID());
        fileMessage.setUsername(user.getUsername());
        fileMessage.setContent("[文件] " + fileName);
        fileMessage.setGroupName("1");  // 私聊标识

        // 保存到双方的聊天历史记录
        string senderHistory = user.getUID() + _friend.getUID();
        string receiverHistory = _friend.getUID() + user.getUID();
        string fileMessageJson = fileMessage.to_json();
        redis.lpush(senderHistory, fileMessageJson);
        redis.lpush(receiverHistory, fileMessageJson);

        // 保存到接收方的文件接收队列（使用原始消息，包含文件路径）
        redis.sadd("recv" + _friend.getUID(), message.to_json());

        cout << "[DEBUG] 文件消息已保存到聊天历史记录和接收队列" << endl;

        // 主动推送文件通知给在线用户（使用统一接收连接）
        cout << "[DEBUG] 尝试推送文件通知给用户: " << _friend.getUID() << endl;

        // 检查用户是否在线
        if (!redis.hexists("is_online", _friend.getUID())) {
            // 用户离线，保存文件通知到离线消息
            redis.hset("offline_file_notify", _friend.getUID(), user.getUsername());
            cout << "[DEBUG] 用户离线，保存文件通知到离线消息" << endl;
            ofs.close();
            return;
        }

        if (redis.hexists("unified_receiver", _friend.getUID())) {
            string receiver_fd_str = redis.hget("unified_receiver", _friend.getUID());
            int receiver_fd = stoi(receiver_fd_str);
            cout << "[DEBUG] 推送文件通知到统一接收连接 fd: " << receiver_fd << endl;

            // 检查对方是否在聊天中，并且是否在与发送者的聊天中
            bool inChat = redis.sismember("is_chat", _friend.getUID());
            cout << "[DEBUG] 接收方是否在聊天中: " << inChat << endl;

            if (inChat) {
                // 对方在聊天中，发送聊天格式的文件消息
                Message fileMsg;
                fileMsg.setUidFrom(user.getUID());
                fileMsg.setUidTo(_friend.getUID());
                fileMsg.setUsername(user.getUsername());
                fileMsg.setContent("[文件] " + fileName);
                fileMsg.setGroupName("1");  // 私聊标识
                sendMsg(receiver_fd, fileMsg.to_json());
                cout << "[DEBUG] 发送聊天格式文件消息: [文件] " << fileName << endl;
            } else {
                // 如果不在聊天中，发送普通文件通知
                sendMsg(receiver_fd, "FILE:" + user.getUsername());
                cout << "[DEBUG] 发送普通文件通知" << endl;
            }
        } else {
            cout << "[DEBUG] 未找到用户 " << _friend.getUID() << " 的统一接收连接" << endl;
        }

        ofs.close();

        cout << "[DEBUG] 文件发送完成" << endl;
 }

// 群聊发送文件
void sendFile_Group(int fd, User &user) {
    cout << "[DEBUG] sendFile_Group 开始，用户: " << user.getUsername() << endl;
    Redis redis;
    redis.connect();
    string group_info;

    // 接收群聊信息
    Group group;
    int ret = recvMsg(fd, group_info);
    if (ret <= 0) {
        cout << "[ERROR] 接收群聊信息失败" << endl;
        redis.hdel("is_online", user.getUID());
        return;
    }

    cout << "[DEBUG] 接收到群聊信息: " << group_info << endl;
    group.json_parse(group_info);
    string filePath, fileName;

    ret = recvMsg(fd, filePath);
    if (ret <= 0) {
        cout << "[ERROR] 接收文件路径失败" << endl;
        redis.hdel("is_online", user.getUID());
        return;
    }

    cout << "[DEBUG] 接收到文件路径: " << filePath << endl;

    // 检查是否取消发送
    if (filePath == "0") {
        cout << "[DEBUG] 用户取消发送群聊文件" << endl;
        return;
    }

    recvMsg(fd, fileName);
    cout << "传输群聊文件名: " << fileName << endl;
    filePath = "./fileBuffer_send/" + fileName;

    // 创建群聊文件消息
    Message message(user.getUsername(), user.getUID(), group.getGroupUid(), group.getGroupName());
    message.setContent(filePath);  // 文件路径存储在content中

    if (!filesystem::exists("./fileBuffer_send")) {
        filesystem::create_directories("./fileBuffer_send");
    }

    ofstream ofs(filePath, ios::binary);
    if (!ofs.is_open()) {
        cerr << "Can't open file" << endl;
        return;
    }

    string ssize;
    recvMsg(fd, ssize);
    off_t size = stoll(ssize);
    off_t sum = 0;
    int n;
    char buf[BUFSIZ];

    while (size > 0) {
        if (size > sizeof(buf)) {
            n = read_n(fd, buf, sizeof(buf));
        } else {
            n = read_n(fd, buf, size);
        }
        if (n < 0) {
            continue;
        }
        size -= n;
        sum += n;
        ofs.write(buf, n);
    }

    // 保存文件消息到群聊历史记录
    redis.lpush(group.getGroupUid() + "history", message.to_json());

    // 将文件添加到群聊成员的接收队列
    redisReply **members = redis.smembers(group.getMembers());
    int memberCount = redis.scard(group.getMembers());

    for (int i = 0; i < memberCount; i++) {
        string memberUID = string(members[i]->str);
        if (memberUID != user.getUID()) {  // 不给发送者自己添加
            redis.sadd("recv" + memberUID, message.to_json());
            cout << "[DEBUG] 添加群聊文件到成员 " << memberUID << " 的接收队列" << endl;
        }
        freeReplyObject(members[i]);
    }

    cout << "[DEBUG] 群聊文件发送完成: " << fileName << endl;
    ofs.close();
}

// 群聊接收文件
void recvFile_Group(int fd, User &user) {
    cout << "[DEBUG] recvFile_Group 开始，用户: " << user.getUsername() << endl;
    Redis redis;
    redis.connect();

    // 获取用户的群聊文件数量
    int num = redis.scard("recv" + user.getUID());
    cout << "[DEBUG] 用户 " << user.getUsername() << " 有 " << num << " 个群聊文件待接收" << endl;

    // 发送文件数量
    sendMsg(fd, to_string(num));
    cout << "[DEBUG] 已发送群聊文件数量: " << num << endl;

    if (num == 0) {
        return;
    }

    redisReply **arr = redis.smembers("recv" + user.getUID());
    for (int i = 0; i < num; i++) {
        sendMsg(fd, arr[i]->str);

        Message message;
        message.json_parse(arr[i]->str);
        string path = message.getContent();

        cout << "[DEBUG] 尝试访问群聊文件路径: " << path << endl;

        struct stat info;
        if (stat(path.c_str(), &info) == -1) {
            cout << "[ERROR] 群聊文件不存在或无法访问: " << path << endl;
            cout << "[ERROR] 错误原因: " << strerror(errno) << endl;

            // 跳过这个文件
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        string reply;
        cout << "[DEBUG] 等待客户端回复是否接收群聊文件..." << endl;
        int _ret = recvMsg(fd, reply);
        cout << "[DEBUG] 收到客户端回复: '" << reply << "'" << endl;

        if (_ret <= 0) {
            cout << "[ERROR] 接收客户端回复失败" << endl;
            redis.hdel("is_online", user.getUID());
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        if (reply == "NO") {
            cout << "[DEBUG] 客户端拒绝接收群聊文件" << endl;
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        if (reply != "YES") {
            cout << "[ERROR] 收到意外的回复: " << reply << ", 跳过此群聊文件" << endl;
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        cout << "[DEBUG] 客户端同意接收群聊文件，开始传输" << endl;

        int fp = open(path.c_str(), O_RDONLY);
        if (fp == -1) {
            cout << "[ERROR] 无法打开群聊文件: " << path << endl;
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        sendMsg(fd, to_string(info.st_size));
        off_t ret;
        off_t sum = info.st_size;

        while (true) {
            ret = sendfile(fd, fp, nullptr, info.st_size);
            if (ret == 0) {
                cout << "[DEBUG] 群聊文件传输成功" << endl;
                break;
            } else if (ret > 0) {
                sum -= ret;
            }
        }

        redis.srem("recv" + user.getUID(), arr[i]->str);
        close(fp);
        freeReplyObject(arr[i]);
    }
}

void recvFile_Friend(int fd, User &user) {
    cout << "[DEBUG] recvFile_Friend 开始，用户: " << user.getUsername() << endl;
    Redis redis;
    redis.connect();
    char buf[BUFSIZ];
    int num = redis.scard("recv" + user.getUID());

    cout << "[DEBUG] 用户 " << user.getUsername() << " 有 " << num << " 个文件待接收" << endl;
    sendMsg(fd, to_string(num));
    cout << "[DEBUG] 已发送文件数量: " << num << endl;
    Message message;
    string path;
    if (num == 0) {
        cout << "当前没有要接收的文件" << endl;
        cout << "\033[90m可以继续聊天（输入消息后按enter发送）\033[0m" << endl;

        return;
    }

    redisReply **arr = redis.smembers("recv" + user.getUID());
    for (int i = 0; i < num; i++) {

        sendMsg(fd, arr[i]->str);
        message.json_parse(arr[i]->str);
        path = message.getContent();
        cout << "[DEBUG] 尝试访问文件路径: " << path << endl;

        struct stat info;
        if (stat(path.c_str(), &info) == -1) {
            cout << "[ERROR] 文件不存在或无法访问: " << path << endl;
            cout << "[ERROR] 错误原因: " << strerror(errno) << endl;

            // 尝试检查文件是否存在
            if (filesystem::exists(path)) {
                cout << "[DEBUG] 文件存在但无法stat，可能是权限问题" << endl;
            } else {
                cout << "[DEBUG] 文件不存在" << endl;
            }

            // 不要直接返回，而是跳过这个文件
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }
        string reply;

        cout << "[DEBUG] 等待客户端回复是否接收文件..." << endl;
        int _ret = recvMsg(fd, reply);
        cout << "[DEBUG] 收到客户端回复: '" << reply << "', 长度: " << reply.length() << endl;

        if (_ret <= 0) {
            cout << "[ERROR] 接收客户端回复失败" << endl;
            redis.hdel("is_online", user.getUID());
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        if (reply == "NO") {
            cout << "[DEBUG] 客户端拒绝接收文件" << endl;
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        if (reply != "YES") {
            cout << "[ERROR] 收到意外的回复: " << reply << ", 跳过此文件" << endl;
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        cout << "[DEBUG] 客户端同意接收文件，开始传输" << endl;

        int fp = open(path.c_str(), O_RDONLY);

        sendMsg(fd, to_string(info.st_size));
        off_t ret;
        off_t sum = info.st_size;
        off_t size = 0;
        while (true) {
            ret = sendfile(fd, fp, nullptr, info.st_size);
            if (ret == 0) {
                cout << "文件传输成功" << buf << endl;
                break;
            } else if (ret > 0) {
             //   cout << ret << endl;
                sum -= ret;
                size += ret;
            }
        }
        redis.srem("recv" + user.getUID(), arr[i]->str);
        close(fp);
        freeReplyObject(arr[i]);
    }
}




void send_file(int fd, User &user) {
    Redis redis;
    redis.connect();
    string friend_info;
    //接收发送文件的对象
    User _friend;
    int ret = recvMsg(fd, friend_info);
    if (ret == 0) {
        redis.hdel("is_online", user.getUID());
        return;
    }
    if (friend_info == BACK) {
        return;
    }
    _friend.json_parse(friend_info);
    string filePath, fileName;

    ret = recvMsg(fd, filePath);
    if (ret == 0) {
        redis.hdel("is_online", user.getUID());
    }

    recvMsg(fd, fileName);
    cout << "传输文件名: " << fileName << endl;
    filePath = "./fileBuffer_send/" + fileName;
    //最后一个groupName填了文件名，接收的时候会显示
    Message message(user.getUsername(), user.getUID(), _friend.getUID(), fileName);
    message.setContent(filePath);
    if (!filesystem::exists("./fileBuffer_send")) {
        filesystem::create_directories("./fileBuffer_send");
    }
    ofstream ofs(filePath, ios::binary);
    if (!ofs.is_open()) {
        cerr << "Can't open file" << endl;
        return;
    }
    string ssize;

    recvMsg(fd, ssize);
    off_t size = stoll(ssize);
    off_t originalSize = size;  // 保存原始文件大小
    off_t sum = 0;
    int n;
    char buf[BUFSIZ];
    int progressCounter = 0;  // 进度计数器
    while (size > 0) {
        if (size > sizeof(buf)) {
            n = read_n(fd, buf, sizeof(buf));
        } else {
            n = read_n(fd, buf, size);
        }
        if (n < 0) {
            continue;
        }
        // cout << "剩余文件大小: " << size << endl;  // 调试信息已注释
        size -= n;
        sum += n;
        ofs.write(buf, n);
    }
    //文件发送实时通知缓冲区
    redis.sadd("file" + _friend.getUID(), user.getUsername());
    redis.sadd("recv" + _friend.getUID(), message.to_json());

    // 创建文件消息并保存到聊天历史记录
    Message fileMessage;
    fileMessage.setUidFrom(user.getUID());
    fileMessage.setUidTo(_friend.getUID());
    fileMessage.setUsername(user.getUsername());
    fileMessage.setContent("[文件] " + fileName);
    fileMessage.setGroupName("1");  // 私聊标识

    // 保存到双方的聊天历史记录
    string senderHistory = user.getUID() + _friend.getUID();
    string receiverHistory = _friend.getUID() + user.getUID();
    string fileMessageJson = fileMessage.to_json();
    redis.lpush(senderHistory, fileMessageJson);
    redis.lpush(receiverHistory, fileMessageJson);

    cout << "[DEBUG] 文件消息已保存到聊天历史记录" << endl;

    // 主动推送文件通知给在线用户（使用统一接收连接）
    cout << "[DEBUG] 尝试推送文件通知给用户: " << _friend.getUID() << endl;

    // 检查用户是否在线
    if (!redis.hexists("is_online", _friend.getUID())) {
        // 用户离线，保存文件通知到离线消息
        redis.hset("offline_file_notify", _friend.getUID(), user.getUsername());
        cout << "[DEBUG] 用户离线，保存文件通知到离线消息" << endl;
        return;
    }

    if (redis.hexists("unified_receiver", _friend.getUID())) {
        string receiver_fd_str = redis.hget("unified_receiver", _friend.getUID());
        int receiver_fd = stoi(receiver_fd_str);
        cout << "[DEBUG] 推送文件通知到统一接收连接 fd: " << receiver_fd << endl;

        // 检查对方是否在聊天中，并且是否在与发送者的聊天中
        bool inChat = redis.sismember("is_chat", _friend.getUID());
        cout << "[DEBUG] 接收方是否在聊天中: " << inChat << endl;

        if (inChat) {
            // 对方在聊天中，发送聊天格式的文件消息
            Message fileMsg;
            fileMsg.setUidFrom(user.getUID());
            fileMsg.setUidTo(_friend.getUID());
            fileMsg.setUsername(user.getUsername());
            fileMsg.setContent("[文件] " + fileName);
            fileMsg.setGroupName("1");  // 私聊标识
            sendMsg(receiver_fd, fileMsg.to_json());
            cout << "[DEBUG] 发送聊天格式文件消息: [文件] " << fileName << endl;
        } else {
            // 如果不在聊天中，发送普通文件通知
            sendMsg(receiver_fd, "FILE:" + user.getUsername());
            cout << "[DEBUG] 发送普通文件通知" << endl;
        }
    } else {
        cout << "[DEBUG] 未找到用户 " << _friend.getUID() << " 的统一接收连接" << endl;
    }
    ofs.close();
}

void receive_file(int fd, User &user) {
    Redis redis;
    redis.connect();
    char buf[BUFSIZ];
    int num = redis.scard("recv" + user.getUID());

    sendMsg(fd, to_string(num));
    Message message;
    string path;
    if (num == 0) {
        cout << "当前没有要接收的文件" << endl;
        return;
    }

    redisReply **arr = redis.smembers("recv" + user.getUID());
    for (int i = 0; i < num; i++) {

        sendMsg(fd, arr[i]->str);
        message.json_parse(arr[i]->str);
        path = message.getContent();
        struct stat info;
        if (stat(path.c_str(), &info) == -1) {
            cout << "非法的路径名" << endl;
            cout << path.c_str() << endl;
            return;
        }
        string reply;



        int _ret = recvMsg(fd, reply);
    cout<< "reply:  "<<reply << endl;
        if (_ret == 0) {
            redis.hdel("is_online", user.getUID());
        }
        if (reply == "NO") {
            cout << "拒接接收文件" << endl;
            redis.srem("recv" + user.getUID(), arr[i]->str);
            freeReplyObject(arr[i]);
            continue;
        }

        int fp = open(path.c_str(), O_RDONLY);

        sendMsg(fd, to_string(info.st_size));
        off_t ret;
        off_t sum = info.st_size;
        off_t size = 0;
        while (true) {
            ret = sendfile(fd, fp, nullptr, info.st_size);
            if (ret == 0) {
                cout << "文件传输成功" << buf << endl;
                break;
            } else if (ret > 0) {
             //   cout << ret << endl;
                sum -= ret;
                size += ret;
            }
        }
        redis.srem("recv" + user.getUID(), arr[i]->str);
        close(fp);
        freeReplyObject(arr[i]);
    }
}

// 注销账户
void deactivateAccount(int fd, User &user) {
    cout << "[DEBUG] deactivateAccount 开始，用户: " << user.getUsername() << endl;
    Redis redis;
    redis.connect();
    
    // 将用户添加到注销集合
    redis.sadd("deactivated_users", user.getUID());

    user.setEmail("");


    
    // 从在线状态中移除
    redis.hdel("is_online", user.getUID());
    redis.hdel("unified_receiver", user.getUID());
    redis.srem("is_chat", user.getUID());


    
    cout << "[DEBUG] 用户 " << user.getUsername() << " 已注销" << endl;
   
}
