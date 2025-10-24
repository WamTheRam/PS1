#ifndef SIMPLEJSON_H
#define SIMPLEJSON_H

#include <string>

// Simple JSON parser for our config file
// Handles basic JSON structure with string and integer values
class SimpleJSON {
public:
    // Trim whitespace and quotes from a string
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\"");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r\",");
        return str.substr(first, (last - first + 1));
    }
    
    // Extract integer value for a given key from JSON content
    static int getInt(const std::string& content, const std::string& key) {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        
        pos = content.find(":", pos);
        size_t endPos = content.find_first_of(",}", pos);
        std::string value = trim(content.substr(pos + 1, endPos - pos - 1));
        return std::stoi(value);
    }
    
    // Extract string value for a given key from JSON content
    static std::string getString(const std::string& content, const std::string& key) {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        
        pos = content.find(":", pos);
        size_t startQuote = content.find("\"", pos);
        size_t endQuote = content.find("\"", startQuote + 1);
        return content.substr(startQuote + 1, endQuote - startQuote - 1);
    }
};

#endif