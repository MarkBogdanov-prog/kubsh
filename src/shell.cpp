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

// Обработчик сигнала SIGHUP
void sighup_handler(int sig) {
    std::cout << "\nConfiguration reloaded" << std::endl;
}

Shell::Shell() : running(true), history_file(std::getenv("HOME") + std::string("/.kubsh_history")) {
    vfs_manager = new VFSManager();
    loadHistory();
    setupSignalHandlers();
    
    // Инициализация VFS
    vfs_manager->initialize();
}

Shell::~Shell() {
    delete vfs_manager;
}

void Shell::setupSignalHandlers() {
    signal(SIGHUP, sighup_handler);
}

void Shell::loadHistory() {
    std::ifstream file(history_file);
    std::string line;
    
    if (file.is_open()) {
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
        // Сохраняем историю после каждой команды
        saveHistory();
    }
}

std::vector<std::string> Shell::parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::stringstream ss(input);
    std::string arg;
    
    while (ss >> arg) {
        // Убираем кавычки если есть
        if (!arg.empty() && arg.front() == '"' && arg.back() == '"') {
            arg = arg.substr(1, arg.size() - 2);
        }
        args.push_back(arg);
    }
    
    return args;
}

void Shell::handleEcho(const std::vector<std::string>& args) {
    if (args.size() > 1) {
        for (size_t i = 1; i < args.size(); ++i) {
            std::cout << args[i];
            if (i < args.size() - 1) {
                std::cout << " ";
            }
        }
        std::cout << std::endl;
    }
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
            // Если есть двоеточие, разбиваем на строки
            size_t pos = var_value.find(':');
            if (pos != std::string::npos) {
                size_t start = 0;
                while (pos != std::string::npos) {
                    std::cout << var_value.substr(start, pos - start) << std::endl;
                    start = pos + 1;
                    pos = var_value.find(':', start);
                }
                std::cout << var_value.substr(start) << std::endl;
            } else {
                std::cout << var_value << std::endl;
            }
        } else {
            std::cout << "Environment variable not found: " << var_name << std::endl;
        }
    }
}

void Shell::handleDiskInfo(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Usage: \\l <device>" << std::endl;
        return;
    }
    
    std::string device = args[1];
    std::string command = "fdisk -l " + device + " 2>/dev/null || lsblk " + device + " 2>/dev/null || echo 'Cannot get disk info'";
    
    int result = system(command.c_str());
    if (result == -1) {
        std::cout << "Failed to execute disk info command" << std::endl;
    }
}

void Shell::executeBuiltinCommand(const std::vector<std::string>& args) {
    if (args.empty()) return;
    
    std::string command = args[0];
    
    if (command == "\\q") {
        running = false;
    } else if (command == "echo") {
        handleEcho(args);
    } else if (command == "\\e") {
        handleEnv(args);
    } else if (command == "\\l") {
        handleDiskInfo(args);
    } else {
        std::cout << "Unknown builtin command: " << command << std::endl;
    }
}

void Shell::executeExternalCommand(const std::vector<std::string>& args) {
    if (args.empty()) return;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // Дочерний процесс
        std::vector<char*> argv;
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        // Поиск в PATH
        std::string command = args[0];
        if (command.find('/') == std::string::npos) {
            // Команда без пути, ищем в PATH
            char* path_env = std::getenv("PATH");
            if (path_env) {
                std::string path_str = path_env;
                size_t start = 0;
                size_t end = path_str.find(':');
                
                while (true) {
                    std::string dir = path_str.substr(start, end - start);
                    std::string full_path = dir + "/" + command;
                    
                    if (access(full_path.c_str(), X_OK) == 0) {
                        execv(full_path.c_str(), argv.data());
                    }
                    
                    if (end == std::string::npos) break;
                    start = end + 1;
                    end = path_str.find(':', start);
                }
            }
        }
        
        // Если не нашли в PATH, пробуем напрямую
        execv(args[0].c_str(), argv.data());
        
        // Если дошли сюда, execv не удался
        std::cerr << "Command not found: " << args[0] << std::endl;
        exit(1);
    } else if (pid > 0) {
        // Родительский процесс
        int status;
        waitpid(pid, &status, 0);
    } else {
        std::cerr << "Failed to fork process" << std::endl;
    }
}

void Shell::run() {
    std::string input;
    
    std::cout << "kubsh> ";
    while (running && std::getline(std::cin, input)) {
        // Проверка на Ctrl+D (EOF)
        if (std::cin.eof()) {
            std::cout << std::endl;
            break;
        }
        
        // Пропускаем пустые строки
        if (input.empty()) {
            std::cout << "kubsh> ";
            continue;
        }
        
        // Добавляем в историю
        addToHistory(input);
        
        // Парсим команду
        auto args = parseCommand(input);
        
        if (args.empty()) {
            std::cout << "kubsh> ";
            continue;
        }
        
        // Проверяем встроенные команды
        std::string first_arg = args[0];
        if (first_arg == "\\q" || first_arg == "echo" || first_arg == "\\e" || first_arg == "\\l") {
            executeBuiltinCommand(args);
        } else {
            executeExternalCommand(args);
        }
        
        if (running) {
            std::cout << "kubsh> ";
        }
    }
    
    saveHistory();
}