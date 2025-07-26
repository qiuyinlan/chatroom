#include "Redis.h"
#include "IO.h"
#include "group_chat.h"
#include <nlohmann/json.hpp>
#include "Group.h"
#include "proto.h"

#include <iostream>

using namespace std;


GroupChat::GroupChat(int fd, const User &user) : fd(fd), user(user) {
    joined = "joined" + user.getUID();
    created = "created" + user.getUID();
    //cout << "created是" << created << endl;
    managed = "managed" + user.getUID();
}

void GroupChat::synchronizeGL(int fd, User &user) {
    Redis redis;
    redis.connect();
    string group_info;
    int num = redis.scard(joined);

    //发送群聊数量
    sendMsg(fd, to_string(num));
   
    //发送群info
    if (num != 0) {
        redisReply **arr = redis.smembers(joined);

        for (int i = 0; i < num; i++) {
            // 先检查群是否存在
            if (!redis.hexists("group_info", arr[i]->str)) {
                redis.srem(joined, arr[i]->str);
                continue;
            }

            group_info = redis.hget("group_info", arr[i]->str);
            sendMsg(fd, group_info);
            freeReplyObject(arr[i]);
        }
    }
    else{
        sendMsg(fd, "0");
    }
    
}    

void GroupChat::sync() {
    Redis redis;
    redis.connect();
    int num = redis.scard(created);

    //发送群数量
    int ret = sendMsg(fd, to_string(num));
    if (ret <= 0) {
        std::cerr << "[ERROR] 同步创建群数量sendMsg() 失败" << std::endl;
        return;
    }
   
    
    if(num != 0 ) {
       
        redisReply **arr = redis.smembers(created);
      
        for (int i = 0; i < num; i++) {
            string json = redis.hget("group_info", arr[i]->str);
            //std::cout << "[DEBUG] 发送创建的群信息: " << json << std::endl;
            int ret = sendMsg(fd, json);
            if (ret <= 0) {
              //  std::cerr << "[ERROR] 同步创建的群信息sendMsg() 失败" << std::endl;
                return;
            }
            freeReplyObject(arr[i]);
        }
    }
    num = redis.scard(managed);
    std::cout << "[DEBUG] 管理的群数量: " << num << std::endl;
    // 发送管理的群数量
    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers(managed);
        for (int i = 0; i < num; i++) {
            string json = redis.hget("group_info", arr[i]->str);
            std::cout << "[DEBUG] 发送管理的群信息: " << json << std::endl;
            sendMsg(fd, json);
            freeReplyObject(arr[i]);
        }
    }
    num = redis.scard(joined);
    std::cout << "[DEBUG] 加入的群数量: " << num << std::endl;
    // 发送加入的群数量
    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers(joined);
        for (int i = 0; i < num; i++) {
            string groupId = arr[i]->str;
            std::cout << "[DEBUG] 正在获取群信息，群ID: " << groupId << std::endl;
            string json = redis.hget("group_info", groupId);
            std::cout << "[DEBUG] 获取到的JSON: " << json << std::endl;

            // 在发送前验证JSON格式
            try {
                nlohmann::json testParse = nlohmann::json::parse(json);
                std::cout << "[DEBUG] JSON解析测试成功" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "[ERROR] JSON解析测试失败: " << e.what() << std::endl;
                std::cout << "[ERROR] 问题JSON: " << json << std::endl;
            }

            sendMsg(fd, json);
            freeReplyObject(arr[i]);
        }
    }
    std::cout << "[DEBUG] GroupChat::sync() 完成" << std::endl;
}

