#include "LoginHandler.h"
#include "Transaction.h"
#include <functional>
#include <sys/epoll.h>
#include "../utils/IO.h"
#include "../utils/proto.h"
#include "Redis.h"
#include <iostream>
#include <map>
#include <unistd.h>
#include <curl/curl.h>
#include <string>
#include <nlohmann/json.hpp>
#include <random>
#include <ctime>

using namespace std;
using json = nlohmann::json;

void serverLogin(int epfd, int fd) {
    struct epoll_event temp;
    temp.data.fd = fd;
    temp.events = EPOLLIN;
    User user;
    Redis redis;
    redis.connect();
    string request;
    //接收用户发送的邮箱和密码
    recvMsg(fd, request);
    LoginRequest loginRequest;
    loginRequest.json_parse(request);
    //得到用户发送的邮箱和密码
    string email = loginRequest.getUID();
    string password = loginRequest.getPassword();

    if (!redis.hexists("email_to_uid", email)) {
        sendMsg(fd, "-1"); // 账号不存在
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    string UID = redis.hget("email_to_uid", email);
    string user_info = redis.hget("user_info", UID);
    user.json_parse(user_info);
    if (password != user.getPassword()) {
        sendMsg(fd, "-2"); // 密码错误
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    //用户已经登录
    if (redis.hexists("is_online", UID)) {
        sendMsg(fd, "-3");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    //登录成功
    sendMsg(fd, "1");
    redis.hset("is_online", UID, to_string(fd));
    //发送从数据库获取的用户信息
    sendMsg(fd, user_info);
    serverOperation(fd, user);
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
}

void serverOperation(int fd, User &user) {
    map<int, function<void(int fd, User &user)>> LoginOperation = {
            {4,  start_chat},
            {5,  history},
            {6,  list_friend},
            {7,  add_friend},
            {8,  findRequest},
            {9,  del_friend},
            {10, blockedLists},
            {11, unblocked},
            {12, group},
            {13, send_file},
            {14, receive_file},
            //改协议后，同步出错
            {17, synchronize}
    };
    Redis redis;
    redis.connect();
    int friend_num = redis.scard(user.getUID());
    //发送好友个数
    sendMsg(fd, to_string(friend_num));
    redisReply **arr = redis.smembers(user.getUID());
    for (int i = 0; i < friend_num; i++) {
        string friend_info = redis.hget("user_info", arr[i]->str);
        //循环发送好友信息
        sendMsg(fd, friend_info);
    }
    string temp;
    int ret;
    while (true) {
        //接收用户输入的操作
        //important: ret 看来是必要的 可以有效的删掉在线用户，防止虚空在线
        ret = recvMsg(fd, temp);
        if (temp == BACK || ret == 0) {
            break;
        }
        int option = stoi(temp);
        if (LoginOperation.find(option) == LoginOperation.end()) {
            cout << "没有这个选项，请重新输入" << endl;
            continue;
        }
        LoginOperation[option](fd, user);
    }
    close(fd);
    //只用在这里删除就行了
    redis.hdel("is_online", user.getUID());
}

void notify(int fd) {
    Redis redis;
    redis.connect();
    string UID;

    int ret = recvMsg(fd, UID);
    if (ret == 0) {
        redis.hdel("is_online", UID);
    }
    //判断是否有好友添加
    if (redis.sismember("add_friend", UID)) {

        sendMsg(fd, REQUEST_NOTIFICATION);
        redis.srem("add_friend", UID);
    } else {

        sendMsg(fd, "NO");
    }
    //判断是否有加群
    if (redis.sismember("add_group", UID)) {

        sendMsg(fd, GROUP_REQUEST);
        redis.srem("add_group", UID);
    } else {

        sendMsg(fd, "NO");
    }
    //判断是否有消息
    if (redis.hexists("chat", UID)) {

        sendMsg(fd, redis.hget("chat", UID));
        //bug hdel写的有问题，返回的一直是"0",key和field之间没有加空格,并且hdel还写错了
        redis.hdel("chat", UID);
    } else {

        sendMsg(fd, "NO");
    }
    //被删除好友
    int num = redis.scard(UID + "del");

    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers(UID + "del");
        for (int i = 0; i < num; i++) {

            sendMsg(fd, arr[i]->str);
            redis.srem(UID + "del", arr[i]->str);
            freeReplyObject(arr[i]);
        }
    }
    //被设置为管理员
    num = redis.scard("appoint_admin" + UID);

    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers("appoint_admin" + UID);
        for (int i = 0; i < num; i++) {

            sendMsg(fd, arr[i]->str);
            redis.srem("appoint_admin" + UID, arr[i]->str);
            freeReplyObject(arr[i]);
        }
    }
    //被取消管理员权限
    num = redis.scard("revoke_admin" + UID);

    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers("revoke_admin" + UID);
        for (int i = 0; i < num; i++) {

            sendMsg(fd, arr[i]->str);
            redis.srem("revoke_admin" + UID, arr[i]->str);
            freeReplyObject(arr[i]);
        }
    }
    //文件消息提醒
    num = redis.scard("file" + UID);

    sendMsg(fd, to_string(num));
    if (num != 0) {
        redisReply **arr = redis.smembers("file" + UID);
        for (int i = 0; i < num; i++) {

            sendMsg(fd, arr[i]->str);
            redis.srem("file" + UID, arr[i]->str);
            freeReplyObject(arr[i]);
        }
    }
}

