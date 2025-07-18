#include "StartMenu.h"
#include <iostream>
#include <cstdint>
#include "../utils/IO.h"
#include "../utils/proto.h"
#include "OperationMenu.h"
#include "User.h"
#include <termios.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

bool isNumber(const string &input) {
    for (char c: input) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return true;
}

char getch() {
    char ch;
    struct termios tm, tm_old;
    tcgetattr(STDIN_FILENO, &tm);
    tm_old = tm;
    tm.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tm);
    while ((ch = getchar()) == EOF) {
        clearerr(stdin);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &tm_old);
    return ch;
}

//bug 退格仍然输出"*"，且没有退格的效果，是因为终端配置方案中的按键绑定，改为Solaris就好了，这样Backspace才是\b
void get_password(const string& prompt, string &password) {
    char ch;
    password.clear();
    cout << prompt;
    while ((ch = getch()) != '\n') {
        if (ch == '\b' || ch == 127) {
            if (!password.empty()) {
                cout << "\b \b";
                password.pop_back();
            }
        } else {
            password.push_back(ch);
            cout << '*';
        }
    }
    cout << endl;
}

int login(int fd, User &user) {
    sendMsg(fd, LOGIN);
    string email;
    string passwd;
    while (true) {
        std::cout << "请输入您的邮箱：" << std::endl;
        getline(cin, email);
        if (email.empty()) {
            cout << "邮箱不能为空，请重新输入。" << endl;
            continue;
        }
        if (email.find(' ') != string::npos) {
            cout << "邮箱不能包含空格，请重新输入。" << endl;
            continue;
        }
        break;
    }
    while (true) {
        get_password("请输入您的密码: ", passwd);
        if (passwd.empty()) {
            cout << "密码不能为空，请重新输入。" << endl;
            continue;
        }
        if (passwd.find(' ') != string::npos) {
            cout << "密码不能包含空格，请重新输入。" << endl;
            continue;
        }
        break;
    }
    LoginRequest loginRequest(email, passwd);
    sendMsg(fd, loginRequest.to_json());
    string buf;
    recvMsg(fd, buf);
    if (buf == "-1") {
        cout << "账号不存在" << endl;
        return 0;
    } else if (buf == "-2") {
        cout << "密码错误" << endl;
        int choice;
        while (true) {
            cout << "请选择：[1]重试登录  [0]返回首页" << endl;
            if (!(cin >> choice)) {
                cout << "输入格式错误" << endl;
                cin.clear();
                cin.ignore(INT32_MAX, '\n');
                continue;
            }
            cin.ignore(INT32_MAX, '\n');
            if (choice == 1) {
                return login(fd, user); // 递归重试
            } else if (choice == 0) {
                return 0;
            } else {
                cout << "无效选项，请重新输入。" << endl;
            }
        }
    } else if (buf == "-3") {
        cout << "该用户已经登录" << endl;
        return 0;
    } else if (buf == "1") {
        cout << "登录成功!" << endl;
        string user_info;
        recvMsg(fd, user_info);
        user.json_parse(user_info);
        cout << "好久不见 " << user.getUsername() << "!" << endl;
        return 1;
    }
    return 0;
}

