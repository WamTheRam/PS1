// PrimeFinder.h
// Header file containing class declaration and interface
#ifndef PRIMEFINDER_H
#define PRIMEFINDER_H

#include <vector>
#include <string>
#include <mutex>

// Structure to hold configuration settings from config file
struct Config {
    int num_threads;      // Number of threads to create (x)
    int max_number;       // Maximum number to search for primes (calculated from 2^X)
    std::string print_mode;      // "immediate" or "wait"
    std::string division_scheme; // "range" or "divisibility"
};

class PrimeFinder {
private:
    Config config;
    std::vector<int> primes;          // Stores all found prime numbers
    std::mutex results_mutex;         // Protects the primes vector from race conditions
    std::mutex print_mutex;           // Protects console output from interleaving
    
    // Helper structure for divisibility testing
    struct DivisibilityResult {
        bool isComposite = false;  // True if number is definitely not prime
        std::mutex mtx;
    };
    
    // Configuration management
    Config loadConfig(const std::string& filename);
    void saveConfig(const std::string& filename, int exponent);
    
    // Prime checking algorithms
    bool isPrime(int n);
    bool isPrimeParallel(int number);
    
    // Thread-safe operations
    void printResult(int threadId, int number);
    void addPrime(int number);
    
    // Division scheme implementations
    void searchRange(int threadId, int start, int end);
    void searchWithDivisibilityThreads(int threadId, int start, int end);
    void checkDivisibility(int number, const std::vector<int>& divisors, 
                          DivisibilityResult* result);
    
public:
    PrimeFinder(const std::string& configFile);
    void configureInteractive(const std::string& configFile);
    void run();
};

#endif