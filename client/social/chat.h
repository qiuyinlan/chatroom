#ifndef CHAT_H
#define CHAT_H
#include <vector>
#include "User.h"
class ChatSession {
public:
    ChatSession(int fd, User user);
    void startChat(std::vector<std::pair<std::string, User>> &my_friends);
    void findHistory(std::vector<std::pair<std::string, User>> &my_friends) const;
private:
    int fd;
    User user;
};
#endif //CHAT_H 