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
        //this_thread::sleep_for(chrono::seconds(3));

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

//私聊群聊，接收对方发送的消息
void chatReceived(int fd, string UID) {
    Message message;
    string json_msg;
    
        while (true) {
            int ret = recvMsg(fd, json_msg);
            if (ret <= 0) {
                cout << "聊天连接断开，退出聊天接收线程" << endl;
                break;
            }
    
            if (json_msg == EXIT) {
                break;
            }
    
            try {
                // 添加调试信息
                cout << "[DEBUG] 接收到的 JSON: " << json_msg << endl;
                cout << "[DEBUG] JSON 长度: " << json_msg.length() << endl;
    
                message.json_parse(json_msg);
                //私发
                if (message.getGroupName() == "1") {
                    if (message.getUidFrom() == UID) {
                        cout << message.getUsername() << ": " << message.getContent() << endl;
                    } else {
                        cout << "\t\t\t\t\t\t\t\t" << RED << "收到一条来自" << message.getUsername() << "的一条消息" << RESET
                             << endl;
                    }
                    continue;
                }
                //群发
                if (message.getUidFrom() == UID) {
                    cout << message.getUsername() << ": " << message.getContent() << endl;
                } else {
                    cout << "\033[1m\033[31m"
                         << "           "
                         << "收到一条来自" << message.getUsername() << "的一条消息"
                         << "\033[0m" << endl;
                    //cout << "收到一条来自" << message.getUsername() << "的一条消息" << endl;
                }
            } catch (const exception& e) {
                cout << "[ERROR] 解析消息失败: " << e.what() << endl;
                cout << "[ERROR] 问题 JSON: " << json_msg << endl;
                cout << "[ERROR] JSON 长度: " << json_msg.length() << endl;
                // 显示 JSON 的十六进制表示
                cout << "[ERROR] JSON 十六进制: ";
                for (char c : json_msg) {
                    printf("%02x ", (unsigned char)c);
                }
                cout << endl;
                continue;
            }
    
    }
}