void GroupChat::startChat() {
    std::cout << "[DEBUG] GroupChat::startChat() 开始" << std::endl;
    Redis redis;
    redis.connect();
    redis.sadd("group_chat", user.getUID());
    string group_info;
    redisReply **arr;

    std::cout << "[DEBUG] 等待接收群聊信息..." << std::endl;
    recvMsg(fd, group_info);
    std::cout << "[DEBUG] 接收到群聊信息: " << group_info << std::endl;

    Group group;
    try {
        group.json_parse(group_info);
        std::cout << "[DEBUG] 群聊信息解析成功: " << group.getGroupName() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[ERROR] 群聊信息解析失败: " << e.what() << std::endl;
        std::cout << "[ERROR] 问题JSON: " << group_info << std::endl;
        return;
    }
    int num = redis.llen(group.getGroupUid() + "history");
    if (num < 5) {

        sendMsg(fd, to_string(num));
    } else {
        num = 5;

        sendMsg(fd, to_string(num));
    }
    if (num != 0) {
        arr = redis.lrange(group.getGroupUid() + "history", "0", to_string(num - 1));
    }
    for (int i = num - 1; i >= 0; i--) {

        sendMsg(fd, arr[i]->str);
        freeReplyObject(arr[i]);
    }
    string msg;
    Message message;
    while (true) {
        int ret = recvMsg(fd, msg);
        if (msg == EXIT || ret == 0) {
            if (ret == 0) {
                cout << "[DEBUG] 群聊中检测到连接断开" << endl;
            }
            redis.srem("group_chat", user.getUID());
            // 注意：只有在真正连接断开时才删除在线状态
            // 正常退出群聊不应该影响在线状态
            return;
        }
        message.json_parse(msg);
        int len = redis.scard(group.getMembers());
        if (len == 0) {
            return;
        }

        redis.lpush(group.getGroupUid() + "history", msg);
        message.setUidTo(group.getGroupUid());
        arr = redis.smembers(group.getMembers());
        string UIDto;
        for (int i = 0; i < len; i++) {
            UIDto = arr[i]->str;
            if (UIDto == user.getUID()) {
                freeReplyObject(arr[i]);
                continue;
            }
            if (!redis.hexists("is_online", UIDto)) {
                redis.hset("chat", UIDto, group.getGroupName());
                freeReplyObject(arr[i]);
                continue;
            }
            if (!redis.sismember("group_chat", UIDto)) {
                redis.hset("chat", UIDto, group.getGroupName());
                freeReplyObject(arr[i]);
                continue;
            }
            string s_fd = redis.hget("is_online", UIDto);
            int _fd = stoi(s_fd);
            sendMsg(_fd, msg);
            freeReplyObject(arr[i]);
        }

    }
}


// 辅助函数：通过群名查找群UID
string GroupChat::findGroupUidByName(Redis& redis, const string& groupName) {
    // 获取所有群信息
    redisReply* reply = (redisReply*)redisCommand(redis.getContext(), "HGETALL group_info");
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        return "";
    }

    // 遍历所有群信息，查找匹配的群名
    for (size_t i = 0; i < reply->elements; i += 2) {
        string groupUid = reply->element[i]->str;
        string groupInfo = reply->element[i + 1]->str;

        try {
            Group group;
            group.json_parse(groupInfo);
            if (group.getGroupName() == groupName) {
                freeReplyObject(reply);
                return groupUid;
            }
        } catch (const exception& e) {
            // 跳过损坏的群信息
            continue;
        }
    }

    freeReplyObject(reply);
    return "";  // 未找到
}

void GroupChat::createGroup() {
    Redis redis;
    redis.connect();
    string group_info;

    recvMsg(fd, group_info);
    Group group;
    group.json_parse(group_info);
    redis.hset("group_info", group.getGroupUid(), group_info);
    redis.sadd(joined, group.getGroupUid());
    redis.sadd(managed, group.getGroupUid());
    redis.sadd(created, group.getGroupUid());
    redis.sadd(group.getMembers(), user.getUID());
    redis.sadd(group.getAdmins(), user.getUID());
}

