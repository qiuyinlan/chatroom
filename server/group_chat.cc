#include "Redis.h"
#include "IO.h"
#include "group_chat.h"
#include <nlohmann/json.hpp>
#include "Group.h"
#include "proto.h"
#include "Transaction.h"
#include "tools.h"

#include <iostream>

using namespace std;


void GroupChat::group(int fd, User &user) {
    std::cout << "[DEBUG] group() 函数开始" << std::endl;
    Redis redis;
    redis.connect();
    GroupChat groupChat(fd, user);
  
    string choice;
    
    int ret;
    while (true) {
        groupChat.sync();
        std::cout << "[DEBUG] 等待接收客户端选择..." << std::endl;
        ret = recvMsg(fd, choice);
        std::cout << "[DEBUG] 接收到客户端选择: '" << choice << "', ret=" << ret << std::endl;

        if (ret == 0) {
            std::cout << "[DEBUG] 客户端断开连接" << std::endl;
            redis.hdel("is_online", user.getUID());
            break;
        }
        if (choice == BACK) {
            std::cout << "[DEBUG] 客户端选择退出" << std::endl;
            return;
        }
        try {
            int option = stoi(choice);
            std::cout << "[DEBUG] 解析选择为数字: " << option << std::endl;
             
            if (option == 1) {
                groupChat.createGroup();
                continue;
            } else if (option == 2) {
                groupChat.joinGroup();
                continue;
            }  else if (option == 3) {
                groupChat.managedGroup();
                continue;
            } else if (option == 4) {
                groupChat.showMembers();
                continue;
            } else if (option == 5) {
                groupChat.quit();
                continue;
            } else {
                // 处理无效选项，这里选择继续循环等待有效输入
                std::cout << "[DEBUG] 无效选择: " << option << "，继续等待有效输入" << std::endl;
                continue;
            }
        } catch (const std::exception& e) {
            std::cout << "[ERROR] 解析选择失败: " << e.what() << std::endl;
            std::cout << "[ERROR] 选择内容: '" << choice << "'" << std::endl;
            // 如果解析失败，通常意味着输入格式错误，这里选择断开连接
            break;
        }
    }
    std::cout << "[DEBUG] group() 函数结束" << std::endl;
}


GroupChat::GroupChat(int fd, const User &user) : fd(fd), user(user) {
    joined = "joined" + user.getUID();
    created = "created" + user.getUID();
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
        if (arr != nullptr) {
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
    }
   cout << "synchronizeGL同步群聊列表结束" << endl;
    
}    

//同步群数量
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
    int ret;
    recvMsg(fd, group_info);
    
    Group group;
    try {
        group.json_parse(group_info);
        
    } catch (const std::exception& e) {
        std::cout << "[ERROR] 群聊信息解析失败: " << e.what() << std::endl;
        std::cout << "[ERROR] 问题JSON: " << group_info << std::endl;
        return;
    }
    int num = redis.llen(group.getGroupUid() + "history");
    
    sendMsg(fd, to_string(num));
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
         ret = recvMsg(fd, msg);
        if (msg == EXIT || ret == 0) {

            if (ret == 0) {
                cout << "[DEBUG] 群聊中检测到连接断开" << endl;
                if (!c_break(ret, fd, user)) {
                    return;
                }
            }
            redis.srem("group_chat", user.getUID());

            // 注意：只有在真正连接断开时才删除在线状态

            return;
        }

        // 处理群聊文件传输协议
        // if (msg == SENDFILE_G) {
        //     cout << "[DEBUG] 群聊中收到 SENDFILE_G 协议" << endl;
        //     sendFile_Group(fd, user);
        //     continue;
        // }

        // if (msg == RECVFILE_G) {
        //     cout << "[DEBUG] 群聊中收到 RECVFILE_G 协议" << endl;
        //     recvFile_Group(fd, user);
        //     continue;
        // }

        // 尝试解析为JSON消息
        try {
            message.json_parse(msg);
        } catch (const exception& e) {
            cout << "[ERROR] 群聊消息JSON解析失败: " << e.what() << endl;
            cout << "[ERROR] 原始消息: " << msg << endl;
            continue;  // 跳过无效消息，继续处理
        }
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
            //不在线，就把用户和群聊名存到chat表里
            if (!redis.hexists("is_online", UIDto)) {
                redis.hset("chat", UIDto, group.getGroupName());
                // 同时保存到离线消息队列
                redis.lpush("off_msg" + UIDto, group.getGroupName());
                freeReplyObject(arr[i]);
                continue;
            }
            //不在群聊中，发送通知
            if (!redis.sismember("group_chat", UIDto)) {
                // 保存到离线消息队列
                redis.lpush("off_msg" + UIDto, group.getGroupName());
                // 使用统一接收连接发送通知
                if (redis.hexists("unified_receiver", UIDto)) {
                    string receiver_fd_str = redis.hget("unified_receiver", UIDto);
                    int receiver_fd = stoi(receiver_fd_str);
                    sendMsg(receiver_fd, "MESSAGE:" + group.getGroupName());
                }
                freeReplyObject(arr[i]);
                continue;
            }
            // 在群聊中，使用统一接收连接发送实时消息
            if (redis.hexists("unified_receiver", UIDto)) {
                string receiver_fd_str = redis.hget("unified_receiver", UIDto);
                int receiver_fd = stoi(receiver_fd_str);
                sendMsg(receiver_fd, msg);
            }
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
    string group_info,groupName;
    int ret;
    while(true){
       ret = recvMsg(fd, groupName);  
       
        if (groupName == BACK) {
            return;
        }
        if (redis.sismember("group_Name", groupName)) {
            sendMsg(fd, "已存在");
            continue;
        }else {
            sendMsg(fd,"0");
            break;
        }
    }
    

    recvMsg(fd, group_info);
    Group group;
    group.json_parse(group_info);
    redis.hset("group_info", group.getGroupUid(), group_info);
    redis.sadd(joined, group.getGroupUid());
    redis.sadd(managed, group.getGroupUid());
    redis.sadd(created, group.getGroupUid());
    redis.sadd(group.getMembers(), user.getUID());
    redis.sadd(group.getAdmins(), user.getUID());

    //名字不重复
    redis.sadd("group_Name", groupName);
}

