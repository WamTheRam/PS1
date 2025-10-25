// main.cpp
// Entry point for the multi-threaded prime number finder
// Handles user interaction and configuration with input validation

#include "PrimeFinder.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <cstdlib>

int main() {
    // FEATURE: Clear screen at startup for clean interface
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
    
    // FEATURE: Interactive startup configuration with input validation
    // User can modify settings before the search begins
    PrimeFinder finder("config.json");
    
    std::string choice;
    while (true) {
        std::cout << "Do you want to configure settings? (yes/no): ";
        std::cin >> choice;
        
        // Convert to lowercase for case-insensitive comparison
        std::transform(choice.begin(), choice.end(), choice.begin(), ::tolower);
        
        if (choice == "y") {
            finder.configureInteractive("config.json");
            break;
        } else if (choice == "n") {
            break;
        } else {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Invalid input! Please enter 'y' or 'n'.\n\n";
        }
    }
    
    // FEATURE: Clear screen before running prime search
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
    
    finder.run();
    return 0;
}