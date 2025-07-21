#ifndef CHATROOM_G_CHAT_H
#define CHATROOM_G_CHAT_H
#include <vector>
#include "User.h"
#include "Group.h"
class GroupChat {
public:
    GroupChat() = default;
    GroupChat(int fd, const User &user);
    void groupMenu();
    void sync(std::vector<Group> &createdGroup, std::vector<Group> &managedGroup, std::vector<Group> &joinedGroup) const;
    void startChat(std::vector<Group> &joinedGroup);
    void createGroup();
    void joinGroup() const;
    void groupHistory(const std::vector<Group> &joinedGroup);
    static void managedMenu();
    void managedGroup(std::vector<Group> &managedGroup) const;
    void approve() const;
    void remove(Group &group) const;
    static void ownerMenu();
    void managedCreatedGroup(std::vector<Group> &createdGroup);
    void appointAdmin(Group &createdGroup) const;
    void revokeAdmin(Group &createdGroup) const;
    void deleteGroup(Group &createdGroup) const;
    void showMembers(std::vector<Group> &group);
    void quit(std::vector<Group> &);
    void showJoinedGroup(const std::vector<Group> &joinedGroup);
    void showManagedGroup(std::vector<Group> &managedGroup);
    void showCreatedGroup(std::vector<Group> &createdGroup);
private:
    int fd;
    User user;
    string joined;
    string managed;
    string created;
};
#endif //CHATROOM_G_CHAT_H 