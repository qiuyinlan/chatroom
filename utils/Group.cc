//
// Created by shawn on 23-8-13.
//

#include <nlohmann/json.hpp>

#include <utility>
#include <random>
#include <iostream>
#include "Group.h"

using namespace std;

Group::Group(string groupName, string UID) : groupName(std::move(groupName)), UID(std::move(UID)) {
    random_device rd;
    mt19937 eng(rd());
    uniform_int_distribution<> dist(10, 99);
    int random_num = dist(eng);

    time_t timer;
    time(&timer);
    //cout<<"timer: "<<timer<<endl;
    string timeStamp = to_string(timer).substr(8, 2);
    //cout<<"timeStamp: "<<timeStamp<<endl;
    groupUID = to_string(random_num).append(timeStamp);
    admins = groupUID + "admin";
    members = groupUID + "member";
}

std::string Group::to_json() {
    nlohmann::json root;
    root["groupName"] = groupName;
    root["UID"] = UID;
    root["groupUID"] = groupUID;
    root["members"] = members;
    root["admins"] = admins;
    return root.dump();
}

void Group::json_parse(const std::string &json) {
    nlohmann::json root = nlohmann::json::parse(json);
    groupName = root["groupName"].get<std::string>();
    UID = root["UID"].get<std::string>();
    groupUID = root["groupUID"].get<std::string>();
    members = root["members"].get<std::string>();
    admins = root["admins"].get<std::string>();
}

const string &Group::getGroupName() const {
    return groupName;
}

const string &Group::getGroupUid() const {
    return groupUID;
}

const string &Group::getMembers() const {
    return members;
}

const string &Group::getAdmins() const {
    return admins;
}

const std::string &Group::getOwnerUid() const {
    return UID;
}


