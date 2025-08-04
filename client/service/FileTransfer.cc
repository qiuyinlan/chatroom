#include "FileTransfer.h"
#include "../utils/proto.h"
#include "../utils/IO.h"
#include "../utils/Group.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <fstream>
#include <thread>
using namespace std;

// 默认构造函数
FileTransfer::FileTransfer() : fd(-1), user() {}

// 是为了得到好友的user——,好友也是user类，让服务器得到信息，帮忙发送
FileTransfer::FileTransfer(int fd, User user) : fd(fd), user(std::move(user)) {}


// 私聊发送文件
void FileTransfer::sendFile_Friend(int fd, const User& targetUser, const User& myUser) const {
    // 发送文件传输协议
    sendMsg(fd, SENDFILE_F);

    // 创建User副本来调用to_json
    User targetCopy = targetUser;
    sendMsg(fd, targetCopy.to_json());

    string filePath;
    int inputFile;
    off_t offset = 0;
    struct stat fileStat;
    while (true) {
        cout << "请输入发送文件的绝对路径" << endl;
          std::cout << "\033[90m输入【0】取消发送文件，返回聊天\033[0m" << std::endl;

        getline(cin,filePath);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        //按0返回
        if(filePath == "0" ){
            std::cout << "\033[90m已取消发送文件，可以继续聊天（输入消息后按enter发送）\033[0m" << std::endl;
            // 不要发送"0"给服务器，直接返回
            return;
        }
        inputFile = open(filePath.c_str(), O_RDONLY);
        if (inputFile == -1) {
            cerr << "无法打开文件,请检查输入路径" << endl;
            continue;
        }
        offset = 0;
        if (fstat(inputFile, &fileStat) == -1) {
            cerr << "获取文件状态失败" << endl;
            close(inputFile);
            continue;
        }

        sendMsg(fd, filePath);
        filesystem::path path(filePath);
        string fileName = path.filename().string();

        sendMsg(fd, fileName);
        break;
    }
    off_t fileSize = fileStat.st_size;

    sendMsg(fd, to_string(fileSize));
    cout << "开始发送文件" << endl;
    ssize_t bytesSent = sendfile(fd, inputFile, &offset, fileSize);
    system("sync");
    if (bytesSent == -1) {
        cerr << "传输文件失败" << endl;
        close(inputFile);
    }
    cout << "成功传输" << bytesSent << "字节的数据" << endl;
    close(inputFile);

    // 创建文件传输消息记录
    string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    Message fileMessage;
    fileMessage.setUidFrom(myUser.getUID());
    fileMessage.setUidTo(targetUser.getUID());
    fileMessage.setUsername(myUser.getUsername());
    fileMessage.setContent("[文件] " + fileName);
    fileMessage.setGroupName(fileName);  // 文件名存储在group_name字段
  

    cout << "你给" << targetUser.getUsername() << "发送了文件: " << fileName << endl;
    cout << "\033[90m可以继续聊天（输入消息后按enter发送）\033[0m" << endl;

 }
   
 
 
 
 
void FileTransfer::recvFile_Friend(int fd, const User& myUser) const {
    sendMsg(fd, RECVFILE_F);
    string nums;
    Message message;
    string filePath;
    string fileName;
    //先接收服务器发来的文件数
    int ret = recvMsg(fd, nums);
    
    cout << "[DEBUG] 接收到文件数量字符串: '" << nums << "'" << endl;

    int num;
    try {
        num = stoi(nums);
    } catch (const exception& e) {
        cout << "[ERROR] 解析文件数量失败: " << e.what() << endl;
        cout << "[ERROR] 接收到的内容: '" << nums << "'" << endl;
        return;
    }
    cout << "你有" << num << "个文件待接收" << endl;
    cout << "\033[90m可以继续聊天（输入消息后按enter发送）\033[0m" << endl;
    string file_info;
    for (int i = 0; i < num; i++) {
        recvMsg(fd, file_info);
        message.json_parse(file_info);
        cout << "你收到" << message.getUsername() << "的文件" << message.getGroupName() << endl;
        cout << "是否要接收[1]YES, [0]NO" << endl;
        int choice;
        while (!(cin >> choice) || (choice != 0 && choice != 1)) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        string reply;
        if (choice == 1) {
            reply = "YES";
        } else {
            reply = "NO";
        }

        sendMsg(fd, reply);
        if (choice == 0) {
            string temp;
            cout << "你拒绝接收了该文件, 按任意键返回" << endl;
            getline(cin, temp);
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            return;
        }
        fileName = message.getGroupName();
        filePath = "./fileBuffer_recv/" + fileName;
        if (!filesystem::exists("./fileBuffer_recv/")) {
            filesystem::create_directories("./fileBuffer_recv/");
        }
        ofstream ofs(filePath);
        string ssize;
        char buf[BUFSIZ];
        int n;

        recvMsg(fd, ssize);
        off_t size = stoll(ssize);
        while (size > 0) {
            if (size > sizeof(buf)) {
                n = read_n(fd, buf, sizeof(buf));
            } else {
                n = read_n(fd, buf, size);
            }
            size -= n;
            ofs.write(buf, n);
        }
        cout << "文件接收完成" << endl;
        cout << "\033[90m可以继续聊天（输入消息后按enter发送）\033[0m" << endl;
        ofs.close();
    }
    if (cin.eof()) {
        cout << "按任意键返回" << endl;
        return;
    }
 }

