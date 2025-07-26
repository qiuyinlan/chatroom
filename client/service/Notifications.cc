#include "Notifications.h"
#include "../utils/proto.h"
#include "../utils/TCP.h"
#include "../utils/IO.h"
#include <iostream>
#include <thread>

using namespace std;

//实时通知
void announce(string UID) {
    int announce_fd = Socket();
    //直接连服务器，不断给服务器发送实时通知处理请求
    Connect(announce_fd, IP, PORT);
    string buf;
    int num;
    // 改进方案：使用消息类型标识，不再依赖固定顺序
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(500));  // 0.5秒，保持实时性

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
            if (recvMsg(announce_fd, buf) <= 0) {
                cout << "通知服务连接断开，退出通知线程" << endl;
                return;
            }

            if (buf == "END") {
                break;  // 结束标志，退出内层循环
            }

            // 根据消息类型前缀处理不同的通知
            if (buf == REQUEST_NOTIFICATION) {
                cout << "您收到一条好友添加申请" << endl;
            } else if (buf == GROUP_REQUEST) {
                cout << "您收到一条群聊添加申请" << endl;
            } else if (buf.find("MESSAGE:") == 0) {
                cout << "收到一条来自" << buf.substr(8) << "的消息" << endl;
            } else if (buf.find("DELETED:") == 0) {
                cout << "您已经被" << buf.substr(8) << "删除" << endl;
            } else if (buf.find("ADMIN_ADD:") == 0) {
                cout << "您已经被设为" << buf.substr(10) << "的管理员" << endl;
            } else if (buf.find("ADMIN_REMOVE:") == 0) {
                cout << "您已被取消" << buf.substr(13) << "的管理权限" << endl;
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

//私聊，接收对方发送的消息
void chatReceived(int fd, string UID) {
    Message message;
    string json_msg;

        while (true) {
            int ret = recvMsg(fd, json_msg);
            if (ret <= 0) {
                cout << "聊天连接断开，退出聊天接收线程" << endl;
                break;
            }

            // 处理空消息
            if (json_msg.empty()) {
                continue;
            }

            if (json_msg == EXIT) {
                break;
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
                //对方私发给我
                if (message.getGroupName() == "1") {
                    //如果此时我处于和对方一样的聊天框
                    if (message.getUidFrom() == UID) {
                        cout << message.getUsername() << ": " << message.getContent() << endl;
                    } else {
                        cout << "\033[1m\033[31m"
                        << "           "
                        << "收到一条来自" << message.getUsername() << "的一条消息"
                        << "\033[0m" << endl;
                    }
                    continue;
                }
                //###群发,逻辑有问题
                if (message.getUidFrom() == UID) {
                    cout << message.getUsername() << ": " << message.getContent() << endl;
                } else {
                    cout << "\033[1m\033[31m"
                         << "           "
                         << "收到一条来自" << message.getUsername() << "的一条消息"
                         << "\033[0m" << endl;
                }
            } catch (const exception& e) {
                // JSON解析失败，跳过这条消息
                continue;
            }

    }
}
//群聊接收线程函数
void groupChatReceived(int fd, const string& groupUid) {
    string buf;
    while (true) {
        int ret = recvMsg(fd, buf);
        if (ret <= 0) {
            cout << "群聊连接断开，退出接收线程" << endl;
            break;
        }

        if (buf == EXIT) {
            break;
        }

        try {
            Message message;
            message.json_parse(buf);

            // 显示群聊消息
            cout << "[" << message.getGroupName() << "] "
                 << message.getUsername() << ": "
                 << message.getContent() << endl;
        } catch (const exception& e) {
            // 跳过无效消息
            continue;
        }
    }
}