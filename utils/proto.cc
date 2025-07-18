//
// Created by shawn on 23-8-7.
//
#include "proto.h"
#include <nlohmann/json.hpp>

#include <utility>
//数据传输操作上方空一行以突出逻辑
using namespace std;
using json = nlohmann::json;

LoginRequest::LoginRequest() = default;

//LoginRequest(const std::string &email, const std::string &passwd) : email(email), passwd(passwd) {}
LoginRequest::LoginRequest(std::string email, std::string password) : email(std::move(email)), password(std::move(password)) {}

[[nodiscard]] const string &LoginRequest::getEmail() const {
    return email;
}

void LoginRequest::setEmail(const std::string &email) {
    this->email = email;
}

[[nodiscard]] const string &LoginRequest::getPassword() const {
    return password;
}

void LoginRequest::setPassword(const std::string &password) {
    this->password = password;
}

string LoginRequest::to_json() {
    json root;
    //bug 序列化和反序列化方式不不一致，一种是数组，一种是对象键值对
    //root.append(UID);
    root["email"] = email;
    root["password"] = password;
    return root.dump();
}

void LoginRequest::json_parse(const string &json_str) {
    json root = json::parse(json_str);
    email = root["email"].get<string>();
    password = root["password"].get<string>();
}



Message::Message() = default;

Message::Message(string username, string email_from, string email_to, string groupName)
    : username(std::move(username)),
      email_from(std::move(email_from)),
      email_to(std::move(email_to)),
      group_name(std::move(groupName)) {}

[[nodiscard]] string Message::getUsername() const {
    return username;
}

void Message::setUsername(const string &name) {
    username = name;
}

const string &Message::getEmailFrom() const {
    return email_from;
}

void Message::setEmailFrom(const string &emailFrom) {
    email_from = emailFrom;
}

const string &Message::getEmailTo() const {
    return email_to;
}

void Message::setEmailTo(const string &emailTo) {
    email_to = emailTo;
}

[[nodiscard]] string Message::getContent() const {
    return content;
}

void Message::setContent(const string &msg) {
    content = msg;
}

[[nodiscard]] const string &Message::getGroupName() const {
    return group_name;
}

void Message::setGroupName(const string &groupName) {
    group_name = groupName;
}

[[nodiscard]] string Message::getTime() const {
    return timeStamp;
}

void Message::setTime(const string &t) {
    timeStamp = t;
}

string Message::get_time() {
    time_t raw_time;
    struct tm *ptm;
    time(&raw_time);
    ptm = localtime(&raw_time);
    //char* data=new char[60];
    unique_ptr<char[]> data(new char[60]);
    sprintf(data.get(), "%d-%d-%d-%02d:%02d:%02d", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour,
            ptm->tm_min, ptm->tm_sec);
    //string result(data);
    //delete[] data;
    string result(data.get());
    return result;
}

string Message::to_json() {
    timeStamp = get_time();
    json root;
    root["timeStamp"] = timeStamp;
    root["username"] = username;
    root["email_from"] = email_from;
    root["email_to"] = email_to;
    root["content"] = content;
    root["group_name"] = group_name;
    return root.dump();
}

void Message::json_parse(const string &json_str) {
    json root = json::parse(json_str);
    timeStamp = root["timeStamp"].get<string>();
    username = root["username"].get<string>();
    email_from = root["email_from"].get<string>();
    email_to = root["email_to"].get<string>();
    content = root["content"].get<string>();
    group_name = root["group_name"].get<string>();
}
