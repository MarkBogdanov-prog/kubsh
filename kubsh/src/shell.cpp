#include "shell.h"
#include "vfs_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <dirent.h>

extern char** environ;

// Обработчик сигнала SIGHUP
void sighup_handler(int sig) {
    std::cout << "Configuration reloaded" << std::endl;
}

Shell::Shell() : running(true), vfs_manager(nullptr) {
    const char* home = std::getenv("HOME");
    if (home) {
        history_file = std::string(home) + "/.kubsh_history";
    } else {
        history_file = ".kubsh_history";
    }
    
    // Инициализация VFS
    vfs_manager = new VFSManager();
    vfs_manager->initialize();
    
    setupSignalHandlers();
    loadHistory();
}

Shell::~Shell() {
    if (vfs_manager) {
        delete vfs_manager;
    }
}

void Shell::setupSignalHandlers() {
    signal(SIGHUP, sighup_handler);
    signal(SIGINT, [](int sig) {
        std::cout << std::endl;
    });
}

void Shell::loadHistory() {
    std::ifstream file(history_file);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                history.push_back(line);
            }
        }
        file.close();
    }
}

void Shell::saveHistory() {
    std::ofstream file(history_file);
    if (file.is_open()) {
        for (const auto& cmd : history) {
            file << cmd << std::endl;
        }
        file.close();
    }
}

void Shell::addToHistory(const std::string& command) {
    if (!command.empty() && command != "\\q") {
        history.push_back(command);
        if (history.size() > 1000) {
            history.erase(history.begin());
        }
    }
}

std::vector<std::string> Shell::parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::stringstream ss(input);
    std::string arg;

    while (ss >> arg) {
        if (!arg.empty()) {
            if ((arg.front() == '"' && arg.back() == '"') || 
                (arg.front() == '\'' && arg.back() == '\'')) {
                arg = arg.substr(1, arg.size() - 2);
            }
            args.push_back(arg);
        }
    }

    return args;
}

void Shell::handleEcho(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); ++i) {
        // Убираем кавычки если есть
        std::string arg = args[i];
        if (!arg.empty()) {
            if ((arg.front() == '"' && arg.back() == '"') || 
                (arg.front() == '\'' && arg.back() == '\'')) {
                arg = arg.substr(1, arg.size() - 2);
            }
        }
        std::cout << arg;
        if (i < args.size() - 1) {
            std::cout << " ";
        }
    }
    std::cout << std::endl;
}

void Shell::handleEnv(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        // Вывод всех переменных окружения
        for (char** env = environ; *env; ++env) {
            std::cout << *env << std::endl;
        }
    } else if (args.size() == 2) {
        std::string var_name = args[1];
        if (var_name[0] == '$') {
            var_name = var_name.substr(1);
        }
        
        char* value = std::getenv(var_name.c_str());
        if (value) {
            std::string var_value = value;
            // Для PATH разделяем по : на отдельные строки
            if (var_name == "PATH") {
                size_t start = 0;
                size_t end = 0;
                while ((end = var_value.find(':', start)) != std::string::npos) {
                    std::string path_item = var_value.substr(start, end - start);
                    if (!path_item.empty()) {
                        std::cout << path_item << std::endl;
                    }
                    start = end + 1;
                }
                // Последний элемент после последнего :
                std::string last_path = var_value.substr(start);
                if (!last_path.empty()) {
                    std::cout << last_path << std::endl;
                }
            } else {
                // Для других переменных - просто значение
                std::cout << var_value << std::endl;
            }
        } else {
            std::cout << "Environment variable not found: " << var_name << std::endl;
        }
    }
}

void Shell::handleUserCreate(const std::vector<std::string>& args) {
    if (args.size() == 2) {
        std::string username = args[1];
        if (vfs_manager) {
            vfs_manager->createUser(username);
        } else {
            std::cout << "VFS manager not initialized" << std::endl;
        }
    } else {
        std::cout << "Usage: useradd <username>" << std::endl;
    }
}

