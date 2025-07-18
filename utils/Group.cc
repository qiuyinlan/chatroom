//
// Created by shawn on 23-8-13.
//

#include <nlohmann/json.hpp>

#include <utility>
#include <random>
#include <iostream>
#include "Group.h"

using namespace std;

Group::Group(string groupName, string email) : groupName(std::move(groupName)), email(std::move(email)) {
    // 群聊唯一标识为email
}

std::string Group::to_json() {
    nlohmann::json root;
    root["groupName"] = groupName;
    root["email"] = email;
    return root.dump();
}

void Group::json_parse(const std::string &json) {
    nlohmann::json root = nlohmann::json::parse(json);
    groupName = root["groupName"].get<std::string>();
    email = root["email"].get<std::string>();
}

const string &Group::getGroupName() const {
    return groupName;
}

const std::string &Group::getEmail() const {
    return email;
}


