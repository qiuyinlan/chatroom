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

## 功能模块详解

### 文件传输功能

#### 接收文件功能 (选项11)
**功能描述**: 允许用户接收其他用户发送的文件

**工作流程**:
1. **客户端发起请求**: 用户选择"接收文件"选项，客户端发送`RECEIVE_FILE`协议
2. **服务器查询待接收文件**: 服务器从Redis中查询该用户是否有待接收的文件
3. **文件信息展示**: 服务器返回待接收文件的数量和详细信息
4. **用户选择**: 客户端显示每个文件的发送者和文件名，用户可以选择接收或拒绝
5. **文件传输**: 如果用户选择接收，服务器使用`sendfile`系统调用高效传输文件
6. **文件存储**: 接收的文件保存在`./fileBuffer_recv/`目录下

**Redis数据结构**:
- **recv + UID**: Set集合，存储待接收文件的消息信息
  ```
  recvUID123: [
    "{\"username\":\"发送者用户名\",\"UID_from\":\"发送者UID\",\"UID_to\":\"接收者UID\",\"groupName\":\"文件名\",\"content\":\"文件路径\"}"
  ]
  ```

**文件存储结构**:
- 服务器端: `./fileBuffer/` - 临时存储发送的文件
- 客户端: `./fileBuffer_recv/` - 存储接收到的文件

**技术特点**:
- 使用`sendfile`系统调用实现零拷贝文件传输
- 支持大文件传输，使用缓冲区分批处理
- 文件传输过程中有进度显示
- 支持用户拒绝接收文件

**使用场景**:
- 好友之间分享文档、图片等文件
- 群聊中的文件共享
- 离线文件传输（用户不在线时文件存储在服务器）

## Features

- **User Authentication:** Users can register and log in securely using email.
- **Friend Management:** Users can add, delete, and block friends using email or UID.
- **Private and Group Chats:** Users can engage in both private and group conversations.
- **Real-time Notifications:** The application provides real-time notifications for new messages and updates.
- **File Upload:** Users can upload and share files within the chat.
- **Multithreading:** Efficiently handles multiple connections using epoll and multithreading.

## 项目结构详解

### 核心文件说明

#### 客户端文件
- `client/client.cc`: 客户端主程序入口
- `client/controller/OperationMenu.cc`: 操作菜单控制器，处理用户选择的功能
- `client/service/FileTransfer.cc`: 文件传输服务，实现发送和接收文件功能
- `client/social/FriendManager.cc`: 好友管理功能
- `client/social/chat.cc`: 私聊功能实现

#### 服务器端文件
- `server/server.cc`: 服务器主程序入口
- `server/LoginHandler.cc`: 登录处理模块，包含登录、注册、验证码等功能
- `server/Transaction.cc`: 业务逻辑处理，包含聊天、好友管理、文件传输等核心功能
- `server/Redis.cc`: Redis数据库操作封装
- `server/group_chat.cc`: 群聊功能实现

#### 工具类文件
- `utils/proto.h`: 协议定义，包含所有通信协议常量
- `utils/IO.cc`: 网络IO操作封装
- `utils/User.cc`: 用户类定义和操作
- `utils/Message.cc`: 消息类定义和操作

### 已完成的开发
- ✅ 用户注册登录系统（支持邮箱验证码）
- ✅ 好友管理功能（添加、删除、屏蔽）
- ✅ 私聊功能（实时消息、历史记录）
- ✅ 群聊功能（创建群组、群聊、群管理）
- ✅ 文件传输功能（发送和接收文件）
- ✅ 实时通知系统
- ✅ UID主键架构重构
- ✅ 线程池模块整合

### 进行中的开发
- 🔄 系统性能优化和压力测试
- 🔄 文件传输功能的稳定性改进
- 🔄 群聊功能的UID主键化改造

### 未完成的开发
- ⏳ 文件传输的断点续传功能
- ⏳ 群聊文件的批量传输
- ⏳ 系统监控和日志记录
- ⏳ 移动端适配

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
