#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <string>

// Simple JSON parser for our config file
// Handles basic JSON structure with string and integer values
class SimpleJSON {
public:
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\"");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r\",");
        return str.substr(first, (last - first + 1));
    }
    
    static int getInt(const std::string& content, const std::string& key) {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        
        pos = content.find(":", pos);
        size_t endPos = content.find_first_of(",}", pos);
        std::string value = trim(content.substr(pos + 1, endPos - pos - 1));
        return std::stoi(value);
    }
    
    static std::string getString(const std::string& content, const std::string& key) {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        
        pos = content.find(":", pos);
        size_t startQuote = content.find("\"", pos);
        size_t endQuote = content.find("\"", startQuote + 1);
        return content.substr(startQuote + 1, endQuote - startQuote - 1);
    }
};

// Structure to hold configuration settings from config file
struct Config {
    int num_threads;      // Number of threads to create (x)
    int max_number;       // Maximum number to search for primes (y)
    std::string print_mode;      // "immediate" or "wait"
    std::string division_scheme; // "range" or "divisibility"
};

class PrimeFinder {
private:
    Config config;
    std::vector<int> primes;          // Stores all found prime numbers
    std::mutex results_mutex;         // Protects the primes vector from race conditions
    std::mutex print_mutex;           // Protects console output from interleaving
    
    // FEATURE: Configuration file loading
    // Reads settings from config.txt and populates the Config structure
    Config loadConfig(const std::string& filename) {
        Config cfg;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            // Create default config if file doesn't exist
            std::cout << "Config file not found. Creating default config.txt\n";
            std::ofstream outfile(filename);
            outfile << "num_threads=4\n";
            outfile << "max_number=1000\n";
            outfile << "print_mode=immediate\n";
            outfile << "division_scheme=range\n";
            outfile.close();
            
            cfg.num_threads = 4;
            cfg.max_number = 1000;
            cfg.print_mode = "immediate";
            cfg.division_scheme = "range";
            return cfg;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                if (key == "num_threads") cfg.num_threads = std::stoi(value);
                else if (key == "max_number") cfg.max_number = std::stoi(value);
                else if (key == "print_mode") cfg.print_mode = value;
                else if (key == "division_scheme") cfg.division_scheme = value;
            }
        }
        file.close();
        return cfg;
    }
    
    // Basic primality test algorithm
    bool isPrime(int n) {
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
    void printResult(int threadId, int number) {
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
    void addPrime(int number) {
        std::lock_guard<std::mutex> lock(results_mutex);
        primes.push_back(number);
    }
    
    // DIVISION SCHEME 1: Range-based division
    // Each thread searches a contiguous range of numbers
    // Example: For 1-1000 with 4 threads: [1-250], [251-500], [501-750], [751-1000]
    void searchRange(int threadId, int start, int end) {
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
    
    // DIVISION SCHEME 2: Divisibility testing per number
    // Helper structure to store divisibility check results from each thread
    struct DivisibilityResult {
        bool isComposite = false;  // True if number is definitely not prime
        std::mutex mtx;
    };
    
    // Each thread checks a subset of divisors for a single number
    void checkDivisibility(int number, const std::vector<int>& divisors, 
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
    bool isPrimeParallel(int number) {
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
    void searchWithDivisibilityThreads(int threadId, int start, int end) {
        for (int num = start; num <= end; num++) {
            if (isPrimeParallel(num)) {
                if (config.print_mode == "immediate") {
                    printResult(threadId, num);
                }
                addPrime(num);
            }
        }
    }
    
public:
    PrimeFinder(const std::string& configFile) {
        config = loadConfig(configFile);
    }
    
    void run() {
        std::cout << "Starting Prime Number Search\n";
        std::cout << "Configuration:\n";
        std::cout << "  - Number of threads: " << config.num_threads << "\n";
        std::cout << "  - Max number: " << config.max_number << "\n";
        std::cout << "  - Print mode: " << config.print_mode << "\n";
        std::cout << "  - Division scheme: " << config.division_scheme << "\n";
        std::cout << std::string(60, '-') << "\n";
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
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
        
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = endTime - startTime;
        
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
    }
};

int main() {
    PrimeFinder finder("config.txt");
    finder.run();
    return 0;
}