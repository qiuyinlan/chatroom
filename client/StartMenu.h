//
// Created by shawn on 23-7-26.
//

#ifndef CHATROOM_STARTMENU_H
#define CHATROOM_STARTMENU_H

#include <string>
#include "User.h"

bool isNumber(const std::string &input);

char getch();

void get_password(std::string &password);

int login(int fd, User &user);

int email_register(int fd);
int email_reset_password(int fd);

#endif  // CHATROOM_STARTMENU_H
