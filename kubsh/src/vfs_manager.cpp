#include "vfs_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <dirent.h>
#include <thread>
#include <chrono>
#include <set>
#include <fcntl.h>

VFSManager::VFSManager() {
    users_dir = "/opt/users";
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

bool VFSManager::userExistsInPasswd(const std::string& username) {
    struct passwd* pw = getpwnam(username.c_str());
    return pw != nullptr;
}

bool VFSManager::userExistsInAlternativePasswd(const std::string& username) {
    std::string alt_passwd = users_dir + "/passwd.db";
    std::ifstream file(alt_passwd);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find(username + ":") == 0) {
                return true;
            }
        }
    }
    return false;
}

bool VFSManager::isPasswdWritable() {
    return access("/etc/passwd", W_OK) == 0;
}

void VFSManager::createUserInAlternativePasswd(const std::string& username, const std::string& user_id, 
                                              const std::string& user_home, const std::string& user_shell) {
    std::string alt_passwd = users_dir + "/passwd.db";
    std::ofstream file(alt_passwd, std::ios::app);
    if (file.is_open()) {
        file << username << ":x:" << user_id << ":" << user_id << "::" << user_home << ":" << user_shell << "\n";
        file.close();
        std::cout << "User added to alternative database: " << username << std::endl;
        
        // Также создаем запись в группе
        std::string alt_group = users_dir + "/group.db";
        std::ofstream group_file(alt_group, std::ios::app);
        if (group_file.is_open()) {
            group_file << username << ":x:" << user_id << ":\n";
            group_file.close();
        }
    } else {
        std::cout << "Warning: Cannot write to alternative database" << std::endl;
    }
}

void VFSManager::syncVFSUsers() {
    DIR* dir = opendir(users_dir.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            std::string username = entry->d_name;
            if (username != "." && username != "..") {
                std::string user_dir = users_dir + "/" + username;
                
                if (!userExistsInPasswd(username) && !userExistsInAlternativePasswd(username)) {
                    createUserInPasswd(username, user_dir);
                }
            }
        }
    }
    closedir(dir);
}

void VFSManager::createUserInPasswd(const std::string& username, const std::string& user_dir) {
    if (userExistsInPasswd(username) || userExistsInAlternativePasswd(username)) {
        return;
    }
    
    std::string user_id, user_home, user_shell;
    
    std::ifstream id_file(user_dir + "/id");
    if (id_file.is_open()) {
        std::getline(id_file, user_id);
        id_file.close();
    } else {
        user_id = "1001";
    }
    
    std::ifstream home_file(user_dir + "/home");
    if (home_file.is_open()) {
        std::getline(home_file, user_home);
        home_file.close();
    } else {
        user_home = "/home/" + username;
    }
    
    std::ifstream shell_file(user_dir + "/shell");
    if (shell_file.is_open()) {
        std::getline(shell_file, user_shell);
        shell_file.close();
    } else {
        user_shell = "/bin/bash";
    }
    
    // Пытаемся записать в /etc/passwd
    if (isPasswdWritable()) {
        int fd = open("/etc/passwd", O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd >= 0) {
            std::string entry = username + ":x:" << user_id << ":" << user_id << "::" << user_home << ":" << user_shell << "\n";
            ssize_t written = write(fd, entry.c_str(), entry.length());
            if (written > 0) {
                fsync(fd);
                std::cout << "User added to /etc/passwd: " << username << std::endl;
            } else {
                std::cout << "Warning: Write to /etc/passwd failed, using alternative database" << std::endl;
                createUserInAlternativePasswd(username, user_id, user_home, user_shell);
            }
            close(fd);
        } else {
            std::cout << "Cannot open /etc/passwd, using alternative database" << std::endl;
            createUserInAlternativePasswd(username, user_id, user_home, user_shell);
        }
    } else {
        std::cout << "/etc/passwd is read-only, using alternative database" << std::endl;
        createUserInAlternativePasswd(username, user_id, user_home, user_shell);
    }
}

void VFSManager::startInstantSync() {
    std::thread([this]() {
        // Немедленная синхронизация при запуске
        syncVFSUsers();
        
        // Бесконечный цикл с минимальной задержкой
        while (true) {
            DIR* dir = opendir(users_dir.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    if (entry->d_type == DT_DIR) {
                        std::string username = entry->d_name;
                        if (username != "." && username != "..") {
                            std::string user_dir = users_dir + "/" + username;
                            if (!userExistsInPasswd(username) && !userExistsInAlternativePasswd(username)) {
                                createUserInPasswd(username, user_dir);
                            }
                        }
                    }
                }
                closedir(dir);
            }
            // Абсолютно минимальная задержка
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }).detach();
}

std::vector<UserInfo> VFSManager::getSystemUsers() {
    std::vector<UserInfo> users;

    setpwent();

    struct passwd* pw;
    while ((pw = getpwent()) != nullptr) {
        std::string shell = pw->pw_shell;
        if (shell.length() >= 2 && shell.substr(shell.length() - 2) == "sh") {
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
    
    if (!directoryExists(user_dir)) {
        mkdir(user_dir.c_str(), 0755);

        writeFile(user_dir + "/id", user.id);
        writeFile(user_dir + "/home", user.home);
        writeFile(user_dir + "/shell", user.shell);
    }
    
    createUserInPasswd(user.username, user_dir);
}

void VFSManager::initialize() {
    mkdir(users_dir.c_str(), 0755);

    auto system_users = getSystemUsers();
    for (const auto& user : system_users) {
        createUserDirectory(user);
    }

    // Запускаем мгновенную синхронизацию
    startInstantSync();

    std::cout << "VFS initialized at: " << users_dir << std::endl;
    
    // Проверяем и сообщаем о режиме работы
    if (!isPasswdWritable()) {
        std::cout << "Running in container mode - using alternative user database" << std::endl;
    }
}

void VFSManager::createUser(const std::string& username) {
    std::string user_dir = users_dir + "/" + username;
    mkdir(user_dir.c_str(), 0755);
    
    writeFile(user_dir + "/id", "1001");
    writeFile(user_dir + "/home", "/home/" + username);
    writeFile(user_dir + "/shell", "/bin/bash");
    
    createUserInPasswd(username, user_dir);
    
    std::cout << "User created: " << username << std::endl;
}

void VFSManager::deleteUser(const std::string& username) {
    std::string user_dir = users_dir + "/" + username;
    std::string command = "rm -rf " + user_dir;
    int result = system(command.c_str());
    (void)result;
}