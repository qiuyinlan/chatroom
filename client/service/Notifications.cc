#include "Notifications.h"
#include "../utils/proto.h"
#include "../utils/TCP.h"
#include "../utils/IO.h"
#include <iostream>
#include <thread>

using namespace std;


std::atomic<bool> stopNotify{false};

//实时通知
void announce(string UID) {
    int announce_fd = Socket();
    //直接连服务器，不断给服务器发送实时通知处理请求
    Connect(announce_fd, IP, PORT);
    string buf;
    int num;

    while (true) {
        this_thread::sleep_for(chrono::milliseconds(500));  // 0.5秒，保持实时性

        //不停地让服务器进入notify函数
        if (sendMsg(announce_fd, NOTIFY) <= 0) {
            cout << "通知服务连接断开，退出通知线程" << endl;
            break;
        }

        if (sendMsg(announce_fd, UID) <= 0) {
            cout << "通知服务连接断开，退出通知线程" << endl;
            break;
        }

        // 循环接收所有通知消息，直到收到 END 标志
        while (true) {
            if (recvMsg(announce_fd, buf) <= 0) {//一直阻塞，知道收到消息——打印，0.5s后继续发，阻塞
                cout << "通知服务连接断开，退出通知线程" << endl;
                return;
            }

            // if (buf == "STOP") {
            //     return;
            // }


            if (buf == "END") {
                break;  // 结束标志，退出内层循环
            }

            

            // 根据消息类型前缀处理不同的通知
            if (buf == REQUEST_NOTIFICATION) {
                cout << "你收到一条好友添加申请" << endl;
            } else if (buf == GROUP_REQUEST) {
                cout << "你收到一条群聊添加申请" << endl;
            } else if (buf.find("MESSAGE:") == 0) {
                cout << "收到一条来自" << buf.substr(8) << "的消息" << endl;  //群聊和私聊都可以显示
            } else if (buf.find("DELETED:") == 0) {
                cout << "你已经被" << buf.substr(8) << "删除" << endl;
            } else if (buf.find("ADMIN_ADD:") == 0) {
                cout << "你已经被设为" << buf.substr(10) << "的管理员" << endl;
            } else if (buf.find("ADMIN_REMOVE:") == 0) {
                cout << "你已被取消" << buf.substr(13) << "的管理权限" << endl;
            } else if (buf.find("FILE:") == 0) {
                cout << "收到" << buf.substr(5) << "发送的文件" << endl;
            }
            
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

//群聊和私聊接收线程
void chatReceived(int fd, const string&UID) {
    //群/好友uid传进来，用来判断————从而显示
    Message message;
    string json_msg;

        while (true) {
            int ret = recvMsg(fd, json_msg);
            if (ret <= 0) {
                cout << "聊天连接断开，退出聊天接收线程" << endl;
                return;
            }

            // 处理空消息
            if (json_msg.empty()) {
                continue;

            }

            if (json_msg == EXIT) {
                return;
            }

            // 处理特殊响应
            if (json_msg == "FRIEND_VERIFICATION_NEEDED" ||
                json_msg.find("VERIFICATION_NEEDED") != string::npos ||
                json_msg.find("ND_VERIFICATION_NEEDED") != string::npos) {
                cout << YELLOW << "系统提示：对方开启了好友验证，你还不是他（她）朋友，请先发送朋友验证请求，对方验证通过后，才能聊天。" << RESET << endl;
                continue;
            }

            if (json_msg == "BLOCKED_MESSAGE") {
                cout << YELLOW << "系统提示：消息已发出，但被对方拒收了。" << RESET << endl;
                continue;
            }

            if (json_msg == "MESSAGE_SENT") {
                // 消息发送成功，不需要特殊处理
                continue;
            }

            try {
                
                message.json_parse(json_msg);
                //私发
                if (message.getGroupName() == "1") { 
                    if (message.getUidFrom() == UID) { //如果此时我处于和对方一样的聊天框
                        cout << message.getUsername() << ": " << message.getContent() << endl;
                    } else {
                        cout << "收到一条来自" << message.getUsername() << "的一条消息" <<  endl;//对方不在聊天框
                    }

                    continue;
                }
                //群发
                 //### 待完善————如果被踢了，要显示，你无法在被踢的群聊中发送消息，首先先保证解散群聊的时候，群聊还在！！！然后再改groupchat,while循环里面，被解散了不能发消息
                else {
                    if (UID == message.getUidTo()){ //如果我在群聊里（群聊uid==uidto）
                        cout << "[" << message.getGroupName() << "] "
                        << message.getUsername() << ": "
                        << message.getContent() << endl;
                    }else {
                        cout << "收到一条来自" << message.getGroupName() << "的一条消息" <<  endl;
                    }
                }
            } catch (const exception& e) {
                // JSON解析失败，跳过这条消息
                cout << "一条消息解析失败，跳过" << endl;
                cout << "跳过的消息为：" << message.getContent() << endl;
                continue;
            }

    }
}


