//
// Created by shawn on 23-8-7.
//
#include <random>
#include <iostream>
#include <nlohmann/json.hpp>
#include "User.h"

using namespace std;
using json = nlohmann::json;

//现将get_time封装到User类中，作为User的一部分使用，更具整体性
string User::get_time() {
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


User::User() : is_online(false) {
    random_device rd;
    mt19937 eng(rd());
    uniform_int_distribution<> dist(10, 99);
    int random_num = dist(eng);

    time_t timer;
    time(&timer);
    //cout<<"timer: "<<to_string(timer)<<endl;
    string timeStamp = to_string(timer).substr(8, 2);
    //cout<<"timeStamp: "<<timeStamp<<endl;
    UID = to_string(random_num).append(timeStamp);
    my_time = get_time();
    passwd = "";
    username = "";
}

void User::setUID(string uid) {
    UID = std::move(uid);
}

[[nodiscard]] string User::getUID() const {
    return UID;
}

void User::setPassword(string password) {
    passwd = std::move(password);
}

void User::setUsername(string name) {
    username = std::move(name);
}

string User::getMyTime() const {
    return my_time;
}

[[nodiscard]] string User::getUsername() const {
    return username;
}

[[nodiscard]] string User::getPassword() const {
    return passwd;
}

void User::setIsOnline(bool online) {
    is_online = online;
}

[[nodiscard]] bool User::getIsOnline() const {
    return is_online;
}

string User::to_json() {
    json root;
    root["my_time"] = my_time;
    root["UID"] = UID;
    root["username"] = username;
    root["passwd"] = passwd;
    return root.dump();
}

void User::json_parse(const string &json_str) {
    json root = json::parse(json_str);
    my_time = root["my_time"].get<string>();
    UID = root["UID"].get<string>();
    username = root["username"].get<string>();
    passwd = root["passwd"].get<string>();
}
