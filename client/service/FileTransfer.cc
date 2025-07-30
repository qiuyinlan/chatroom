#include "FileTransfer.h"
#include "../utils/proto.h"
#include "../utils/IO.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <fstream>
using namespace std;

FileTransfer::FileTransfer(int fd, User user) : fd(fd), user(std::move(user)) {}

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
        cout << "已选择要发送的好友" << endl;
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
        ofs.close();
    }
    string temp;
    getline(cin, temp);
    if (cin.eof()) {
        cout << "按任意键返回" << endl;
        return;
    }
} 