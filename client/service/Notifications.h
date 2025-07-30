#ifndef CHATROOM_NOTIFICATIONS_H
#define CHATROOM_NOTIFICATIONS_H

#include "User.h"

void announce(string UID);

void chatReceived(int fd, const string&UID);

bool isNumericString(const std::string &str);

#endif //CHATROOM_NOTIFICATIONS_H
