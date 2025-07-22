#ifndef CHAT_H
#define CHAT_H
#include <vector>
#include <string>
#include "../utils/User.h"

using namespace std;

class ChatSession {
public:
    ChatSession(int fd, User user);
    void startChat(vector<pair<string, User>> &my_friends);
    void findHistory(vector<pair<string, User>> &my_friends);
private:
    int fd;
    User user;
};
#endif //CHAT_H