std::string generateCode() {
    // 取当前时间戳的后两位
    time_t timer;
    time(&timer);
    std::string timeStamp = std::to_string(timer).substr(std::to_string(timer).size() - 2, 2);
    // 生成2位随机数
    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<> dist(10, 99);
    int random_num = dist(eng);
    // 拼接成4位验证码
    return timeStamp + std::to_string(random_num);
}

#include <iostream>
#include <string>
#include <curl/curl.h>

// 定义上传状态结构体
struct upload_status {
    size_t bytes_read;
    const std::string* payload;
};

// 自定义 payload_source 回调函数
static size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp) {
    struct upload_status *upload_ctx = (struct upload_status *)userp;
    const std::string& payload = *(upload_ctx->payload);
    size_t left = payload.size() - upload_ctx->bytes_read;
    size_t len = size * nmemb;
    if (len > left) {
        len = left;
    }
    if (len) {
        memcpy(ptr, payload.data() + upload_ctx->bytes_read, len);
        upload_ctx->bytes_read += len;
    }
    return len;
}

bool sendMail(const std::string& to_email, const std::string& code, bool is_find) {
    CURL *curl;
    CURLcode res = CURLE_OK;
    curl = curl_easy_init();
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.163.com:465");
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_USERNAME, "17750601137@163.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "GAWKe3AaL7bEV64e");
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, "17750601137@163.com");
        struct curl_slist *recipients = nullptr;
        recipients = curl_slist_append(recipients, to_email.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        // 构建更详细的邮件内容
        std::string from = "From: 17750601137@163.com\r\n";
        std::string to = "To: " + to_email + "\r\n";
        std::string subject = "Subject: 验证码\r\n";
        std::string mimeVersion = "MIME-Version: 1.0\r\n";
        std::string contentType = "Content-Type: text/plain; charset=utf-8\r\n";
        std::string body = "您好，\r\n\r\n您的验证码是：" + code + "\r\n请在 5 分钟内使用此验证码完成验证。\r\n\r\n感谢您的使用！";

        std::string full_mail_payload = from + to + subject + mimeVersion + contentType + "\r\n" + body;

        upload_status up_status = { 0, &full_mail_payload };
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &up_status);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, (long)full_mail_payload.length());

        res = curl_easy_perform(curl);
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);

        return res == CURLE_OK;
    }

    return false;
}

void handleRequestCode(int epfd, int fd) {
    Redis redis;
    redis.connect();
    struct epoll_event temp;
    temp.data.fd = fd;
    temp.events = EPOLLIN;
    // 1. 接收邮箱

   string email;
   int ret2;
    ret2 = recvMsg(fd, email);
   
   
    // 2. 生成验证码
    string code = generateCode();
    // 3. 存到Redis，5分钟有效

    redis.hset("verify_code", email, code);
    // 4. 发邮件
    bool ok = sendMail(email, code, false);
    // 5. 通知客户端
    if (ok) {
        sendMsg(fd, "验证码已发送，请查收邮箱");
    } else {
        sendMsg(fd, "验证码发送失败，请稍后重试");
    }
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
}

void handleResetCode(int epfd, int fd) {
    // 1. 接收邮箱
    string email;
    recvMsg(fd, email);
    // 2. 生成验证码
    string code = generateCode();
    // 3. 存到Redis，5分钟有效
    Redis redis;
    redis.connect();
    redis.hset("reset_code", email, code);
    // 4. 发邮件
    bool ok = sendMail(email, code, true);
    // 5. 通知客户端
    if (ok) {
        sendMsg(fd, "验证码已发送，请查收邮箱");
    } else {
        sendMsg(fd, "验证码发送失败，请稍后重试");
    }
    struct epoll_event temp;
    temp.data.fd = fd;
    temp.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
}