void Shell::handleTestUserCreate(const std::vector<std::string>& args) {
    if (args.size() == 2) {
        std::string username = args[1];
        
        // Создаем директорию пользователя в VFS
        std::string user_dir = "/opt/users/" + username;
        mkdir(user_dir.c_str(), 0755);
        
        // Создаем файлы с информацией
        std::ofstream id_file(user_dir + "/id");
        if (id_file.is_open()) {
            id_file << "1001";
            id_file.close();
        }
        
        std::ofstream home_file(user_dir + "/home");
        if (home_file.is_open()) {
            home_file << "/home/" + username;
            home_file.close();
        }
        
        std::ofstream shell_file(user_dir + "/shell");
        if (shell_file.is_open()) {
            shell_file << "/bin/bash";
            shell_file.close();
        }
        
        // Создаем запись в /etc/passwd
        std::ofstream passwd_file("/etc/passwd", std::ios::app);
        if (passwd_file.is_open()) {
            passwd_file << username << ":x:1001:1001::/home/" << username << ":/bin/bash\n";
            passwd_file.close();
            std::cout << "Test user created in /etc/passwd: " << username << std::endl;
        } else {
            std::cout << "Warning: Cannot write to /etc/passwd. Running without privileges?" << std::endl;
            std::cout << "User directory created: " << username << std::endl;
        }
    } else {
        std::cout << "Usage: test_add_user <username>" << std::endl;
    }
}

void Shell::handleContainerMode(const std::vector<std::string>& args) {
    std::cout << "=== Container Environment Analysis ===" << std::endl;
    
    // Проверяем, находимся ли мы в контейнере
    std::ifstream proc_file("/proc/1/cgroup");
    if (proc_file.is_open()) {
        std::string line;
        bool in_container = false;
        while (std::getline(proc_file, line)) {
            if (line.find("docker") != std::string::npos || 
                line.find("kubepods") != std::string::npos ||
                line.find("containerd") != std::string::npos) {
                in_container = true;
                break;
            }
        }
        proc_file.close();
        std::cout << "Container detected: " << (in_container ? "YES" : "NO") << std::endl;
    }
    
    // Проверяем доступность /etc/passwd для записи
    if (vfs_manager && vfs_manager->isPasswdWritable()) {
        std::cout << "/etc/passwd is WRITABLE" << std::endl;
        std::cout << "Mode: Direct system integration" << std::endl;
    } else {
        std::cout << "/etc/passwd is READ-ONLY" << std::endl;
        std::cout << "Mode: Container mode (using alternative database)" << std::endl;
    }
    
    // Проверяем альтернативную базу данных
    std::string alt_passwd = "/opt/users/passwd.db";
    struct stat st;
    if (stat(alt_passwd.c_str(), &st) == 0) {
        std::cout << "Alternative user database: EXISTS (" << st.st_size << " bytes)" << std::endl;
        
        // Показываем несколько пользователей из альтернативной базы
        std::ifstream db_file(alt_passwd);
        if (db_file.is_open()) {
            std::string line;
            int count = 0;
            std::cout << "Users in alternative database:" << std::endl;
            while (std::getline(db_file, line) && count < 5) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::cout << "  " << line.substr(0, colon) << std::endl;
                    count++;
                }
            }
            db_file.close();
        }
    } else {
        std::cout << "Alternative user database: NOT CREATED YET" << std::endl;
    }
    
    std::cout << "\nRecommendations:" << std::endl;
    if (vfs_manager && !vfs_manager->isPasswdWritable()) {
        std::cout << "- Users will be created in /opt/users/passwd.db" << std::endl;
        std::cout << "- Use 'cat /opt/users/passwd.db' to view users" << std::endl;
    } else {
        std::cout << "- Users will be created directly in /etc/passwd" << std::endl;
    }
}

