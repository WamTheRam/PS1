#include "PrimeFinder.h"
#include <iostream>

int main() {
    PrimeFinder finder("config.json");
    
    std::cout << "Do you want to configure settings? (y/n): ";
    char choice;
    std::cin >> choice;
    
    if (choice == 'y' || choice == 'Y') {
        finder.configureInteractive("config.json");
    }
    
    finder.run();
    return 0;
}