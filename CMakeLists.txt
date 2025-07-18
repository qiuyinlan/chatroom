cmake_minimum_required(VERSION 3.10)

project(MessengerRebuild CXX)

# 添加 nlohmann/json 依赖
find_package(nlohmann_json 3.2.0 REQUIRED)

#加上这个可以直接#include "User.h"而不用#include "../utils/User.h"
include_directories(
        server
        client
        utils
)

add_executable(
        server.out
        server/server.cc
        server/LoginHandler.cc
        server/Transaction.cc
        server/Redis.cc
        server/group_chat.cc
        utils/User.cc
        utils/TCP.cc
        utils/IO.cc
        utils/proto.cc
        utils/Group.cc
)

add_executable(
        client.out
        client/client.cc
        client/StartMenu.cc
        client/OperationMenu.cc
        client/Notifications.cc
        client/Telegram.cc
        client/GroupChat.cc
        utils/User.cc
        utils/TCP.cc
        utils/IO.cc
        utils/proto.cc
        utils/Group.cc
)

add_executable(
        test.out
        utils/User.cc
        utils/Group.cc
        server/test.cc
)

target_link_libraries(
        server.out
        nlohmann_json::nlohmann_json
        hiredis
)

target_link_libraries(
        client.out
        nlohmann_json::nlohmann_json
)

target_link_libraries(
        test.out
        nlohmann_json::nlohmann_json
)