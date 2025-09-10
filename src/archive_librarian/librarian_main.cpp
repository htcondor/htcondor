
#include "condor_common.h"
//#include "condor_daemon_core.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "librarian.h"
#include <iostream>
#include <string>

namespace conf = LibrarianConfigOptions;

int main() {
    set_priv_initialize(); // allow uid switching if root
    config();

    dprintf_set_tool_debug("TOOL", 0);

    Librarian librarian;

    param(librarian.config[conf::str::ArchiveFile], "HISTORY");
    param(librarian.config[conf::str::DBPath], "LIBRARIAN_DATABASE");

    if ( ! librarian.initialize()) {
        dprintf(D_ERROR, "Failed to initialize Librarian.\n");
        return 1;
    }

    // Main menu loop
    while (true) {
        std::cout << "\n=== Librarian Main Menu ===" << std::endl;
        std::cout << "1) Update database" << std::endl;
        std::cout << "2) Exit" << std::endl;
        std::cout << "3) Exit" << std::endl;
        std::cout << "Choose an option (1-3): ";
        
        std::string choice;
        std::getline(std::cin, choice);
        
        if (choice == "1") {
            std::cout << "Starting database update..." << std::endl;
            if (librarian.update()) {
                std::cout << "Database update completed successfully." << std::endl;
            } else {
                std::cerr << "Database update failed." << std::endl;
            }
        }
        else if (choice == "2" || choice == "3") {
            std::cout << "Goodbye!" << std::endl;
            break;
        }
        else {
            std::cout << "Invalid choice. Please enter 1, 2, or 3." << std::endl;
        }
    }

    return 0;
}