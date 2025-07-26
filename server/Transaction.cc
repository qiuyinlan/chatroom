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
#include <fstream>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <nlohmann/json.hpp>


using namespace std;
using json = nlohmann::json;

void synchronize(int fd, User &user) {
    Redis redis;
    redis.connect();
    string friend_info;
    int num = redis.scard(user.getUID());

    //发好友数量
    sendMsg(fd, to_string(num));
    redisReply **arr = redis.smembers(user.getUID());
    //发送好友详细信息
    for (int i = 0; i < num; i++) {
        friend_info = redis.hget("user_info", arr[i]->str);
        sendMsg(fd, friend_info);
        freeReplyObject(arr[i]);
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
    
    
    // if (num <= 10) {
    //     //发送历史记录数
    //     sendMsg(fd, to_string(num));
    // } else {
    //     //最多显示x条
    //     num = x;
    //     sendMsg(fd, "x");
    // }

    //发送所有
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
            sendMsg(fd, EXIT);  // 向客户端发送EXIT确认
            redis.srem("is_chat", user.getUID());
            return;
        }

        //正常消息
        Message message;
        try {
            message.json_parse(msg);
        } catch (const exception& e) {
            // JSON解析失败，跳过这条消息
            continue;
        }
        string UID = message.getUidTo();
        
        // 检查是否被对方删除（在发送消息时检查）
        if (!redis.sismember(UID, user.getUID())) {
            // 消息保存到发送者的历史记录中
            string me = user.getUID() + UID;
            redis.lpush(me, msg);

            // 通知发送者需要好友验证
            sendMsg(fd, "FRIEND_VERIFICATION_NEEDED");
            continue;  // 不发送给对方
        }
        
        // 检查对方是否屏蔽了我
        if (redis.sismember("blocked" + UID, user.getUID())) {
            // 发送被拒收提示
            sendMsg(fd, "BLOCKED_MESSAGE");
            continue;
        }
        
        // 正常发送消息的逻辑
        sendMsg(fd, "MESSAGE_SENT");
        
        //对方不在线
        if (!redis.hexists("is_online", UID)) {
            redis.hset("chat", UID, message.getUsername());
            string me = message.getUidFrom() + message.getUidTo();
            string her = message.getUidTo() + message.getUidFrom();
            redis.lpush(me, msg);
            redis.lpush(her, msg);
            continue;
        }
        //对方不在聊天
        if (!redis.sismember("is_chat", UID)) {
            redis.hset("chat", UID, message.getUsername());
            string me = message.getUidFrom() + message.getUidTo();
            string her = message.getUidTo() + message.getUidFrom();
            redis.lpush(me, msg);
            redis.lpush(her, msg);
            continue;
        }
        
        // 检查我是否屏蔽了对方（我屏蔽对方，对方的消息我收不到）
        if (!redis.sismember("blocked" + user.getUID(), UID)) {
            string _fd = redis.hget("is_online", UID);
            if (!_fd.empty()) {
                try {
                    int her_fd = stoi(_fd);
                    // 验证消息格式
                    json test_json = json::parse(msg);
                    sendMsg(her_fd, msg);
                } catch (const exception& e) {
                    cout << "[ERROR] 消息格式错误，跳过发送: " << e.what() << endl;
                }
            }
        }
        
        string me = message.getUidFrom() + message.getUidTo();
        string her = message.getUidTo() + message.getUidFrom();
        redis.lpush(me, msg);
        redis.lpush(her, msg);
    }
}

void history(int fd, User &user) {
    Redis redis;
    redis.connect();
    string UID;
    //接收客户端发送的好友UID查找历史记录
    recvMsg(fd, UID);
    string temp = UID + user.getUID();
    cout << temp << endl;
    int num = redis.llen(temp);
    //发送历史记录数量
    sendMsg(fd, to_string(num));
    redisReply **arr = redis.lrange(temp);
    //倒序遍历，先发送最新的聊天记录
    for (int i = num - 1; i >= 0; i--) {
        //循环发送历史记录
        sendMsg(fd, arr[i]->str);
        freeReplyObject(arr[i]);
    }
}

void list_friend(int fd, User &user) {
    Redis redis;
    redis.connect();
    string temp;

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
    
    // 检查是否被对方屏蔽
    if (redis.sismember("blocked" + UID, user.getUID())) {
        sendMsg(fd, "-4"); // 被屏蔽，无法添加
        return;
    }
    
    sendMsg(fd, "1");
    redis.sadd("add_friend", UID);
    redis.sadd(UID + "add_friend", user.getUID());

    // 设置好友申请通知标记
    redis.hset("friend_request_notify", UID, "1");

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
            return;
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
    string blocked_info;
    for (int i = 0; i < num; ++i) {
        blocked_info = redis.hget("user_info", arr[i]->str);
        //循环发送屏蔽名单信息
        sendMsg(fd, blocked_info);
        freeReplyObject(arr[i]);
    }
    //接收解除屏蔽的信息
    string UID;
    recvMsg(fd, UID);
    redis.srem("blocked" + user.getUID(), UID);
}

void group(int fd, User &user) {
    std::cout << "[DEBUG] group() 函数开始" << std::endl;
    Redis redis;
    redis.connect();
    GroupChat groupChat(fd, user);
    groupChat.sync();
    string choice;
    
    int ret;
    while (true) {
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
            break;
        }
        try {
            int option = stoi(choice);
            std::cout << "[DEBUG] 解析选择为数字: " << option << std::endl;

            if (option == 1) {
                groupChat.startChat();
            } else if (option == 2) {
                groupChat.createGroup();
            } else if (option == 3) {
                groupChat.joinGroup();
            } else if (option == 4) {
                groupChat.groupHistory();
            } else if (option == 5) {
                groupChat.managedGroup();
            } else if (option == 6) {
                groupChat.managedCreatedGroup();
            } else if (option == 7) {
                groupChat.showMembers();
            } else if (option == 8) {
                groupChat.quit();
            } else if (option == 11) {
                groupChat.synchronizeGL(fd,user); // 再次同步群聊信息
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



void send_file(int fd, User &user) {
    Redis redis;
    redis.connect();
    string friend_info;
    //接收发送文件的对象
    User _friend;
    int ret = recvMsg(fd, friend_info);
    if (ret == 0) {
        redis.hdel("is_online", user.getUID());
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
    filePath = "./fileBuffer/" + fileName;
    //最后一个groupName填了文件名，接收的时候会显示
    Message message(user.getUsername(), user.getUID(), _friend.getUID(), fileName);
    message.setContent(filePath);
    if (!filesystem::exists("./fileBuffer")) {
        filesystem::create_directories("./fileBuffer");
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
        cout << "剩余文件大小: " << size << endl;
        size -= n;
        sum += n;
        ofs.write(buf, n);
    }
    //文件发送实时通知缓冲区
    redis.sadd("file" + _friend.getUID(), user.getUsername());
    redis.sadd("recv" + _friend.getUID(), message.to_json());
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
                cout << ret << endl;
                sum -= ret;
                size += ret;
            }
        }
        redis.srem("recv" + user.getUID(), arr[i]->str);
        close(fp);
        freeReplyObject(arr[i]);
    }
}