void Shell::handleListPartitions(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Usage: \\l <device>" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  \\l /dev/sda     - Show partitions for sda" << std::endl;
        std::cout << "  \\l proc          - Show partitions from /proc/partitions" << std::endl;
        std::cout << "  \\l sys           - Show block devices from /sys/block" << std::endl;
        std::cout << "  \\l boot          - Show boot-related information" << std::endl;
        return;
    }

    std::string device = args[1];
    
    // Специальные команды для альтернативных источников информации
    if (device == "proc") {
        // Показываем информацию из /proc/partitions
        std::ifstream proc_file("/proc/partitions");
        if (proc_file.is_open()) {
            std::string line;
            std::cout << "Partitions from /proc/partitions:" << std::endl;
            while (std::getline(proc_file, line)) {
                std::cout << line << std::endl;
            }
            proc_file.close();
        } else {
            std::cout << "Cannot open /proc/partitions" << std::endl;
        }
        return;
    }
    else if (device == "sys") {
        // Показываем информацию из /sys/block
        std::cout << "Block devices from /sys/block:" << std::endl;
        DIR* dir = opendir("/sys/block");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name != "." && name != ".." && 
                    (name.find("sd") == 0 || name.find("hd") == 0 || 
                     name.find("vd") == 0 || name.find("nvme") == 0)) {
                    std::cout << name << std::endl;
                    
                    // Попробуем получить размер устройства
                    std::string size_path = "/sys/block/" + name + "/size";
                    std::ifstream size_file(size_path);
                    if (size_file.is_open()) {
                        std::string size_str;
                        std::getline(size_file, size_str);
                        if (!size_str.empty()) {
                            unsigned long long sectors = std::stoull(size_str);
                            unsigned long long size_gb = (sectors * 512) / (1024 * 1024 * 1024);
                            std::cout << "  Size: " << size_gb << " GB" << std::endl;
                        }
                        size_file.close();
                    }
                }
            }
            closedir(dir);
        }
        return;
    }
    else if (device == "boot") {
        // Показываем информацию о загрузочных устройствах
        std::cout << "=== Boot Information ===" << std::endl;
        
        // Проверяем UEFI/BIOS
        if (access("/sys/firmware/efi", F_OK) == 0) {
            std::cout << "Boot mode: UEFI" << std::endl;
        } else {
            std::cout << "Boot mode: BIOS (Legacy)" << std::endl;
        }
        
        // Проверяем загрузочное устройство через /proc/cmdline
        std::ifstream cmdline_file("/proc/cmdline");
        if (cmdline_file.is_open()) {
            std::string cmdline;
            std::getline(cmdline_file, cmdline);
            std::cout << "Kernel command line: " << cmdline << std::endl;
            
            // Ищем root= параметр
            size_t root_pos = cmdline.find("root=");
            if (root_pos != std::string::npos) {
                size_t end_pos = cmdline.find(" ", root_pos);
                std::string root_dev = cmdline.substr(root_pos + 5, end_pos - root_pos - 5);
                std::cout << "Root device: " << root_dev << std::endl;
            }
            cmdline_file.close();
        }
        
        // Показываем mounted filesystems
        std::cout << "\nMounted filesystems:" << std::endl;
        std::ifstream mounts_file("/proc/mounts");
        if (mounts_file.is_open()) {
            std::string line;
            while (std::getline(mounts_file, line)) {
                if (line.find("/boot") != std::string::npos || 
                    line.find(" / ") != std::string::npos) {
                    std::cout << line << std::endl;
                }
            }
            mounts_file.close();
        }
        return;
    }

    // Проверяем существование устройства
    struct stat st;
    if (stat(device.c_str(), &st) != 0) {
        std::cout << "Device " << device << " does not exist" << std::endl;
        std::cout << "Try: \\l proc  or  \\l sys  or  \\l boot  for alternative information" << std::endl;
        return;
    }

    // Пытаемся использовать системные утилиты как fallback
    bool used_fallback = false;
    
    // Попробуем lsblk (обычно есть в системах)
    std::string command = "lsblk " + device + " 2>/dev/null";
    int result = system(command.c_str());
    if (result == 0) {
        used_fallback = true;
        return;
    }
    
    // Попробуем fdisk
    command = "fdisk -l " + device + " 2>/dev/null";
    result = system(command.c_str());
    if (result == 0) {
        used_fallback = true;
        return;
    }

    // Если системные утилиты не сработали, пробуем прямой доступ
    if (!used_fallback) {
        // Открываем устройство
        int fd = open(device.c_str(), O_RDONLY);
        if (fd < 0) {
            std::cout << "Cannot open device " << device << " (permission denied)" << std::endl;
            std::cout << "You need root privileges to access block devices directly" << std::endl;
            std::cout << "Try using: \\l proc  or  \\l sys  or  \\l boot instead" << std::endl;
            return;
        }

        // Получаем размер устройства
        unsigned long long size = 0;
        if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
            std::cout << "Device: " << device << std::endl;
            std::cout << "Size: " << size / (1024 * 1024 * 1024) << " GB" << std::endl;
        } else {
            std::cout << "Device: " << device << std::endl;
            std::cout << "Size: unknown (cannot get size)" << std::endl;
        }

        // Читаем MBR/GPT заголовок
        unsigned char buffer[512];
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        
        if (bytes_read == sizeof(buffer)) {
            // Проверяем сигнатуру MBR (55 AA в конце)
            if (buffer[510] == 0x55 && buffer[511] == 0xAA) {
                std::cout << "Partition table: MBR" << std::endl;
                
                // Парсим разделы MBR
                bool found_partitions = false;
                bool has_bootable = false;
                
                for (int i = 0; i < 4; i++) {
                    int offset = 446 + i * 16;
                    
                    // Байт статуса (0x80 = bootable)
                    unsigned char status = buffer[offset];
                    
                    // Тип раздела
                    unsigned char type = buffer[offset + 4];
                    
                    // Смещение и размер в секторах
                    unsigned int lba_start = 
                        (buffer[offset + 8]) |
                        (buffer[offset + 9] << 8) |
                        (buffer[offset + 10] << 16) |
                        (buffer[offset + 11] << 24);
                    
                    unsigned int sectors = 
                        (buffer[offset + 12]) |
                        (buffer[offset + 13] << 8) |
                        (buffer[offset + 14] << 16) |
                        (buffer[offset + 15] << 24);
                    
                    if (type != 0) { // Не пустой раздел
                        found_partitions = true;
                        std::cout << "Partition " << (i + 1) << ": ";
                        
                        // ЯВНО отмечаем загрузочный раздел
                        if (status == 0x80) {
                            std::cout << "*** BOOTABLE *** ";
                            has_bootable = true;
                        } else {
                            std::cout << "Non-bootable ";
                        }
                        
                        std::cout << "Type: 0x" << std::hex << (int)type << std::dec;
                        std::cout << " Start: " << lba_start;
                        std::cout << " Sectors: " << sectors;
                        std::cout << " Size: " << (sectors * 512) / (1024 * 1024) << " MB" << std::endl;
                    }
                }
                
                if (!found_partitions) {
                    std::cout << "No partitions found" << std::endl;
                } else if (!has_bootable) {
                    std::cout << "*** WARNING: No bootable partition found! ***" << std::endl;
                }
            } else {
                // Проверяем GPT сигнатуру
                std::string gpt_signature((char*)buffer + 0x38, 8);
                if (gpt_signature == "EFI PART") {
                    std::cout << "Partition table: GPT" << std::endl;
                    std::cout << "GPT detected - boot information requires additional parsing" << std::endl;
                    
                    // Для GPT загрузочным обычно считается ESP (EFI System Partition)
                    unsigned long long first_lba = 
                        (unsigned long long)buffer[0x48] |
                        ((unsigned long long)buffer[0x49] << 8) |
                        ((unsigned long long)buffer[0x4A] << 16) |
                        ((unsigned long long)buffer[0x4B] << 24) |
                        ((unsigned long long)buffer[0x4C] << 32) |
                        ((unsigned long long)buffer[0x4D] << 40) |
                        ((unsigned long long)buffer[0x4E] << 48) |
                        ((unsigned long long)buffer[0x4F] << 56);
                    
                    std::cout << "First usable LBA: " << first_lba << std::endl;
                    std::cout << "Note: For GPT, EFI System Partition (type C12A7328-F81F-11D2-BA4B-00A0C93EC93B) is boot-related" << std::endl;
                } else {
                    std::cout << "Unknown partition table format" << std::endl;
                }
            }
        } else {
            std::cout << "Cannot read from device" << std::endl;
        }

        close(fd);
    }
}