int email_register(int fd) {
    string email, code, username, password, password2, server_reply;
    while (true) { // 无限循环，直到输入有效邮箱
        std::cout << "请输入您的邮箱: ";
        std::getline(std::cin, email);
        std::cout << "[LOG] 用户输入邮箱: " << email << std::endl;
        if (email.empty()) {
            std::cout << "邮箱不能为空，请重新输入！" << std::endl;
            // 继续循环，重新提示用户输入
        } else {
            break; // 邮箱不为空，跳出循环
        }
    }

    // 获取验证码
    cout << "按回车获取验证码..." << endl;
    cin.ignore(INT32_MAX, '\n');
    std::cout << "[LOG] 发送 REQUEST_CODE 指令" << std::endl;
    sendMsg(fd, REQUEST_CODE);
    std::cout << "[LOG] 发送邮箱: " << email << std::endl;
    sendMsg(fd, email);
    recvMsg(fd, server_reply);
    std::cout << "[LOG] 收到服务器验证码回复: " << server_reply << std::endl;
    cout << server_reply << endl;
    if (server_reply.find("失败") != string::npos) return 0;
   
    
    while (true) { // 无限循环，直到输入有效邮箱
        cout << "请输入收到的验证码: ";
        std::getline(std::cin, code);
        std::cout << "[LOG] 用户输入验证码: " << code << std::endl;
        if (code.empty()) {
            cout << "验证码不能为空！" << endl;
        }
        if (email.empty()) {
            std::cout << "邮箱不能为空，请重新输入！" << std::endl;
            // 继续循环，重新提示用户输入
        } else {
            break; 
        }
    }
    cout << "请输入您的用户名: ";
    getline(cin, username);
    std::cout << "[LOG] 用户输入用户名: " << username << std::endl;
    if (username.empty()) {
        cout << "用户名不能为空！" << endl;
        return 0;
    }
    cout << "请输入您的密码: ";
    get_password("请输入您的密码: ", password);
    std::cout << "[LOG] 用户输入密码: " << password << std::endl;
    while (true) {
        cout << "请再次输入您的密码: ";
        get_password("请再次输入您的密码: ", password2);
        std::cout << "[LOG] 用户再次输入密码: " << password2 << std::endl;
        if (password != password2) {
            cout << "两次密码不一致！请重新输入。" << endl;
            cout << "请输入您的密码: ";
            get_password("请输入您的密码: ", password);
            continue;
        }
        break;
    }
    // 组装JSON
    json root;
    root["email"] = email;
    root["code"] = code;
    root["username"] = username;
    root["password"] = password;
    string json_str = root.dump();
    std::cout << "[LOG] 发送 REGISTER_WITH_CODE 指令" << std::endl;
    sendMsg(fd, REGISTER_WITH_CODE);
    std::cout << "[LOG] 发送注册 JSON: " << json_str << std::endl;
    sendMsg(fd, json_str);
    recvMsg(fd, server_reply);
    std::cout << "[LOG] 收到服务器注册回复: " << server_reply << std::endl;
    cout << server_reply << endl;
    return server_reply == "注册成功";
}

int email_reset_password(int fd) {
    string email, code, password, password2, server_reply;
    cout << "请输入您的邮箱: ";
    getline(cin, email);
    if (email.empty()) {
        cout << "邮箱不能为空！" << endl;
        return 0;
    }
    // 获取验证码
    cout << "按回车获取验证码..." << endl;
    cin.ignore(INT32_MAX, '\n');
    sendMsg(fd, REQUEST_RESET_CODE);
    sendMsg(fd, email);
    recvMsg(fd, server_reply);
    cout << server_reply << endl;
    if (server_reply.find("失败") != string::npos) return 0;
    // 优化：循环输入验证码，不能为空
    while (true) {
        cout << "请输入收到的验证码: ";
        getline(cin, code);
        if (code.empty()) {
            cout << "验证码不能为空！" << endl;
            continue;
        }
        break;
    }

    get_password("请输入新密码: ", password);
    get_password("请再次输入新密码: ", password2);
    if (password != password2) {
        cout << "两次密码不一致！" << endl;
        return 0;
    }
    // 组装JSON
    json root;
    root["email"] = email;
    root["code"] = code;
    root["password"] = password;
    string json_str = root.dump();
    sendMsg(fd, RESET_PASSWORD_WITH_CODE);
    sendMsg(fd, json_str);
    recvMsg(fd, server_reply);
    cout << server_reply << endl;
    return server_reply == "密码重置成功";
}

int email_find_password(int fd) {
    string email, code, server_reply;
    
   
    while (true) {
        cout << "请输入您的邮箱: ";
        getline(cin, email);
        if (email.empty()) {
            cout << "邮箱不能为空！" << endl;
            continue;
        }
        break;
    }
    // 获取验证码
    cout << "按回车获取验证码..." << endl;
    cin.ignore(INT32_MAX, '\n');
    sendMsg(fd, REQUEST_RESET_CODE); // 复用请求验证码的协议
    sendMsg(fd, email);
    recvMsg(fd, server_reply);
    cout << server_reply << endl;
    if (server_reply.find("失败") != string::npos) return 0;
    // 循环输入验证码，不能为空
    while (true) {
        cout << "请输入收到的验证码: ";
        getline(cin, code);
        if (code.empty()) {
            cout << "验证码不能为空！" << endl;
            continue;
        }
        break;
    }
    // 组装JSON
    json root;
    root["email"] = email;
    root["code"] = code;
    string json_str = root.dump();
    sendMsg(fd, FIND_PASSWORD_WITH_CODE); // 需要在协议中定义 FIND_PASSWORD_WITH_CODE
    sendMsg(fd, json_str);
    recvMsg(fd, server_reply);
    cout << server_reply << endl;
    return server_reply == "找回密码成功";
}