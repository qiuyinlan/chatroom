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
    void sendFile_Friend(const User& targetUser, const User& myUser) const;
    void recvFile_Friend( User& myUser) const;

    // 群聊文件传输
    void sendFile_Group(int fd, const Group& targetGroup, const User& myUser) const ;
    void recvFile_Group(int fd, const User& myUser) const;

    //线程
   void  sendFileThread(int fd, const User& targetUser, const User& myUser, int inputFile, off_t fileSize, off_t offset,string filePath) const;
   void  recvFileThread(int fd, string fileName,  off_t size, string filePath)const;
    private:
    int fd;
    User user;
};

#endif //FILE_TRANSFER_H 