// Новая функция для тестирования FUSE user management
void Shell::handleFuseTest(const std::vector<std::string>& args) {
    std::cout << "=== FUSE User Management Test ===" << std::endl;
    
    // Проверяем доступность FUSE mount
    if (access("/mnt/etc/passwd", F_OK) == 0) {
        std::cout << "✓ FUSE mount is accessible at /mnt/etc/passwd" << std::endl;
        
        // Читаем текущее содержимое
        std::ifstream fuse_file("/mnt/etc/passwd");
        if (fuse_file.is_open()) {
            std::string line;
            int user_count = 0;
            while (std::getline(fuse_file, line)) {
                user_count++;
            }
            fuse_file.close();
            std::cout << "✓ Current user count in FUSE: " << user_count << std::endl;
        }
        
        // Создаем тестового пользователя через FUSE
        std::string test_user = "fusetest_" + std::to_string(getpid());
        std::string user_entry = test_user + ":x:9999:9999::/home/" + test_user + ":/bin/bash\n";
        
        std::ofstream out_file("/mnt/etc/passwd", std::ios::app);
        if (out_file.is_open()) {
            out_file << user_entry;
            out_file.close();
            std::cout << "✓ Test user created via FUSE: " << test_user << std::endl;
            
            // Проверяем, что пользователь появился в VFS
            sleep(1); // Даем время для обработки
            std::string user_dir = "/opt/users/" + test_user;
            if (access(user_dir.c_str(), F_OK) == 0) {
                std::cout << "✓ User directory created in VFS: " << user_dir << std::endl;
                
                // Проверяем файлы пользователя
                if (access((user_dir + "/id").c_str(), F_OK) == 0) {
                    std::cout << "✓ User ID file created" << std::endl;
                }
                if (access((user_dir + "/home").c_str(), F_OK) == 0) {
                    std::cout << "✓ User home file created" << std::endl;
                }
                if (access((user_dir + "/shell").c_str(), F_OK) == 0) {
                    std::cout << "✓ User shell file created" << std::endl;
                }
            } else {
                std::cout << "✗ User directory not created in VFS" << std::endl;
            }
        } else {
            std::cout << "✗ Cannot write to FUSE mount" << std::endl;
        }
    } else {
        std::cout << "✗ FUSE mount not accessible at /mnt/etc/passwd" << std::endl;
        std::cout << "Make sure FUSE filesystem is mounted with: ./build/etc_fuse /mnt/etc" << std::endl;
    }
}

