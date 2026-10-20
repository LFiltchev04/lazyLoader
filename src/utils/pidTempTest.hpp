#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

// Returns a list of PIDs matching the given binary name
std::vector<int> getPIDsByName(const std::string& binaryName) {
    std::vector<int> pids;

    for (const auto& entry : fs::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;

        std::string filename = entry.path().filename().string();
        
        // Check if the directory name is a number (PID)
        if (std::all_of(filename.begin(), filename.end(), [](unsigned char ch) {
                return std::isdigit(ch) != 0;
            })) {
            std::string commPath = entry.path().string() + "/comm";
            std::ifstream commFile(commPath);
            
            if (commFile.is_open()) {
                std::string name;
                std::getline(commFile, name);
                
                // Trim trailing newline if present
                if (!name.empty() && name.back() == '\n') {
                    name.pop_back();
                }

                if (name == binaryName) {
                    pids.push_back(std::stoi(filename));
                }
            }
        }
    }

    return pids;
}