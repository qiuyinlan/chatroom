#include "../utils/TCP.h"
#include "../utils/IO.h"
#include "LoginHandler.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <csignal>
#include "ThreadPool.hpp"
#include "proto.h"
#include "Redis.h"
#include <curl/curl.h>
#include <string>



using namespace std;

void signalHandler(int signum) {
    cout << "\n[服务器] 接收到信号 " << signum;

    // 只处理用户主动退出的信号
    if (signum == SIGINT) {
        cout << " (Ctrl+C)" << endl;
        cout << "[服务器] 正在清理资源..." << endl;

        // 清理Redis中的在线状态
        Redis redis;
        if (redis.connect()) {
            cout << "[服务器] 清理Redis中的在线状态..." << endl;
            redis.del("is_online");
            redis.del("is_chat");
            cout << "[服务器] Redis清理完成" << endl;
        } else {
            cout << "[服务器] Redis连接失败，无法清理" << endl;
        }

        cout << "[服务器] 服务器正常退出" << endl;
        exit(signum);
    } else if (signum == SIGTERM) {
        cout << " (终止信号)" << endl;
        cout << "[服务器] 收到终止信号，正在退出..." << endl;
        exit(signum);
    } else {
        cout << " (未处理的信号，忽略)" << endl;
        // 其他信号不处理，让程序继续运行
    }
}


int main(int argc, char *argv[]) {
    if (argc == 1) {
        IP = "10.30.0.146";
        PORT = 8000;
    } else if (argc == 3) {
        IP = argv[1];
        PORT = stoi(argv[2]);
    } else {
        // 错误情况
        cerr << "Invalid number of arguments. Usage: program_name [IP] [port]" << endl;
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);        // 忽略SIGPIPE信号，避免客户端断开导致服务器退出
    signal(SIGINT, signalHandler);   // 处理Ctrl+C

    //signal(SIGTERM, signalHandler);  // 处理终止信号
    
    //服务器启动时删除所有在线用户
    Redis redis;
    if (redis.connect()) {
        cout << "服务器启动，清理残留的在线状态..." << endl;
        redis.del("is_online");
        redis.del("is_chat");
        cout << "在线状态清理完成" << endl;

        // 额外检查：显示清理后的在线用户数
        int online_count = redis.hlen("is_online");
        cout << "当前在线用户数: " << online_count << endl;
    } else {
        cout << "Redis连接失败，无法清理在线状态" << endl;
    }

    int ret;
    int num = 0;
    char str[INET_ADDRSTRLEN];
    string msg;
    //bug 这里在线程池里打断点的话，直接导致客户端无法连接
    ThreadPool pool(16);
    int listenfd = Socket();
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void *) &opt, sizeof(opt));
    Bind(listenfd, IP, PORT);
    Listen(listenfd);
    int epfd = epoll_create(1024);

    struct epoll_event temp, ep[1024];
    //bug 之前写成temp.events=listenfd,导致if(ep[i].data.fd==listen)直接没有执行
    //客户端不连还好，一连上直接触发大量IO操作
    temp.data.fd = listenfd;
    temp.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &temp);

    while (true) {
        ret = epoll_wait(epfd, ep, 1024, -1);
        for (int i = 0; i < ret; i++) {
            //如果不是读事件，继续循环
            if (!(ep[i].events & EPOLLIN))
                continue;

            int fd = ep[i].data.fd;
            //控制连接
            if (ep[i].data.fd == listenfd) {
                struct sockaddr_in cli_addr;
                memset(&cli_addr, 0, sizeof(cli_addr));
                socklen_t cli_len = sizeof(cli_addr);
                int connfd = Accept(listenfd, (struct sockaddr *) &cli_addr, &cli_len);

                cout << "received from " << inet_ntop(AF_INET, &cli_addr.sin_addr.s_addr, str, sizeof(str))
                     << " at port " << ntohs(cli_addr.sin_port) << endl;
                cout << "cfd " << connfd << "-----client " << ++num << endl;

                int flag = fcntl(connfd, F_GETFL);
                flag |= O_NONBLOCK;
                fcntl(connfd, F_SETFL, flag);

                temp.data.fd = connfd;
                temp.events = EPOLLIN;
                //启用 TCP keepalive 功能。
                int keep_alive = 1;
                //表示在连接空闲3秒后开始发送 TCP keepalive 探测包。
                int keep_idle = 3;
                //表示在发送第一个 TCP keepalive 探测包后，每隔1秒发送一个探测包。
                int keep_interval = 1;
                //表示如果在发送30个 TCP keepalive 探测包后仍然没有收到响应，连接将被关闭。
                int keep_count = 30;
                setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive));
                setsockopt(connfd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
                setsockopt(connfd, SOL_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
                setsockopt(connfd, SOL_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
                //将connfd加入监听红黑树
                ret = epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &temp);
                if (ret < 0) {
                    sys_err("epoll_ctl error");
                }

            } 
            //数据连接
              else {
                int recv_ret = recvMsg(fd, msg);

                if (recv_ret <= 0 || msg.empty()) {
                    // 连接断开，从epoll中移除
                    cout << "[INFO] 客户端 " << fd << " 断开连接" << endl;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    continue;
                }

                if (msg == LOGIN) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);
                    pool.addTask([=](){ serverLogin(epfd, fd); });//登陆
                } else if (msg == NOTIFY) {
                    string uid;
                    recvMsg(fd, uid);  // 接收用户UID
                    notify(fd, uid);   // 传递fd和uid参数
                } else if (msg == REQUEST_CODE) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);//发验证码，发前检查邮箱
                    pool.addTask([=](){ handleRequestCode(epfd, fd); });
                } else if (msg == REGISTER_WITH_CODE) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);
                    pool.addTask([=](){ serverRegisterWithCode(epfd, fd); });//注册
                } else if (msg == REQUEST_RESET_CODE) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);
                    pool.addTask([=](){ handleResetCode(epfd, fd); });
                } else if (msg == RESET_PASSWORD_WITH_CODE) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);
                    pool.addTask([=](){ resetPasswordWithCode(epfd, fd); });
                } else if (msg == FIND_PASSWORD_WITH_CODE) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);
                    pool.addTask([=](){ findPasswordWithCode(epfd, fd); });
                }
                else{
                    cout << "[DEBUG] 未知协议: '" << msg << "' (长度: " << msg.length() << ")" << endl;
                }
            }
        }
    }
}