void GroupChat::joinGroup() {
    Redis redis;
    redis.connect();
    string groupName;
    int ret;
    //接收客户端发送的群聊名称
    recvMsg(fd, groupName);
    
cout << "收到客户端发送的群聊名称" << groupName << endl;
    if (groupName == BACK) {
        return;
    }
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
     if (arr != nullptr) {
        for (int i = 0; i < num; i++) {
            string adminUID = arr[i]->str;//遍历每个管理
           
     cout << "管理员UID" << adminUID << endl;
            redis.sadd("add_group", adminUID);
            redis.hset("group_request_info", adminUID, group.getGroupName());

            // 如果管理员在线，立即推送群聊申请通知
            if (redis.hexists("unified_receiver", adminUID)) {
                string receiver_fd_str = redis.hget("unified_receiver", adminUID);
                int receiver_fd = stoi(receiver_fd_str);
                sendMsg(receiver_fd, "GROUP_REQUEST:" + group.getGroupName());
                cout << "[DEBUG] 已推送群聊申请通知给在线管理员: " << adminUID << "，群聊: " << group.getGroupName() << endl;
            }
            else{
                cout << "buzaixian" << endl;
            }

            freeReplyObject(arr[i]);
         }
    }
}


void GroupChat::managedGroup() const {
    Redis redis;
    redis.connect();
    Group group;
    string group_info;

    recvMsg(fd, group_info);
    if (group_info == BACK) {
        return;
    }
    group.json_parse(group_info);
    string choice;
    int ret;
    while (true) {
        ret = recvMsg(fd, choice);
    cout << "managedGroup() choice=" << choice << endl;
        if (ret == 0) {
               redis.hdel("is_online", user.getUID());
              return;
        }
        if (choice == BACK) {
               return;
        }
        if (choice == "1") {
                approve(group);
                return;
        } else if (choice == "2") {
                 remove(group);
                 return;
        } else if (choice == "3") {
             //设置管理员
                appointAdmin(group);
                return;
        } else if (choice == "4") {
             //撤销管理员
               revokeAdmin(group);
                return ;
           
        } else if (choice == "5") {
           //解散群聊
                deleteGroup(group);
                return;
            
        }
    }
}

void GroupChat::approve(Group &group) const {
    Redis redis;
    redis.connect();
    int num = redis.scard("if_add" + group.getGroupUid());

    sendMsg(fd, to_string(num));
    if (num == 0) {
    cout << "暂无入群申请" << endl;
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
    int ret;
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
    if (group_info == BACK) {
        return;
    }
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
    if (group_info == BACK) {
        return;
    }
    group.json_parse(group_info);
    redis.srem("joined" + user.getUID(), group.getGroupUid());
    redis.srem("managed" + user.getUID(), group.getGroupUid());
    redis.srem(group.getMembers(), user.getUID());
    redis.srem(group.getAdmins(), user.getUID());
}
