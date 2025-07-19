# Chatroom Application

This repository contains the source code for a Chatroom application developed in C++. The application supports user login, registration, adding friends, blocking/deleting users, private and group chats, real-time notifications, and file uploads. The main technology stack includes Redis, CMake, and epoll with multithreading.

## 系统架构设计

### UID为主键的Redis数据结构设计

本系统采用**UID为主键、邮箱为辅助查找**的大厂级架构设计：

#### Redis数据结构
- **user_info**: Hash表，以UID为主键存储用户信息
  ```
  user_info: {
    "UID123": "{\"UID\":\"UID123\",\"email\":\"user@example.com\",\"username\":\"用户名\",\"password\":\"密码\",\"my_time\":\"注册时间\"}",
    "UID456": "{\"UID\":\"UID456\",\"email\":\"user2@example.com\",\"username\":\"用户名2\",\"password\":\"密码2\",\"my_time\":\"注册时间2\"}"
  }
  ```

- **email_to_uid**: Hash表，邮箱到UID的映射关系
  ```
  email_to_uid: {
    "user@example.com": "UID123",
    "user2@example.com": "UID456"
  }
  ```

- **all_uid**: Set集合，存储所有用户的UID
  ```
  all_uid: ["UID123", "UID456", "UID789"]
  ```

- **all_emails**: Set集合，存储所有用户的邮箱
  ```
  all_emails: ["user@example.com", "user2@example.com", "user3@example.com"]
  ```

#### 系统优势
1. **性能优化**: UID作为主键，避免邮箱长度不固定导致的性能问题
2. **安全性**: 用户真实身份通过UID标识，邮箱仅用于登录和辅助查找
3. **扩展性**: 支持邮箱变更而不影响用户身份标识
4. **兼容性**: 同时支持邮箱和UID两种查找方式

## Features

- **User Authentication:** Users can register and log in securely using email.
- **Friend Management:** Users can add, delete, and block friends using email or UID.
- **Private and Group Chats:** Users can engage in both private and group conversations.
- **Real-time Notifications:** The application provides real-time notifications for new messages and updates.
- **File Upload:** Users can upload and share files within the chat.
- **Multithreading:** Efficiently handles multiple connections using epoll and multithreading.

## Getting Started

To run the Chatroom application locally, follow these steps:

1. **Clone the repository:**
    ```sh
    git clone https://github.com/ShawnJeffersonWang/Messenger.git
    cd Messenger
    ```

2. **Install dependencies:**
    Ensure you have Redis installed and running. Install CMake and any other necessary libraries for the project.

3. **Compile the source code:**
    ```sh
    mkdir build
    cd build
    cmake ..
    make
    ```

4. **Run the application:**
    ```sh
    ./chatroom_executable
    ```

5. **Connect to the application:**
    Access the Chatroom application through your preferred client interface.

## Dependencies

The Chatroom application relies on the following dependencies:

- **Redis:** For data storage and message queueing.
- **CMake:** For build configuration.
- **epoll:** For handling multiple I/O events efficiently.
- **Multithreading:** To handle concurrent connections.

Please ensure that you have these dependencies installed and properly configured before running the application.

## Usage

1. **Launch the Chatroom application.**
2. **Create an account or log in** with your email address.
3. **Explore the user interface** and navigate through the available features.
4. **Manage your friends**: add, delete, or block users using email or UID.
5. **Start conversations**: engage in private or group chats.
6. **Send notifications and share files** in real-time with other users.

## Contributing

Contributions to the Chatroom application are welcome. If you encounter any issues or have suggestions for improvements, please submit a pull request or open an issue on this repository.

When contributing, please adhere to the existing code style and follow the established guidelines.

## License

This Chatroom application is licensed under the [MIT License](LICENSE). You are free to modify and distribute the application as per the license terms.

## 2024年6月 项目结构与线程池模块更新

### 主要变更
- 原有的 `server/ThreadPool.cc`、`server/ThreadPool.hpp`、`server/ThreadPool.tpp` 已全部被删除。
- 线程池相关的所有实现现已整合到 `server/MyThreadPool.hpp`，包括模板与非模板代码。
- 项目中所有线程池相关的引用均已切换为 `#include "MyThreadPool.hpp"`。
- CMakeLists.txt 已移除对 `server/ThreadPool.cc` 的编译引用。

### 文件说明
- `server/MyThreadPool.hpp`：线程池完整实现，支持动态扩容/收缩、任务泛型添加，详见文件内注释。
- 其它原有 ThreadPool 相关文件（ThreadPool.cc、ThreadPool.hpp、ThreadPool.tpp）已废弃并删除。

### 已完成的开发
- 线程池功能整合与替换。
- 相关引用与构建配置同步更新。
- **UID为主键的Redis架构重构**：完成用户注册、登录、找回密码、好友管理等核心功能的UID主键化改造。

### 进行中的开发
- 线程池功能后续优化与性能测试。
- UID主键架构的全面测试和验证。

### 未完成的开发
- 线程池与业务模块的深度集成测试。
- 群聊功能的UID主键化改造。
- 系统性能基准测试和优化。
