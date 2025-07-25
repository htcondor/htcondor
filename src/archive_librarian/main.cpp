// main.cpp
// Alpha Driver for the Librarian service: reads config, updates database, and provides interactive query shell.


#include "librarian.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

std::unordered_map<std::string, std::string> parseConfigFile(const std::string& filename) {
    std::unordered_map<std::string, std::string> configMap;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return configMap;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream lineStream(line);
        std::string key, value;

        if (std::getline(lineStream, key, '=') && std::getline(lineStream, value)) {
            // Trim whitespace (optional)
            configMap[key] = value;
        }
    }

    return configMap;
}

int main() {
    // Load and parse librarian_config.txt
    auto config = parseConfigFile("librarian_config.txt");

    std::string epochHistoryPath = config["epoch_history_path"];
    std::string historyPath = config["history_path"];
    std::string dbPath = config["db_path"];
    std::string schemaPath = config["schema_path"];
    size_t jobCacheSize = 10000; // still hardcoded
    

    Librarian librarian(schemaPath, dbPath, historyPath, epochHistoryPath, jobCacheSize);

    if (!librarian.initialize()) {
        std::cerr << "Failed to initialize Librarian." << std::endl;
        return 1;
    }

    bool librarianUpdated = librarian.update();

    // After librarian.update()
    if (librarianUpdated) {
        while (true) {
            std::string response;
            std::cout << "\nWould you like to run a query? (y/n): ";
            std::getline(std::cin, response);

            if (response != "y" && response != "Y") break;

            std::cout << "\nQuery Options:\n";
            std::cout << "  1. List my jobs\n";
            std::cout << "  2. List all users and how many jobs they've run\n";
            std::cout << "Enter choice (1/2): ";
            std::getline(std::cin, response);

            if (response == "1") {
                std::string username;
                std::cout << "Enter user name: ";
                std::getline(std::cin, username);

                auto jobs = librarian.getDBHandler().getJobsForUser(username);

                if (jobs.empty()) {
                    std::cout << "No jobs found for user '" << username << "'.\n";
                } else {
                    std::cout << "\nJobs for user '" << username << "':\n";
                    for (const auto& [clusterId, procId, timeCreated] : jobs) {
                        std::cout << "  ClusterId: " << clusterId
                                << ", ProcId: " << procId
                                << ", Created: " << timeCreated << "\n";
                    }
                }
            }

            else if (response == "2") {
                auto userJobCounts = librarian.getDBHandler().getJobCountsPerUser();

                if (userJobCounts.empty()) {
                    std::cout << "No user job data found.\n";
                } else {
                    std::cout << "\nUser Job Counts:\n";
                    for (const auto& [username, count] : userJobCounts) {
                        std::cout << "  " << username << ": " << count << " jobs\n";
                    }
                }
            }

            else {
                std::cout << "Invalid choice. Please enter 1 or 2.\n";
            }
        }

        std::cout << "Exiting query interface.\n";

    } else {
        std::cerr << "Librarian update failed." << std::endl;
        return 1;
    }

    return 0;
}
