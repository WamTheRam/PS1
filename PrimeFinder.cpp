#include "PrimeFinder.h"
#include "SimpleJSON.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iomanip>

// Constructor: Load configuration from JSON file
PrimeFinder::PrimeFinder(const std::string& configFile) {
    config = loadConfig(configFile);
}

// FEATURE: Configuration file loading from JSON
// Reads settings from config.json and populates the Config structure
// Handles max_number in "2^X" format
Config PrimeFinder::loadConfig(const std::string& filename) {
    Config cfg;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << std::endl;
        std::cerr << "Please ensure config.json exists in the same directory." << std::endl;
        exit(1);
    }
    
    // Read entire file content
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Parse JSON using our simple parser
    cfg.num_threads = SimpleJSON::getInt(content, "num_threads");
    
    // Parse max_number - handle "2^X" format
    std::string maxNumStr = SimpleJSON::getString(content, "max_number");
    if (maxNumStr.find("2^") == 0) {
        // Extract the exponent from "2^X"
        int exponent = std::stoi(maxNumStr.substr(2));
        cfg.max_number = static_cast<int>(std::pow(2, exponent));
    } else {
        // Fallback to direct integer parsing
        cfg.max_number = SimpleJSON::getInt(content, "max_number");
    }
    
    cfg.print_mode = SimpleJSON::getString(content, "print_mode");
    cfg.division_scheme = SimpleJSON::getString(content, "division_scheme");
    
    return cfg;
}

// FEATURE: Save configuration back to JSON file
// Writes the current config settings to config.json
void PrimeFinder::saveConfig(const std::string& filename, int exponent) {
    std::ofstream outfile(filename);
    outfile << "{\n";
    outfile << "    \"num_threads\": " << config.num_threads << ",\n";
    outfile << "    \"max_number\": \"2^" << exponent << "\",\n";
    outfile << "    \"print_mode\": \"" << config.print_mode << "\",\n";
    outfile << "    \"division_scheme\": \"" << config.division_scheme << "\"\n";
    outfile << "}\n";
    outfile.close();
}