bool Shell::executeBuiltinCommand(const std::vector<std::string>& args) {
    if (args.empty()) return false;

    std::string command = args[0];

    if (command == "\\q" || command == "exit") {
        running = false;
        return true;
    }
    else if (command == "echo" || command == "debug") {
        handleEcho(args);
        return true;
    }
    else if (command == "\\e" || command == "env") {
        handleEnv(args);
        return true;
    }
    else if (command == "useradd") {
        handleUserCreate(args);
        return true;
    }
    else if (command == "test_add_user") {
        handleTestUserCreate(args);
        return true;
    }
    else if (command == "\\l" || command == "list") {
        handleListPartitions(args);
        return true;
    }
    else if (command == "\\container" || command == "container") {
        handleContainerMode(args);
        return true;
    }
    else if (command == "\\fuse_test" || command == "fuse_test") {
        handleFuseTest(args);
        return true;
    }
    else if (command == "history") {
        std::cout << "Command history (" << history.size() << " commands):" << std::endl;
        for (size_t i = 0; i < history.size(); ++i) {
            std::cout << "  " << (i + 1) << ": " << history[i] << std::endl;
        }
        return true;
    }
    else if (command == "clear") {
        std::cout << "\033[2J\033[1;1H"; // ANSI escape codes to clear screen
        return true;
    }

    return false;
}

void Shell::executeExternalCommand(const std::vector<std::string>& args) {
    if (args.empty()) return;

    std::string command = args[0];

    bool command_found = false;
    std::string full_path;

    if (command.find('/') != std::string::npos) {
        // Абсолютный или относительный путь
        if (access(command.c_str(), X_OK) == 0) {
            command_found = true;
            full_path = command;
        }
    } else {
        // Ищем в PATH
        char* path_env = std::getenv("PATH");
        if (path_env) {
            std::string path_str = path_env;
            size_t start = 0;
            size_t end;

            while ((end = path_str.find(':', start)) != std::string::npos) {
                std::string dir = path_str.substr(start, end - start);
                if (!dir.empty()) {
                    std::string test_path = dir + "/" + command;
                    if (access(test_path.c_str(), X_OK) == 0) {
                        command_found = true;
                        full_path = test_path;
                        break;
                    }
                }
                start = end + 1;
            }

            if (!command_found && start < path_str.length()) {
                std::string dir = path_str.substr(start);
                if (!dir.empty()) {
                    std::string test_path = dir + "/" + command;
                    if (access(test_path.c_str(), X_OK) == 0) {
                        command_found = true;
                        full_path = test_path;
                    }
                }
            }
        }
    }

    if (!command_found) {
        std::cout << command << ": command not found" << std::endl;
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {
        // Дочерний процесс
        std::vector<char*> argv;
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        // Исполняем команду
        execvp(full_path.c_str(), argv.data());
        
        // Если execvp вернул управление - ошибка
        std::cerr << "Failed to execute: " << command << std::endl;
        exit(127);
    } else if (pid > 0) {
        // Родительский процесс
        int status;
        waitpid(pid, &status, 0);
    } else {
        std::cout << "Failed to fork process" << std::endl;
    }
}

void Shell::run() {
    std::string input;
    bool is_interactive = isatty(fileno(stdin));

    while (running) {
        if (is_interactive) {
            std::cout << "kubsh> ";
            std::cout.flush();
        }
        
        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input.empty()) {
            if (is_interactive) {
                continue;
            } else {
                break;
            }
        }

        addToHistory(input);
        auto args = parseCommand(input);

        if (args.empty()) {
            continue;
        }

        // Для неинтерактивного режима (тестов) не выводим приглашение после команд
        if (!executeBuiltinCommand(args)) {
            executeExternalCommand(args);
        }
    }
    
    saveHistory();
}