void GroupChat::joinGroup() {
    Redis redis;
    redis.connect();
    string groupName;
    //接收客户端发送的群聊名称
    recvMsg(fd, groupName);

    // 通过群名查找群UID
    string groupUid = findGroupUidByName(redis, groupName);
    if (groupUid.empty()) {
        sendMsg(fd, "-1");  // 群不存在
        return;
    }

    string json = redis.hget("group_info", groupUid);
    Group group;
    group.json_parse(json);
    //已经加入该群
    if (redis.sismember(joined, groupUid)) {
        sendMsg(fd, "-2");
        return;
    }

    sendMsg(fd, "1");
    redis.sadd("if_add" + groupUid, user.getUID());
    //群聊实时通知
    int num = redis.scard(group.getAdmins());
    redisReply **arr = redis.smembers(group.getAdmins());
    for (int i = 0; i < num; i++) {
        redis.sadd("add_group", arr[i]->str);
    }
}

void GroupChat::groupHistory() const {
    Redis redis;
    redis.connect();
    string group_uid;

    recvMsg(fd, group_uid);
    string group_history = group_uid + "history";
    int num = redis.llen(group_history);

    sendMsg(fd, to_string(num));
    redisReply **arr = redis.lrange(group_history);
    for (int i = num - 1; i >= 0; i--) {

        sendMsg(fd, arr[i]->str);
        freeReplyObject(arr[i]);
    }
}

void GroupChat::managedGroup() const {
    Redis redis;
    redis.connect();
    Group group;
    string group_info;

    recvMsg(fd, group_info);
    group.json_parse(group_info);
    string choice;
    int ret;
    while (true) {

        ret = recvMsg(fd, choice);
        if (ret == 0) {
            redis.hdel("is_online", user.getUID());
        }
        if (choice == BACK) {
            break;
        }
        if (choice == "1") {
            approve(group);
        } else if (choice == "2") {
            remove(group);
        }
    }
}

void GroupChat::approve(Group &group) const {
    Redis redis;
    redis.connect();
    int num = redis.scard("if_add" + group.getGroupUid());

    sendMsg(fd, to_string(num));
    if (num == 0) {
        return;
    }
    redisReply **arr = redis.smembers("if_add" + group.getGroupUid());
    string info;
    string choice;
    User member;
    for (int i = 0; i < num; i++) {
        info = redis.hget("user_info", arr[i]->str);
        member.json_parse(info);

        sendMsg(fd, member.getUsername());

        int ret = recvMsg(fd, choice);
        if (ret == 0) {
            redis.hdel("is_online", user.getUID());
        }
        if (choice == "n") {
            //删除缓冲区
            redis.srem("if_add" + group.getGroupUid(), member.getUID());
        } else {
            redis.sadd("joined" + member.getUID(), group.getGroupUid());
            redis.sadd(group.getMembers(), member.getUID());
            //删除缓冲区
            redis.srem("if_add" + group.getGroupUid(), member.getUID());
        }
        freeReplyObject(arr[i]);
    }
}

void GroupChat::remove(Group &group) const {
    Redis redis;
    redis.connect();
    int num = redis.scard(group.getMembers());
    //发送群员数量
    sendMsg(fd, to_string(num));
    redisReply **arr = redis.smembers(group.getMembers());
    User member;
    string member_info;
    for (int i = 0; i < num; i++) {
        member_info = redis.hget("user_info", arr[i]->str);
        //发送群员信息
        std::cout << "[SERVER DEBUG] sendMsg to client: " << member_info << std::endl;
        sendMsg(fd, member_info);
        freeReplyObject(arr[i]);
    }
    string remove_info;

    recvMsg(fd, remove_info);
    if (remove_info == "0") {
        return;
    }
    member.json_parse(remove_info);
    GroupChat groupChat(0, member);
    redis.srem(groupChat.joined, group.getGroupUid());
    redis.srem(groupChat.managed, group.getGroupUid());
    redis.srem(group.getMembers(), member.getUID());
    redis.srem(group.getAdmins(), member.getUID());

    redis.sadd(member.getUID() + "del", group.getGroupName());
}

