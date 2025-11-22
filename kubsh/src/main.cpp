#include "shell.h"
#include <iostream>

int main() {
    std::cout << "Welcome to kubsh - Custom Shell" << std::endl;
    std::cout << "Type '\\q' to exit, '\\e $VAR' to show environment variables" << std::endl;
    std::cout << "Type '\\l /dev/sda' to show disk partitions" << std::endl;
    std::cout << "Type '\\l boot' to show boot information" << std::endl;
    std::cout << "Type '\\container' to check container mode" << std::endl;

    Shell shell;
    shell.run();

    return 0;
}