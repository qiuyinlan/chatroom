#ifndef CHATROOM_TRANSACTION_H
#define CHATROOM_TRANSACTION_H

#include "../utils/User.h"

//业务处理函数，对应实现的所有功能

 void sendFile_Friend(int fd, User &user) ;

void recvFile_Friend(int fd, User &user);

void synchronize(int fd, User &user);

void start_chat(int fd, User &user);


void list_friend(int fd, User &user);

void add_friend(int fd, User &user);

void findRequest(int fd, User &user);

void del_friend(int fd, User &user);

void blockedLists(int fd, User &user);

void unblocked(int fd, User &user);

void group(int fd, User &user);

// void synchronizeGL(int fd, User &user);

void send_file(int fd, User &user);

void receive_file(int fd, User &user);


#endif //CHATROOM_TRANSACTION_H
