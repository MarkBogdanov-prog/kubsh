#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>

class VFSManager;

class Shell {
private:
    std::vector<std::string> history;
    std::string history_file;
    bool running;
    VFSManager* vfs_manager;
    
    void setupSignalHandlers();
    void loadHistory();
    void saveHistory();
    void addToHistory(const std::string& command);
    std::vector<std::string> parseCommand(const std::string& input);
    bool executeBuiltinCommand(const std::vector<std::string>& args);
    void executeExternalCommand(const std::vector<std::string>& args);
    void handleEcho(const std::vector<std::string>& args);
    void handleEnv(const std::vector<std::string>& args);
    void handleUserCreate(const std::vector<std::string>& args);
    void handleTestUserCreate(const std::vector<std::string>& args);
    void handleListPartitions(const std::vector<std::string>& args);
    void handleContainerMode(const std::vector<std::string>& args);

public:
    Shell();
    ~Shell();
    void run();
};

#endif