// Basic primality test algorithm
bool PrimeFinder::isPrime(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    
    int limit = static_cast<int>(std::sqrt(n));
    for (int i = 3; i <= limit; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

// FEATURE: Immediate printing with thread ID and timestamp
// Prints result immediately when a prime is found
void PrimeFinder::printResult(int threadId, int number) {
    std::lock_guard<std::mutex> lock(print_mutex);
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm* timeinfo = std::localtime(&time);
    
    std::cout << "[Thread-" << threadId << "] "
              << "[" << std::put_time(timeinfo, "%H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
              << "Found prime: " << number << std::endl;
}

// Thread-safe method to add prime to results vector
void PrimeFinder::addPrime(int number) {
    std::lock_guard<std::mutex> lock(results_mutex);
    primes.push_back(number);
}

// DIVISION SCHEME 1: Range-based division
// Each thread searches a contiguous range of numbers
// Example: For 1-1000 with 4 threads: [1-250], [251-500], [501-750], [751-1000]
void PrimeFinder::searchRange(int threadId, int start, int end) {
    for (int num = start; num <= end; num++) {
        if (isPrime(num)) {
            // FEATURE: Immediate print mode
            if (config.print_mode == "immediate") {
                printResult(threadId, num);
            }
            addPrime(num);
        }
    }
}

// Each thread checks a subset of divisors for a single number
void PrimeFinder::checkDivisibility(int number, const std::vector<int>& divisors, 
                      DivisibilityResult* result) {
    for (int divisor : divisors) {
        if (number % divisor == 0) {
            std::lock_guard<std::mutex> lock(result->mtx);
            result->isComposite = true;
            return;  // Found a divisor, number is composite
        }
    }
}

// DIVISION SCHEME 2: Parallel primality test
// Uses multiple threads to test divisibility of a single number
bool PrimeFinder::isPrimeParallel(int number) {
    if (number < 2) return false;
    if (number == 2) return true;
    if (number % 2 == 0) return false;
    
    int sqrtN = static_cast<int>(std::sqrt(number));
    std::vector<int> divisors;
    
    // Generate odd divisors to test
    for (int i = 3; i <= sqrtN; i += 2) {
        divisors.push_back(i);
    }
    
    if (divisors.empty()) return true;
    
    // Divide divisors among threads
    DivisibilityResult result;
    std::vector<std::thread> threads;
    int chunkSize = std::max(1, static_cast<int>(divisors.size()) / config.num_threads);
    
    for (int i = 0; i < config.num_threads; i++) {
        int startIdx = i * chunkSize;
        int endIdx = (i == config.num_threads - 1) ? divisors.size() : startIdx + chunkSize;
        
        if (startIdx >= divisors.size()) break;
        
        std::vector<int> chunk(divisors.begin() + startIdx, 
                              divisors.begin() + endIdx);
        
        threads.emplace_back(&PrimeFinder::checkDivisibility, this, 
                           number, chunk, &result);
    }
    
    // Wait for all divisibility checks
    for (auto& thread : threads) {
        thread.join();
    }
    
    return !result.isComposite;
}

// Search using parallel divisibility testing
void PrimeFinder::searchWithDivisibilityThreads(int threadId, int start, int end) {
    for (int num = start; num <= end; num++) {
        if (isPrimeParallel(num)) {
            if (config.print_mode == "immediate") {
                printResult(threadId, num);
            }
            addPrime(num);
        }
    }
}

// FEATURE: Interactive configuration at startup
// Allows user to modify settings before running the prime search
// INPUT VALIDATION: Ensures user enters valid options
void PrimeFinder::configureInteractive(const std::string& configFile) {
    std::cout << "=== Prime Number Finder Configuration ===\n\n";
    
    // Get number of threads with validation
    int threads;
    while (true) {
        std::cout << "Enter number of threads (current: " << config.num_threads << "): ";
        std::cin >> threads;
        
        if (std::cin.fail() || threads <= 0) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Invalid input! Please enter a positive integer.\n\n";
        } else {
            config.num_threads = threads;
            break;
        }
    }
    
    // Get max number as 2^X with validation
    int exponent;
    while (true) {
        std::cout << "Enter X for max number (2^X) (current calculates to: " 
                  << config.max_number << "): ";
        std::cin >> exponent;
        
        if (std::cin.fail() || exponent <= 0 || exponent >= 31) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Invalid input! Please enter an integer between 1 and 30.\n\n";
        } else {
            config.max_number = static_cast<int>(std::pow(2, exponent));
            break;
        }
    }
    
    // Get print mode with validation
    int printChoice;
    while (true) {
        std::cout << "\nPrinting Variations:\n";
        std::cout << "  1. Print immediately (with thread ID and timestamp)\n";
        std::cout << "  2. Wait until all threads are done then print\n";
        std::cout << "Enter choice (1 or 2) (current: " << config.print_mode << "): ";
        std::cin >> printChoice;
        
        if (std::cin.fail() || (printChoice != 1 && printChoice != 2)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "\nInvalid input! Please enter either 1 or 2.\n";
        } else {
            if (printChoice == 1) {
                config.print_mode = "immediate";
            } else {
                config.print_mode = "wait";
            }
            break;
        }
    }
    
    // Get division scheme with validation
    int divisionChoice;
    while (true) {
        std::cout << "\nTask Division Schemes:\n";
        std::cout << "  1. Range division (divide search range among threads)\n";
        std::cout << "  2. Divisibility testing (linear search, parallel divisibility check)\n";
        std::cout << "Enter choice (1 or 2) (current: " << config.division_scheme << "): ";
        std::cin >> divisionChoice;
        
        if (std::cin.fail() || (divisionChoice != 1 && divisionChoice != 2)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "\nInvalid input! Please enter either 1 or 2.\n";
        } else {
            if (divisionChoice == 1) {
                config.division_scheme = "range";
            } else {
                config.division_scheme = "divisibility";
            }
            break;
        }
    }
    
    // Save updated configuration
    saveConfig(configFile, exponent);
    std::cout << "\nConfiguration saved to " << configFile << "\n\n";
}

// Main execution method
void PrimeFinder::run() {
    // Record start time (for calculation, not printing yet)
    auto startTime = std::chrono::high_resolution_clock::now();
    auto startSystemTime = std::chrono::system_clock::now();
    auto startTimeT = std::chrono::system_clock::to_time_t(startSystemTime);
    std::tm* startTm = std::localtime(&startTimeT);

    std::cout << "\nStarting Prime Number Search\n";
    std::cout << "Configuration:\n";
    std::cout << "  - Number of threads: " << config.num_threads << "\n";
    std::cout << "  - Max number: " << config.max_number 
              << " (2^" << static_cast<int>(std::log2(config.max_number)) << ")\n";
    std::cout << "  - Print mode: " << config.print_mode << "\n";
    std::cout << "  - Division scheme: " << config.division_scheme << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    std::vector<std::thread> threads;
    
    // FEATURE: Division scheme selection
    if (config.division_scheme == "range") {
        // Range division: Split the number range among threads
        int rangeSize = config.max_number / config.num_threads;
        
        for (int i = 0; i < config.num_threads; i++) {
            int start = i * rangeSize + 1;
            int end = (i == config.num_threads - 1) ? 
                     config.max_number : (i + 1) * rangeSize;
            
            threads.emplace_back(&PrimeFinder::searchRange, this, 
                                i + 1, start, end);
        }
    } 
    else {
        // Divisibility division: Parallel testing of each number
        int rangeSize = config.max_number / config.num_threads;
        
        for (int i = 0; i < config.num_threads; i++) {
            int start = i * rangeSize + 1;
            int end = (i == config.num_threads - 1) ? 
                     config.max_number : (i + 1) * rangeSize;
            
            threads.emplace_back(&PrimeFinder::searchWithDivisibilityThreads, 
                                this, i + 1, start, end);
        }
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Record end time
    auto endTime = std::chrono::high_resolution_clock::now();
    auto endSystemTime = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    auto endTimeT = std::chrono::system_clock::to_time_t(endSystemTime);
    std::tm* endTm = std::localtime(&endTimeT);
    
    // FEATURE: Wait mode printing
    // Print all results after threads complete
    if (config.print_mode == "wait") {
        std::cout << "\nAll threads completed. Results:\n";
        std::cout << std::string(60, '-') << "\n";
        
        // Sort primes for better readability
        std::sort(primes.begin(), primes.end());
        
        for (int prime : primes) {
            std::cout << "Prime: " << prime << std::endl;
        }
    }
    
    // Summary statistics
    std::cout << std::string(60, '-') << "\n";
    std::cout << "\nSummary:\n";
    std::cout << "  - Total primes found: " << primes.size() << "\n";
    std::cout << "  - Execution time: " << elapsed.count() << " seconds\n";
    
    // Show first 20 primes
    std::cout << "  - Primes: ";
    std::sort(primes.begin(), primes.end());
    int displayCount = std::min(20, static_cast<int>(primes.size()));
    for (int i = 0; i < displayCount; i++) {
        std::cout << primes[i];
        if (i < displayCount - 1) std::cout << ", ";
    }
    if (primes.size() > 20) std::cout << "...";
    std::cout << std::endl;
    
    // FEATURE: Print start and end timestamps at the end
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "START TIME: " << std::put_time(startTm, "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "END TIME:   " << std::put_time(endTm, "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << std::string(60, '=') << "\n";
}