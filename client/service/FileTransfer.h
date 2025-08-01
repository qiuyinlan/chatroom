#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <vector>
#include "User.h"
#include "Group.h"

class FileTransfer {
public:
    FileTransfer() ;  // 添加默认构造函数
    FileTransfer(int fd, User user);

    void sendFile(std::vector<std::pair<std::string, User>> &my_friends) const;
    void receiveFile(std::vector<std::pair<std::string, User>> &my_friends) const;

    // 私聊文件传输
    void sendFile_Friend(int fd, const User& targetUser, const User& myUser) const;
    void recvFile_Friend(int fd, const User& myUser) const;

    // 群聊文件传输
    void sendFile_Group(int fd, const Group& targetGroup, const User& myUser) const ;

    void recvFile_Group(int fd, const User& myUser) const;
    private:
    int fd;
    User user;
};

#endif //FILE_TRANSFER_H 