void serverRegisterWithCode(int epfd, int fd) {
    struct epoll_event temp;
    temp.data.fd = fd;
    temp.events = EPOLLIN;
    string json_str;

    Redis redis;

    string email;
    int ret;
    while (true) {
        ret = recvMsg(fd, email);
        std::cout << "[SERVER] 收到邮箱输入: " << email << std::endl;
        if (ret == 0) { 
            std::cout << "[SERVER] 客户端断开，关闭fd" << std::endl;
            close(fd);
            return;
        }
        if (email == "0") { 
            std::cout << "[SERVER] 用户选择返回注册菜单" << std::endl;
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
            return;   
        }
        if (!redis.hexists("email_to_uid", email)) {
            std::cout << "[SERVER] 邮箱未注册，进入后续流程" << std::endl;
            break;
        }
        std::cout << "[SERVER] 邮箱已注册，发送提示" << std::endl;
        sendMsg(fd, "该邮箱已注册，请重新输入邮箱（输入0返回）：");
    }
    ret = recvMsg(fd, json_str);
    if (ret <= 0) {
        sendMsg(fd, "接收注册信息失败");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }

    json root;
    try {
        root = json::parse(json_str);
    } catch (const json::exception& e) {
        sendMsg(fd, "JSON 格式错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }

     email = root["email"].get<string>();
    string code = root["code"].get<string>();
    string username = root["username"].get<string>();
    string password = root["password"].get<string>();

    if (!redis.connect()) {
        sendMsg(fd, "服务器内部错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }

    // 验证验证码
    string real_code = redis.hget("verify_code", email);
    if (real_code != code) {
        sendMsg(fd, "验证码错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    // 检查用户名是否已注册，若已注册则循环让客户端重新输入
    int ret2;
    while (true) {
        ret2 = recvMsg(fd, email);
        if (ret2 == 0) { // 客户端断开连接
            close(fd);
        return;
    }
        if (email == "0") { // 用户选择返回
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
        }
        if (!redis.hexists("username_to_uid", email)) {
            break; // 未注册，可以继续后续流程
        }
        sendMsg(fd, "该用户名已注册，请重新输入（输入0返回）：");
        // 循环，等待客户端输入新邮箱
    }

    // 生成UID并创建User对象
    User user;
    user.setEmail(email);
    user.setUsername(username);
    user.setPassword(password);
    // UID自动生成

    string user_info = user.to_json();
    if (!redis.hset("user_info", user.getUID(), user_info)) {
        sendMsg(fd, "服务器内部错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    if (!redis.hset("email_to_uid", email, user.getUID())) {
        sendMsg(fd, "服务器内部错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    if (!redis.hset("username_to_uid", username, user.getUID())) {
        sendMsg(fd, "服务器内部错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    if (!redis.sadd("all_uid", user.getUID())) {
        sendMsg(fd, "服务器内部错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    sendMsg(fd, "注册成功");
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
}

void resetPasswordWithCode(int epfd, int fd) {
    struct epoll_event temp;
    temp.data.fd = fd;
    temp.events = EPOLLIN;
    string json_str;
    recvMsg(fd, json_str);
    json root = json::parse(json_str);
    string email = root["email"].get<string>();
    string code = root["code"].get<string>();
    string password = root["password"].get<string>();
    Redis redis;
    redis.connect();
    string real_code = redis.hget("reset_code", email);
    if (real_code != code) {
        sendMsg(fd, "验证码错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    // 检查用户是否存在
    if (!redis.hexists("email_to_uid", email)) {
        sendMsg(fd, "该邮箱未注册");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    string UID = redis.hget("email_to_uid", email);
    string user_info = redis.hget("user_info", UID);
    User user;
    user.json_parse(user_info);
    user.setPassword(password);
    redis.hset("user_info", UID, user.to_json());
    sendMsg(fd, "密码重置成功");
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
}

void findPasswordWithCode(int epfd, int fd) {
    struct epoll_event temp;
    temp.data.fd = fd;
    temp.events = EPOLLIN;
    string json_str;
    recvMsg(fd, json_str);
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::exception& e) {
        sendMsg(fd, "JSON 格式错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    string email = root["email"].get<string>();
    string code = root["code"].get<string>();
    Redis redis;
    redis.connect();
    string real_code = redis.hget("reset_code", email);
    if (real_code != code) {
        sendMsg(fd, "验证码错误");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    // 检查用户是否存在
    if (!redis.hexists("email_to_uid", email)) {
        sendMsg(fd, "该邮箱未注册");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    string UID = redis.hget("email_to_uid", email);
    string user_info = redis.hget("user_info", UID);
    nlohmann::json user_json;
    try {
        user_json = nlohmann::json::parse(user_info);
    } catch (const nlohmann::json::exception& e) {
        sendMsg(fd, "用户信息损坏");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    if (!user_json.contains("password")) {
        sendMsg(fd, "未找到密码信息");
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
        return;
    }
    string password = user_json["password"].get<string>();
    sendMsg(fd, "您的密码是: " + password);
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
}

