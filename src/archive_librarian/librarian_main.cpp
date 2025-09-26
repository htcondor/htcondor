
#include "condor_common.h"
//#include "condor_daemon_core.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "match_prefix.h"

#include "librarian.h"
#include <iostream>
#include <string>
#include <stdexcept>

namespace conf = LibrarianConfigOptions;

int main(int argc, const char** argv) {
    set_priv_initialize(); // allow uid switching if root
    config();

    dprintf_set_tool_debug("TOOL", "D_FULLDEBUG");

    Librarian librarian;

    param(librarian.config[conf::str::ArchiveFile], "HISTORY");
    param(librarian.config[conf::str::DBPath], "LIBRARIAN_DATABASE");

    int delay = 20;
    int break_after = -1;

    for (int i = 1; i < argc; i++) {
        if (is_dash_arg_prefix(argv[i], "delay", 1)) {
            if (i + 1 >= argc) {
                dprintf(D_ERROR, "Error: -delay flag requires number parameter\n");
                exit(1);
            }

            i++;

            try {
                delay = std::stoi(argv[i]);
                if (delay < 0) { throw std::invalid_argument("Value must be a positive integer"); }
            } catch (const std::exception& e) {
                dprintf(D_ERROR, "Error: Invalid argument for delay '%s': %s\n", argv[i], e.what());
                exit(1);
            }
        } else if (is_dash_arg_prefix(argv[i], "break-after", 5)) {
            if (i + 1 >= argc) {
                dprintf(D_ERROR, "Error: -break-after flag requires number parameter\n");
                exit(1);
            }

            i++;

            try {
                break_after = std::stoi(argv[i]);
            } catch (const std::exception& e) {
                dprintf(D_ERROR, "Error: Invalid argument for break-after '%s': %s\n", argv[i], e.what());
                exit(1);
            }
        }
    }

    if ( ! librarian.initialize()) {
        dprintf(D_ERROR, "Failed to initialize Librarian.\n");
        return 1;
    }

    int iterations = 0;
    while (true) {
        if (break_after >= 0 && break_after <= iterations++) { break; }

        if (delay) {
            dprintf(D_FULLDEBUG, "Sleeping for %d seconds\n", delay);
            sleep(delay);
        }

        dprintf(D_FULLDEBUG, "Updating database\n");
        if ( ! librarian.update()) {
            dprintf(D_ALWAYS, "Update failed\n");
        }
    }

    return 0;
}