#ifndef G_CHATCTRL_H
#define G_CHATCTRL_H
#include <vector>
#include "User.h"
class GroupChatSession {
public:
    GroupChatSession(int fd, User user);
    void group(std::vector<std::pair<std::string, User>> &my_friends) const;
private:
    int fd;
    User user;
};
#endif //G_CHATCTRL_H 