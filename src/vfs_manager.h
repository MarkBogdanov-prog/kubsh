#ifndef VFS_MANAGER_H
#define VFS_MANAGER_H

#include <string>
#include <vector>

struct UserInfo {
    std::string username;
    std::string id;
    std::string home;
    std::string shell;
};

class VFSManager {
private:
    std::string users_dir;
    
    std::vector<UserInfo> getSystemUsers();
    void createUserDirectory(const UserInfo& user);
    void removeUserDirectory(const std::string& username);
    bool directoryExists(const std::string& path);
    void writeFile(const std::string& path, const std::string& content);

public:
    VFSManager();
    ~VFSManager();
    void initialize();
    void createUser(const std::string& username);
    void deleteUser(const std::string& username);
};

#endif