void FileTransfer::sendFile(vector<pair<string, User>> &my_friends) const {
    if (my_friends.empty()) {
        cout << "你当前还没有好友" << endl;
        cout << "按任意键退出" << endl;
        string temp;
        getline(cin, temp);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        return;
    }

    sendMsg(fd, SEND_FILE);
    cout << "-------------------------------------------------" << endl;
    for (int i = 0; i < my_friends.size(); ++i) {
        cout << i + 1 << ". " << my_friends[i].second.getUsername() << endl;
    }
    cout << "-------------------------------------------------" << endl;
    cout << "请选择你要发送文件的好友" << endl;
    return_last();
    int who;
    while (!(cin >> who) || who < 0 || who > my_friends.size()) {
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        cin.clear();
        cin.ignore(INT32_MAX, '\n');
    }
    cin.ignore(INT32_MAX, '\n');
    if (who == 0) {
        sendMsg(fd, BACK);
        return;
    }
    who--;

    sendMsg(fd, my_friends[who].second.to_json());
    string filePath;
    int inputFile;
    off_t offset = 0;
    struct stat fileStat;
    while (true) {
        cout << "请输入发送文件的绝对路径" << endl;
        return_last();
        getline(cin,filePath);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }
        //按0返回
        if(filePath == "0" ){
           return;
        }
        inputFile = open(filePath.c_str(), O_RDONLY);
        if (inputFile == -1) {
            cerr << "无法打开文件" << endl;
            continue;
        }
        offset = 0;
        if (fstat(inputFile, &fileStat) == -1) {
            cerr << "获取文件状态失败" << endl;
            close(inputFile);
            continue;
        }

        sendMsg(fd, filePath);
        filesystem::path path(filePath);
        string fileName = path.filename().string();

        sendMsg(fd, fileName);
        break;
    }
    off_t fileSize = fileStat.st_size;

    sendMsg(fd, to_string(fileSize));
    cout << "开始发送文件" << endl;
    ssize_t bytesSent = sendfile(fd, inputFile, &offset, fileSize);
    system("sync");
    if (bytesSent == -1) {
        cerr << "传输文件失败" << endl;
        close(inputFile);
    }
    cout << "成功传输" << bytesSent << "字节的数据" << endl;
    close(inputFile);
    cout << "按任意键返回" << endl;
    string temp;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "读到文件结尾" << endl;
        return;
    }
}

void FileTransfer::receiveFile(vector<pair<string, User>> &my_friends) const {
    sendMsg(fd, RECEIVE_FILE);
    string nums;
    Message message;
    string filePath;
    string fileName;
    //先接收服务器发来的文件数
    recvMsg(fd, nums);
    int num = stoi(nums);
    cout << "你有" << num << "个文件待接收" << endl;
    string file_info;
    for (int i = 0; i < num; i++) {
        recvMsg(fd, file_info);
        message.json_parse(file_info);
        cout << "你收到" << message.getUsername() << "的文件" << message.getGroupName() << endl;
        cout << "是否要接收[1]YES, [0]NO" << endl;
        int choice;
        while (!(cin >> choice) || (choice != 0 && choice != 1)) {
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');
        string reply;
        if (choice == 1) {
            reply = "YES";
        } else {
            reply = "NO";
        }

        sendMsg(fd, reply);
        if (choice == 0) {
            string temp;
            cout << "你拒绝接收了该文件, 按任意键返回" << endl;
            getline(cin, temp);
            if (cin.eof()) {
                cout << "读到文件结尾" << endl;
                return;
            }
            return;
        }
        fileName = message.getGroupName();
        filePath = "./fileBuffer_recv/" + fileName;
        if (!filesystem::exists("./fileBuffer_recv/")) {
            filesystem::create_directories("./fileBuffer_recv/");
        }
        ofstream ofs(filePath);
        string ssize;
        char buf[BUFSIZ];
        int n;

        recvMsg(fd, ssize);
        off_t size = stoll(ssize);
        while (size > 0) {
            if (size > sizeof(buf)) {
                n = read_n(fd, buf, sizeof(buf));
            } else {
                n = read_n(fd, buf, size);
            }
            size -= n;
            ofs.write(buf, n);
        }
        cout << "文件接收完成" << endl;
        cout << "按任意键返回" << endl;
        ofs.close();
    }
    string temp;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "按任意键返回" << endl;
        return;
    }
}

