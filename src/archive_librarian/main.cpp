// main.cpp
// Alpha Driver for the Librarian service: reads config, updates database, and provides interactive query shell.


#include "librarian.h"
#include <fstream>
#include <iostream>
#include "json.hpp"

int main() {
    // Load and parse config.json
    std::ifstream configFile("librarian_config.json");
    if (!configFile.is_open()) {
        std::cerr << "Failed to open librarian_config.json" << std::endl;
        return 1;
    }

    nlohmann::json config;
    try {
        configFile >> config;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
        return 1;
    }

    // Extract required fields
    std::string epochHistoryPath = config["epoch_history_path"];
    std::string historyPath = config["history_path"];
    std::string dbPath = config["db_path"];
    std::string schemaPath = config["schema_path"];
    size_t jobCacheSize = 10000; // TODO: add this to json file

    Librarian librarian(schemaPath, dbPath, historyPath, epochHistoryPath, jobCacheSize);

    if (!librarian.initialize()) {
        std::cerr << "Failed to initialize Librarian." << std::endl;
        return 1;
    }

    bool librarianUpdated = librarian.update();
    if (!librarianUpdated) {
        std::cerr << "Librarian update failed." << std::endl;
        return 1;
    }

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

    }

    return 0;
}
