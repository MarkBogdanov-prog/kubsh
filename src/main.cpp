#include "shell.h"
#include <iostream>

int main() {
    std::cout << "Welcome to kubsh - Custom Shell" << std::endl;
    std::cout << "Type '\\q' to exit, '\\e $VAR' to show environment variables" << std::endl;
    
    Shell shell;
    shell.run();
    
    std::cout << "Goodbye!" << std::endl;
    return 0;
}