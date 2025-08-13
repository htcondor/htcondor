
#include "condor_common.h"
//#include "condor_daemon_core.h"
#include "condor_config.h"

#include "librarian.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>

namespace conf = LibrarianConfigOptions;

// Helper function to split command into argc/argv format
std::vector<std::string> parseCommand(const std::string& command) {
    std::vector<std::string> tokens;
    std::istringstream iss(command);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

void runUpdateProcess(Librarian& librarian) {
    std::cout << "Starting database update..." << std::endl;
    bool librarianUpdated = librarian.update();

    if (librarianUpdated) {
        std::cout << "Database update completed successfully." << std::endl;
    } else {
        std::cerr << "Database update failed." << std::endl;
    }
}

void runQueryProcess(Librarian& librarian) {
    while (true) {
        std::string response;
        std::cout << "\nWould you like to run a query? (y/n): ";
        std::getline(std::cin, response);

        if (response != "y" && response != "Y") break;

        std::cout << "\nEnter myHistoryList command (e.g., '-user snayar2 -clusterId 23 -usage'):\n";
        std::cout << "Command: ";
        
        std::string command;
        std::getline(std::cin, command);
        
        // Parse the command string into argc/argv format
        auto tokens = parseCommand("myHistoryList " + command); // Add program name
        
        // Convert to char* array - create a persistent copy of the strings
        std::vector<std::unique_ptr<char[]>> argStorage;
        std::vector<char*> argv;
        
        for (const auto& token : tokens) {
            // Allocate memory and copy the string
            auto storage = std::make_unique<char[]>(token.length() + 1);
            std::strcpy(storage.get(), token.c_str());
            argv.push_back(storage.get());
            argStorage.push_back(std::move(storage));
        }
        argv.push_back(nullptr);

        int argc = static_cast<int>(tokens.size());
        
        // Call the query system - argStorage keeps the strings alive
        std::cout << "\n--- Query Results ---\n";
        int result = librarian.query(argc, argv.data());
        
        if (result != 0) {
            std::cout << "Query failed with exit code: " << result << "\n";
        }
        std::cout << "--- End Results -----\n";
        
        // tokens goes out of scope here, which is fine because we're done with argv
    }
    
    std::cout << "Exiting query interface.\n";
}


int main() {
    dprintf_set_tool_debug("TOOL", 0);
    set_debug_flags(NULL, D_ALWAYS | D_NOHEADER);
    set_priv_initialize(); // allow uid switching if root
    config();

    Librarian librarian;

    //std::string historyPath;
    param(librarian.config[conf::str::ArchiveFile], "HISTORY");

    //std::string dbPath;
    param(librarian.config[conf::str::DBPath], "LIBRARIAN_DATABASE");

    if ( ! librarian.initialize()) {
        std::cerr << "Failed to initialize Librarian." << std::endl;
        return 1;
    }

    // Main menu loop
    while (true) {
        std::cout << "\n=== Librarian Main Menu ===" << std::endl;
        std::cout << "1) Update database" << std::endl;
        std::cout << "2) Query database" << std::endl;
        std::cout << "3) Exit" << std::endl;
        std::cout << "Choose an option (1-3): ";
        
        std::string choice;
        std::getline(std::cin, choice);
        
        if (choice == "1") {
            runUpdateProcess(librarian);
        }
        else if (choice == "2") {
            runQueryProcess(librarian);
        }
        else if (choice == "3") {
            std::cout << "Goodbye!" << std::endl;
            break;
        }
        else {
            std::cout << "Invalid choice. Please enter 1, 2, or 3." << std::endl;
        }
    }

    return 0;
}