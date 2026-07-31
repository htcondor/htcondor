/***************************************************************
 *
 * Copyright (C) 1990-2026, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "condor_debug.h"
#include "match_prefix.h"

#include "librarian.h"

namespace conf = LibrarianConfigOptions;

static Librarian librarian;

void update_timer([[maybe_unused]] int tid) {
	if ( ! librarian.update()) {
		dprintf(D_FULLDEBUG, "Failed to update database\n");
	}
}

void main_config() {
	librarian.reconfig();
}

void main_init([[maybe_unused]] int argc, [[maybe_unused]] char** const argv) {
	librarian.reconfig(true);

	if ( ! librarian.initialize()) {
		dprintf(D_ERROR, "Failed to initialize Librarian.\n");
		DC_Exit(1);
	}

	// Register periodic timer to process archive files and update database
	daemonCore->Register_Timer(0, librarian.config[conf::i::UpdateInterval], update_timer, "update_timer");
}

void main_exit() {
	DC_Exit(0);
}

int main(int argc, char **argv) {
	set_mySubSystem("LIBRARIAN", false, SUBSYSTEM_TYPE_DAEMON);

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_exit;
	dc_main_shutdown_graceful = main_exit;

	return dc_main(argc, argv);
}
