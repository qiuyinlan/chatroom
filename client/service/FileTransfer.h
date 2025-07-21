#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <vector>
#include "User.h"

class FileTransfer {
public:
    FileTransfer(int fd, User user);

    void sendFile(std::vector<std::pair<std::string, User>> &my_friends) const;
    void receiveFile(std::vector<std::pair<std::string, User>> &my_friends) const;

private:
    int fd;
    User user;
};

#endif //FILE_TRANSFER_H 