// 群聊发送文件
void FileTransfer::sendFile_Group(int fd, const Group& targetGroup, const User& myUser) const {
    // 发送群文件传输协议
    sendMsg(fd, SENDFILE_G);

    // 创建Group副本来调用to_json
    Group groupCopy = targetGroup;
    sendMsg(fd, groupCopy.to_json());

    string filePath;
    int inputFile;
    off_t offset = 0;
    struct stat fileStat;

    while (true) {
        cout << "请输入发送文件的绝对路径" << endl;
        cout << "\033[90m输入【0】取消发送文件，返回聊天\033[0m" << endl;

        getline(cin, filePath);
        if (cin.eof()) {
            cout << "读到文件结尾" << endl;
            return;
        }

        // 按0返回
        if (filePath == "0") {
            cout << "\033[90m已取消发送文件，可以继续聊天（输入消息后按enter发送）\033[0m" << endl;
            // 不要发送"0"给服务器，直接返回
            return;
        }

        inputFile = open(filePath.c_str(), O_RDONLY);
        if (inputFile == -1) {
            cerr << "无法打开文件,请检查输入路径" << endl;
            continue;
        }

        offset = 0;
        if (fstat(inputFile, &fileStat) == -1) {
            cerr << "获取文件状态失败" << endl;
            close(inputFile);
            continue;
        }

        sendMsg(fd, filePath);
        filesystem::path path(filePath);
        string fileName = path.filename().string();

        sendMsg(fd, fileName);
        break;
    }

    off_t fileSize = fileStat.st_size;
    sendMsg(fd, to_string(fileSize));

    cout << "开始发送文件" << endl;
    ssize_t bytesSent = sendfile(fd, inputFile, &offset, fileSize);
    system("sync");

    if (bytesSent == -1) {
        cerr << "传输文件失败" << endl;
        close(inputFile);
        return;
    }

    cout << "成功传输" << bytesSent << "字节的数据到群聊" << endl;
    close(inputFile);

    // 创建文件传输消息记录
    string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    cout << "你给群聊" << targetGroup.getGroupName() << "发送了文件: " << fileName << endl;
    cout << "\033[90m可以继续聊天（输入消息后按enter发送）\033[0m" << endl;
}

// 群聊接收文件
void FileTransfer::recvFile_Group(int fd, const User& myUser) const {
    sendMsg(fd, RECVFILE_G);
    string nums;

    recvMsg(fd, nums);
    int num = stoi(nums);

    if (num == 0) {
        cout << "暂无群聊文件可接收" << endl;
        return;
    }

    cout << "你有" << num << "个群聊文件待接收" << endl;

    for (int i = 0; i < num; i++) {
        Message message;
        string file_info;
        recvMsg(fd, file_info);
        message.json_parse(file_info);

        // 从content中提取文件名（去掉路径前缀）
        string fullPath = message.getContent();
        string fileName = fullPath.substr(fullPath.find_last_of("/\\") + 1);
        cout << "你收到" << message.getUsername() << "的文件: " << fileName << endl;
        cout << "是否要接收[1]YES, [0]NO" << endl;

        int choice;
        while (!(cin >> choice) || (choice != 0 && choice != 1)) {
            cout << "输入格式错误" << endl;
            cin.clear();
            cin.ignore(INT32_MAX, '\n');
        }
        cin.ignore(INT32_MAX, '\n');

        string reply = (choice == 1) ? "YES" : "NO";
        sendMsg(fd, reply);

        if (choice == 0) {
            cout << "你拒绝接收了该文件" << endl;
            continue;
        }

        // 接收文件 - 使用之前提取的fileName
        string filePath = "./fileBuffer_recv/" + fileName;

        if (!filesystem::exists("./fileBuffer_recv")) {
            filesystem::create_directories("./fileBuffer_recv");
        }

        ofstream ofs(filePath, ios::binary);
        string ssize;
        recvMsg(fd, ssize);

        off_t size = stoll(ssize);
        int n;
        char buf[BUFSIZ];

        while (size > 0) {
            if (size > sizeof(buf)) {
                n = read_n(fd, buf, sizeof(buf));
            } else {
                n = read_n(fd, buf, size);
            }
            size -= n;
            ofs.write(buf, n);
        }
        cout << "文件接收完成" << endl;
        cout << "\033[90m可以继续聊天（输入消息后按enter发送）\033[0m" << endl;
        ofs.close();
       
    }
}