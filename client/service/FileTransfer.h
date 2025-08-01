#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <vector>
#include "User.h"

class FileTransfer {
public:

    FileTransfer(int fd, User user);

    void sendFile(std::vector<std::pair<std::string, User>> &my_friends) const;
    void receiveFile(std::vector<std::pair<std::string, User>> &my_friends) const;

    // 传递要发送的人的信息
    void sendFile_Friend(User _friend) const;
    void recvFile_Friend(User _friend) const;

    // void sendFile_Group(int fd, std::vector<std::pair<std::string, User>> &my_friends) const;
    // void recvFile_Group(int fd, std::vector<std::pair<std::string, User>> &my_friends) const;
    private:
    int fd;
    User user;
};

#endif //FILE_TRANSFER_H 