#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>
#include <unordered_map>

class Shell {
private:
    std::vector<std::string> history;
    std::string history_file;
    bool running;
    
    // Менеджер виртуальной файловой системы
    class VFSManager* vfs_manager;
    
    // Внутренние методы
    void loadHistory();
    void saveHistory();
    void addToHistory(const std::string& command);
    std::vector<std::string> parseCommand(const std::string& input);
    void executeBuiltinCommand(const std::vector<std::string>& args);
    void executeExternalCommand(const std::vector<std::string>& args);
    void handleEcho(const std::vector<std::string>& args);
    void handleEnv(const std::vector<std::string>& args);
    void handleDiskInfo(const std::vector<std::string>& args);
    void setupSignalHandlers();

public:
    Shell();
    ~Shell();
    void run();
};

#endif