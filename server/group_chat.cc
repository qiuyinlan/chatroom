#include "Redis.h"
#include "IO.h"
#include "group_chat.h"
#include "Group.h"
#include "proto.h"
//
// Created by shawn on 23-8-13.
//
using namespace std;

GroupChat::GroupChat(int fd, const User &user) : fd(fd), user(user) {
    joined = "joined" + user.getEmail();
    created = "created" + user.getEmail();
    managed = "managed" + user.getEmail();
}

void GroupChat::sync() {
    Redis redis;
    redis.connect();
    int num = redis.scard(created);

    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers(created);
        for (int i = 0; i < num; i++) {
            string json = redis.hget("group_info", arr[i]->str);

            sendMsg(fd, json);
            freeReplyObject(arr[i]);
        }
    }
    num = redis.scard(managed);

    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers(managed);
        for (int i = 0; i < num; i++) {
            string json = redis.hget("group_info", arr[i]->str);

            sendMsg(fd, json);
            freeReplyObject(arr[i]);
        }
    }
    num = redis.scard(joined);

    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers(joined);
        for (int i = 0; i < num; i++) {
            string json = redis.hget("group_info", arr[i]->str);

            sendMsg(fd, json);
            freeReplyObject(arr[i]);
        }
    }
}

void GroupChat::startChat() {
    Redis redis;
    redis.connect();
    redis.sadd("group_chat", user.getEmail());
    string group_info;
    redisReply **arr;

    recvMsg(fd, group_info);
    Group group;
    group.json_parse(group_info);
    int num = redis.llen(group.getGroupEmail() + "history");
    if (num < 5) {

        sendMsg(fd, to_string(num));
    } else {
        num = 5;

        sendMsg(fd, to_string(num));
    }
    if (num != 0) {
        arr = redis.lrange(group.getGroupEmail() + "history", "0", to_string(num - 1));
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
            sendMsg(fd, EXIT);
            redis.srem("group_chat", user.getEmail());
            if (ret == 0) {
                redis.hdel("is_online", user.getEmail());
            }
            return;
        }
        message.json_parse(msg);
        int len = redis.scard(group.getMembers());
        if (len == 0) {
            return;
        }

        redis.lpush(group.getGroupEmail() + "history", msg);
        message.setEmailTo(group.getGroupEmail());
        arr = redis.smembers(group.getMembers());
        string emailto;
        for (int i = 0; i < len; i++) {
            emailto = arr[i]->str;
            if (emailto == user.getEmail()) {
                freeReplyObject(arr[i]);
                continue;
            }
            if (!redis.hexists("is_online", emailto)) {
                redis.hset("chat", emailto, group.getGroupName());
                freeReplyObject(arr[i]);
                continue;
            }
            if (!redis.sismember("group_chat", emailto)) {
                redis.hset("chat", emailto, group.getGroupName());
                freeReplyObject(arr[i]);
                continue;
            }
            string s_fd = redis.hget("is_online", emailto);
            int _fd = stoi(s_fd);
            sendMsg(_fd, msg);
            freeReplyObject(arr[i]);
        }

    }
}

void GroupChat::createGroup() {
    Redis redis;
    redis.connect();
    string group_info;

    recvMsg(fd, group_info);
    Group group;
    group.json_parse(group_info);
    redis.hset("group_info", group.getGroupEmail(), group_info);
    redis.sadd(joined, group.getGroupEmail());
    redis.sadd(managed, group.getGroupEmail());
    redis.sadd(created, group.getGroupEmail());
    redis.sadd(group.getMembers(), user.getEmail());
    redis.sadd(group.getAdmins(), user.getEmail());
}

void GroupChat::joinGroup() {
    Redis redis;
    redis.connect();
    string groupEmail;
    //接收客户端发送的群聊email
    recvMsg(fd, groupEmail);
    if (!redis.hexists("group_info", groupEmail)) {
        sendMsg(fd, "-1");
        return;
    }
    string json = redis.hget("group_info", groupEmail);
    Group group;
    group.json_parse(json);
    //已经加入该群
    if (redis.sismember(joined, groupEmail)) {
        sendMsg(fd, "-2");
        return;
    }

    sendMsg(fd, "1");
    redis.sadd("if_add" + groupEmail, user.getEmail());
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
    string group_email;

    recvMsg(fd, group_email);
    string group_history = group_email + "history";
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
            redis.hdel("is_online", user.getEmail());
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
    int num = redis.scard("if_add" + group.getGroupEmail());

    sendMsg(fd, to_string(num));
    if (num == 0) {
        return;
    }
    redisReply **arr = redis.smembers("if_add" + group.getGroupEmail());
    string info;
    string choice;
    User member;
    for (int i = 0; i < num; i++) {
        info = redis.hget("user_info", arr[i]->str);
        member.json_parse(info);

        sendMsg(fd, member.getUsername());

        int ret = recvMsg(fd, choice);
        if (ret == 0) {
            redis.hdel("is_online", user.getEmail());
        }
        if (choice == "n") {
            //删除缓冲区
            redis.srem("if_add" + group.getGroupEmail(), member.getEmail());
        } else {
            redis.sadd("joined" + member.getEmail(), group.getGroupEmail());
            redis.sadd(group.getMembers(), member.getEmail());
            //删除缓冲区
            redis.srem("if_add" + group.getGroupEmail(), member.getEmail());
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
    redis.srem(groupChat.joined, group.getGroupEmail());
    redis.srem(groupChat.managed, group.getGroupEmail());
    redis.srem(group.getMembers(), member.getEmail());
    redis.srem(group.getAdmins(), member.getEmail());

    redis.sadd(member.getEmail() + "del", group.getGroupName());
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
            redis.hdel("is_online", user.getEmail());
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
        redis.hdel("is_online", user.getEmail());
    }
    User member;
    member.json_parse(member_choose);
    //选择的已经是管理员了
    if (redis.sismember(group.getAdmins(), member.getEmail())) {
        sendMsg(fd, "-1");
        return;
    }
    redis.sadd(group.getAdmins(), member.getEmail());
    redis.sadd("managed" + member.getEmail(), group.getGroupEmail());
    //加入缓冲区
    redis.sadd("have_managed" + member.getEmail(), group.getGroupName());
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
    redis.srem(group.getAdmins(), admin.getEmail());
    redis.srem("managed" + admin.getEmail(), group.getGroupEmail());
    //缓冲区
    redis.srem("revoked" + admin.getEmail(), group.getGroupName());
}

void GroupChat::deleteGroup(Group &group) {
    Redis redis;
    redis.connect();
    int num = redis.scard(group.getMembers());
    redisReply **arr = redis.smembers(group.getMembers());
    string email;
    for (int i = 0; i < num; i++) {
        email = arr[i]->str;
        redis.srem("joined" + email, group.getGroupEmail());
        redis.srem("created" + email, group.getGroupEmail());
        redis.srem("managed" + email, group.getGroupEmail());
        freeReplyObject(arr[i]);
    }
    redis.del(group.getMembers());
    redis.del(group.getAdmins());
    redis.del(group.getGroupEmail() + "history");
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
    redis.srem("joined" + user.getEmail(), group.getGroupEmail());
    redis.srem("managed" + user.getEmail(), group.getGroupEmail());
    redis.srem(group.getMembers(), user.getEmail());
    redis.srem(group.getAdmins(), user.getEmail());
}
