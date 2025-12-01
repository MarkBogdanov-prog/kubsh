#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <map>
#include <algorithm>

static std::string real_etc = "/real_etc";
static std::string vfs_root = "/opt/users";

static int etc_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    std::string full_path = real_etc + path;
    
    memset(stbuf, 0, sizeof(struct stat));
    
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
        *stbuf = st;
        return 0;
    }
    
    return -ENOENT;
}

static int etc_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);

    std::string full_path = real_etc + path;
    DIR *dp = opendir(full_path.c_str());
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        
        if (filler(buf, de->d_name, &st, 0, (fuse_fill_dir_flags)0)) break;
    }
    
    closedir(dp);
    return 0;
}

static int etc_open(const char *path, struct fuse_file_info *fi) {
    std::string full_path = real_etc + path;
    
    int res = access(full_path.c_str(), F_OK);
    if (res == -1) return -errno;
    
    return 0;
}

static int etc_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    std::string full_path = real_etc + path;
    
    // Для passwd файла объединяем реальный файл и пользователей из VFS
    if (strcmp(path, "/passwd") == 0) {
        std::string content;
        
        // Читаем оригинальный /etc/passwd
        std::ifstream orig_file(full_path);
        if (orig_file.is_open()) {
            std::string line;
            while (std::getline(orig_file, line)) {
                content += line + "\n";
            }
            orig_file.close();
        } else {
            // Если файла нет, создаем базовый контент
            content = "root:x:0:0:root:/root:/bin/bash\n";
        }
        
        // Добавляем пользователей из VFS
        DIR *dir = opendir(vfs_root.c_str());
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_type == DT_DIR) {
                    std::string username = entry->d_name;
                    if (username != "." && username != "..") {
                        std::string user_dir = vfs_root + "/" + username;
                        
                        // Проверяем, есть ли уже такой пользователь в content
                        if (content.find(username + ":") == std::string::npos) {
                            std::string user_id = "1001";
                            std::string user_home = "/home/" + username;
                            std::string user_shell = "/bin/bash";
                            
                            std::ifstream id_file(user_dir + "/id");
                            if (id_file.is_open()) {
                                std::getline(id_file, user_id);
                                id_file.close();
                            }
                            
                            std::ifstream home_file(user_dir + "/home");
                            if (home_file.is_open()) {
                                std::getline(home_file, user_home);
                                home_file.close();
                            }
                            
                            std::ifstream shell_file(user_dir + "/shell");
                            if (shell_file.is_open()) {
                                std::getline(shell_file, user_shell);
                                shell_file.close();
                            }
                            
                            content += username + ":x:" + user_id + ":" + user_id + "::" + user_home + ":" + user_shell + "\n";
                        }
                    }
                }
            }
            closedir(dir);
        }
        
        // Возвращаем объединенное содержимое
        size_t content_size = content.size();
        if (offset < content_size) {
            if (offset + size > content_size)
                size = content_size - offset;
            memcpy(buf, content.c_str() + offset, size);
        } else {
            size = 0;
        }
        return size;
    }
    
    // Для остальных файлов - обычное чтение
    int fd = open(full_path.c_str(), O_RDONLY);
    if (fd == -1) return -errno;
    
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    return res;
}

