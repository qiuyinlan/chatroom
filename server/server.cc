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
    cout << "signum: " << signum << endl;
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
    signal(SIGPIPE, signalHandler);
    //服务器启动时删除所有在线用户
    Redis redis;
    redis.connect();
    int len = redis.hlen("is_online");
    redisReply **arr = redis.hgetall("is_online");
    for (int i = 0; i < len; i++) {
        redis.hdel("is_online", arr[i]->str);
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
                recvMsg(fd, msg);
                if (msg == LOGIN) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);
                    pool.addTask([=](){ serverLogin(epfd, fd); });
                } else if (msg == NOTIFY) {
                    notify(fd);
                } else if (msg == REQUEST_CODE) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);
                    pool.addTask([=](){ handleRequestCode(epfd, fd); });
                } else if (msg == REGISTER_WITH_CODE) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, nullptr);
                    pool.addTask([=](){ serverRegisterWithCode(epfd, fd); });
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
                    cout << "协议指令错误" << endl ;
                }
            }
        }
    }
}