void GroupChat::managedCreatedGroup() const {
    Redis redis;
    redis.connect();
    string group_info;

    recvMsg(fd, group_info);
    Group group;
    group.json_parse(group_info);
    string choice;
    while (true) {

        int ret = recvMsg(fd, choice);
        if (ret == 0) {
            redis.hdel("is_online", user.getUID());
        }
        if (choice == "0") {
            break;
        }
        if (choice == "1") {
            approve(group);
        } else if (choice == "2") {
            revokeAdmin(group);
        } else if (choice == "3") {
            deleteGroup(group);
        }
    }
}

void GroupChat::appointAdmin(Group &group) const {
    Redis redis;
    redis.connect();
    int num = redis.scard(group.getMembers());

    sendMsg(fd, to_string(num));
    string member_info;
    redisReply **arr = redis.smembers(group.getMembers());
    for (int i = 0; i < num; i++) {
        member_info = redis.hget("user_info", arr[i]->str);

        sendMsg(fd, member_info);
        freeReplyObject(arr[i]);
    }
    string member_choose;

    int ret = recvMsg(fd, member_choose);
    if (ret == 0) {
        redis.hdel("is_online", user.getUID());
    }
    User member;
    member.json_parse(member_choose);
    //选择的已经是管理员了
    if (redis.sismember(group.getAdmins(), member.getUID())) {
        sendMsg(fd, "-1");
        return;
    }
    redis.sadd(group.getAdmins(), member.getUID());
    redis.sadd("managed" + member.getUID(), group.getGroupUid());
    //加入缓冲区
    redis.sadd("have_managed" + member.getUID(), group.getGroupName());
    sendMsg(fd, "1");
}

void GroupChat::revokeAdmin(Group &group) const {
    Redis redis;
    redis.connect();
    int num = redis.scard(group.getAdmins());

    sendMsg(fd, to_string(num));
    redisReply **arr = redis.smembers(group.getAdmins());
    string admin_info;
    for (int i = 0; i < num; i++) {
        admin_info = redis.hget("user_info", arr[i]->str);

        sendMsg(fd, admin_info);
        freeReplyObject(arr[i]);
    }
    User admin;

    recvMsg(fd, admin_info);
    admin.json_parse(admin_info);
    redis.srem(group.getAdmins(), admin.getUID());
    redis.srem("managed" + admin.getUID(), group.getGroupUid());
    //缓冲区
    redis.srem("revoked" + admin.getUID(), group.getGroupName());
}

void GroupChat::deleteGroup(Group &group) {
    Redis redis;
    redis.connect();
    int num = redis.scard(group.getMembers());
    redisReply **arr = redis.smembers(group.getMembers());
    string UID;
    for (int i = 0; i < num; i++) {
        UID = arr[i]->str;
        redis.srem("joined" + UID, group.getGroupUid());
        redis.srem("created" + UID, group.getGroupUid());
        redis.srem("managed" + UID, group.getGroupUid());
        freeReplyObject(arr[i]);
    }
    redis.del(group.getMembers());
    redis.del(group.getAdmins());
    redis.del(group.getGroupUid() + "history");
}

void GroupChat::showMembers() const {
    Redis redis;
    redis.connect();
    string group_info;
    Group group;

    recvMsg(fd, group_info);
    group.json_parse(group_info);
    int num = redis.scard(group.getMembers());

    sendMsg(fd, to_string(num));
    redisReply **arr = redis.smembers(group.getMembers());
    User member;
    string member_info;
    for (int i = 0; i < num; i++) {
        member_info = redis.hget("user_info", arr[i]->str);
        member.json_parse(member_info);

        sendMsg(fd, member.getUsername());
        freeReplyObject(arr[i]);
    }
}

void GroupChat::quit() {
    Redis redis;
    redis.connect();
    Group group;
    string group_info;

    recvMsg(fd, group_info);
    group.json_parse(group_info);
    redis.srem("joined" + user.getUID(), group.getGroupUid());
    redis.srem("managed" + user.getUID(), group.getGroupUid());
    redis.srem(group.getMembers(), user.getUID());
    redis.srem(group.getAdmins(), user.getUID());
}
