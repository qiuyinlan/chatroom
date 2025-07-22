
#include "IO.h"
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <string>
#include "Redis.h"

using namespace std;

//通用返回按钮
void return_last(){
    std::cout << "\033[90m输入【0】返回\033[0m" << std::endl;
}
int read_menu_option(int min, int max, const std::string& prompt);



int write_n (int fd, const char *msg, int n) {
    int n_written;
    int n_left = n;
    const char *ptr = msg;
    while (n_left > 0) {
        if ((n_written = send(fd, ptr, n_left, 0)) < 0) {
            if (n_written < 0 && errno == EINTR)
                continue;
            else
                return -1;
        } else if (n_written == 0) {
            continue;
        }
        ptr += n_written;
        n_left -= n_written;
    }
    return n;
}

int sendMsg(int fd, string msg) {
    if (fd < 0 || msg.empty()) {
        cout << "[ERROR] sendMsg: 无效参数 fd=" << fd << ", msg长度=" << msg.size() << endl;
        return -1;
    }

    // 验证消息内容
    if (msg.size() > 10000) {
        cout << "[ERROR] 消息过长: " << msg.size() << " 字节" << endl;
        return -1;
    }

    // 验证消息长度
    if (msg.size() > 10000) {
        cout << "警告：消息过长 " << msg.size() << " 字节" << endl;
    }

    int ret;
    char *data = (char *) malloc(sizeof(char) * (msg.size() + 4));
    if (!data) {
        cout << "sendMsg: 内存分配失败" << endl;
        return -1;
    }

    int len = htonl(msg.size());
    memcpy(data, &len, 4);
    memcpy(data + 4, msg.c_str(), msg.size());
    ret = write_n(fd, data, msg.size() + 4);
    free(data);

    if (ret < 0) {
        perror("sendMsg error");
        close(fd);
        return -1;
    }

    if (ret != (int)(msg.size() + 4)) {
        cout << "发送字节数不匹配：期望 " << (msg.size() + 4) << "，实际 " << ret << endl;
    }
    return ret;
}

int read_n(int fd, char *msg, int n) {
    int n_left = n;
    int n_read;
    char *ptr = msg;
    while (n_left > 0) {
        if ((n_read = recv(fd, ptr, n_left, 0)) < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK) {
                n_read = 0;  // 继续尝试
            } else {
                return -1;  // 真正的错误
            }
        } else if (n_read == 0) {
            // 对端关闭连接，但我们还没读完所需的数据
            if (n_left == n) {
                // 一开始就没读到数据，返回0表示连接关闭
                return 0;
            } else {
                // 读到了部分数据但连接关闭了，这是错误情况
                cout << "连接意外关闭，期望读取 " << n << " 字节，实际读取 " << (n - n_left) << " 字节" << endl;
                return -1;
            }
        }
        ptr += n_read;
        n_left -= n_read;
    }
    return n - n_left;  // 应该等于 n
}

int recvMsg(int fd, string &msg) {
    int len = 0;
    int header_ret = read_n(fd, (char *) &len, 4);
    if (header_ret != 4) {
        if (header_ret <= 0) {
            cout << "对端断开连接" << endl;
            close(fd);
            return 0;
        }
        cout << "[ERROR] 长度字段读取失败，期望4字节，实际读取" << header_ret << "字节" << endl;
        close(fd);
        return -1;
    }

    len = ntohl(len);

    if (len <= 0 || len > 1024 * 1024) {  // 防止异常长度
        cout << "接收到异常消息长度: " << len << endl;
        close(fd);
        return -1;
    }

    char *data = (char *) malloc(sizeof(char) * (len + 1));
    if (!data) {
        cout << "内存分配失败" << endl;
        close(fd);
        return -1;
    }

    int ret = read_n(fd, data, len);
    if (ret == 0) {
        cout << "对端断开连接" << endl;
        free(data);
        close(fd);
        return 0;
    } else if (ret != len) {
        cout << "数据接收失败，期望: " << len << ", 实际: " << ret << endl;
        free(data);
        close(fd);
        return -1;
    }

    data[len] = '\0';
    msg = string(data);  // 使用 string 构造函数
    free(data);
    return ret;
}