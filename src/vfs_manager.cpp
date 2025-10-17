#include "vfs_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#include <filesystem>

VFSManager::VFSManager() {
    users_dir = std::getenv("HOME") + std::string("/users");
}

VFSManager::~VFSManager() {}

bool VFSManager::directoryExists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR);
}

void VFSManager::writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

std::vector<UserInfo> VFSManager::getSystemUsers() {
    std::vector<UserInfo> users;
    
    setpwent(); // Сбрасываем указатель в файле паролей
    
    struct passwd* pw;
    while ((pw = getpwent()) != nullptr) {
        // Пропускаем системных пользователей без логина
        std::string shell = pw->pw_shell;
        if (shell.find("false") == std::string::npos && 
            shell.find("nologin") == std::string::npos) {
            UserInfo user;
            user.username = pw->pw_name;
            user.id = std::to_string(pw->pw_uid);
            user.home = pw->pw_dir;
            user.shell = pw->pw_shell;
            users.push_back(user);
        }
    }
    
    endpwent();
    return users;
}

void VFSManager::createUserDirectory(const UserInfo& user) {
    std::string user_dir = users_dir + "/" + user.username;
    
    // Создаем директорию пользователя
    mkdir(user_dir.c_str(), 0755);
    
    // Создаем файлы с информацией
    writeFile(user_dir + "/id", user.id);
    writeFile(user_dir + "/home", user.home);
    writeFile(user_dir + "/shell", user.shell);
}

void VFSManager::removeUserDirectory(const std::string& username) {
    std::string user_dir = users_dir + "/" + username;
    std::string command = "rm -rf " + user_dir;
    system(command.c_str());
}

void VFSManager::initialize() {
    // Создаем основную директорию
    if (!directoryExists(users_dir)) {
        mkdir(users_dir.c_str(), 0755);
    }
    
    // Получаем список пользователей системы и создаем для них директории
    auto system_users = getSystemUsers();
    for (const auto& user : system_users) {
        std::string user_dir = users_dir + "/" + user.username;
        if (!directoryExists(user_dir)) {
            createUserDirectory(user);
        }
    }
    
    std::cout << "VFS initialized at: " << users_dir << std::endl;
}

void VFSManager::createUser(const std::string& username) {
    std::string command = "sudo adduser " + username + " --disabled-password --gecos ''";
    int result = system(command.c_str());
    
    if (result == 0) {
        // Получаем информацию о новом пользователе
        struct passwd* pw = getpwnam(username.c_str());
        if (pw) {
            UserInfo user;
            user.username = pw->pw_name;
            user.id = std::to_string(pw->pw_uid);
            user.home = pw->pw_dir;
            user.shell = pw->pw_shell;
            createUserDirectory(user);
            std::cout << "User created: " << username << std::endl;
        }
    } else {
        std::cerr << "Failed to create user: " << username << std::endl;
    }
}

void VFSManager::deleteUser(const std::string& username) {
    std::string command = "sudo userdel -r " + username;
    int result = system(command.c_str());
    
    if (result == 0) {
        removeUserDirectory(username);
        std::cout << "User deleted: " << username << std::endl;
    } else {
        std::cerr << "Failed to delete user: " << username << std::endl;
    }
}