static int etc_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    std::string full_path = real_etc + path;
    
    // Для passwd файла - записываем в VFS
    if (strcmp(path, "/passwd") == 0) {
        // Получаем полную запись
        std::string entry(buf, size);
        
        // Убираем перевод строки если есть
        if (!entry.empty() && entry.back() == '\n') {
            entry.pop_back();
        }
        
        // Парсим запись пользователя
        size_t colon1 = entry.find(':');
        if (colon1 != std::string::npos) {
            std::string username = entry.substr(0, colon1);
            
            // Пропускаем системных пользователей
            if (username != "root" && username != "daemon" && username != "bin" && 
                username != "sys" && username != "sync" && username != "games" &&
                username != "man" && username != "lp" && username != "mail" &&
                username != "news" && username != "uucp" && username != "proxy" &&
                username != "www-data" && username != "backup" && username != "list" &&
                username != "irc" && username != "gnats" && username != "nobody") {
                
                // Создаем директорию пользователя в VFS
                std::string user_dir = vfs_root + "/" + username;
                mkdir(user_dir.c_str(), 0755);
                
                // Парсим остальные поля
                size_t colon2 = entry.find(':', colon1 + 1);
                size_t colon3 = entry.find(':', colon2 + 1);
                size_t colon4 = entry.find(':', colon3 + 1);
                size_t colon5 = entry.find(':', colon4 + 1);
                size_t colon6 = entry.find(':', colon5 + 1);
                
                if (colon3 != std::string::npos && colon6 != std::string::npos) {
                    std::string user_id = entry.substr(colon2 + 1, colon3 - colon2 - 1);
                    std::string user_home = entry.substr(colon5 + 1, colon6 - colon5 - 1);
                    std::string user_shell = entry.substr(colon6 + 1);
                    
                    // Записываем в файлы VFS
                    std::ofstream id_file(user_dir + "/id");
                    if (id_file.is_open()) {
                        id_file << user_id;
                        id_file.close();
                    }
                    
                    std::ofstream home_file(user_dir + "/home");
                    if (home_file.is_open()) {
                        home_file << user_home;
                        home_file.close();
                    }
                    
                    std::ofstream shell_file(user_dir + "/shell");
                    if (shell_file.is_open()) {
                        shell_file << user_shell;
                        shell_file.close();
                    }
                    
                    std::cout << "User created via FUSE: " << username << " (UID: " << user_id << ")" << std::endl;
                }
            }
        }
        return size;
    }
    
    // Для остальных файлов - обычная запись
    int fd = open(full_path.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd == -1) return -errno;
    
    int res = pwrite(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    return res;
}

// Исправленная структура fuse_operations с правильным порядком полей
static struct fuse_operations etc_oper = {
    .getattr = etc_getattr,
    .readlink = NULL,
    .mknod = NULL,
    .mkdir = NULL,
    .unlink = NULL,
    .rmdir = NULL,
    .symlink = NULL,
    .rename = NULL,
    .link = NULL,
    .chmod = NULL,
    .chown = NULL,
    .truncate = NULL,
    .open = etc_open,
    .read = etc_read,
    .write = etc_write,
    .statfs = NULL,
    .flush = NULL,
    .release = NULL,
    .fsync = NULL,
    .setxattr = NULL,
    .getxattr = NULL,
    .listxattr = NULL,
    .removexattr = NULL,
    .opendir = NULL,
    .readdir = etc_readdir,
    .releasedir = NULL,
    .fsyncdir = NULL,
    .init = NULL,
    .destroy = NULL,
    .access = NULL,
    .create = NULL,
    .lock = NULL,
    .utimens = NULL,
    .bmap = NULL,
    .ioctl = NULL,
    .poll = NULL,
    .write_buf = NULL,
    .read_buf = NULL,
    .flock = NULL,
    .fallocate = NULL,
    .copy_file_range = NULL,
};

int main(int argc, char *argv[]) {
    // Добавляем 5-секундную задержку для монтирования FUSE
    std::cout << "Waiting 5 seconds for FUSE to initialize..." << std::endl;
    sleep(5);
    
    std::cout << "Starting FUSE filesystem for /etc emulation..." << std::endl;
    
    // Создаем необходимые директории
    mkdir(vfs_root.c_str(), 0755);
    mkdir(real_etc.c_str(), 0755);
    
    // Копируем реальный /etc/passwd если нужно
    std::ifstream src("/etc/passwd");
    std::ofstream dst(real_etc + "/passwd");
    if (src.is_open() && dst.is_open()) {
        dst << src.rdbuf();
        std::cout << "Copied /etc/passwd to " << real_etc << "/passwd" << std::endl;
    } else {
        // Создаем тестовый файл если не удалось скопировать
        std::ofstream test_file(real_etc + "/passwd");
        if (test_file.is_open()) {
            test_file << "root:x:0:0:root:/root:/bin/bash\n";
            test_file << "nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin\n";
            test_file.close();
            std::cout << "Created test passwd file" << std::endl;
        }
    }
    
    std::cout << "FUSE filesystem ready. Mounting..." << std::endl;
    return fuse_main(argc, argv, &etc_oper, NULL);
}