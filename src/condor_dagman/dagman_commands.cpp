/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "dagman_main.h"
#include "debug.h"
#include "parse.h"
#include "dagman_commands.h"

static void command_halt(const ClassAd& request, Dagman& dm) {
	bool pause = false;

	if (request.LookupBool("IsPause", pause) && pause) {
		debug_printf(DEBUG_NORMAL, dm.paused ? "DAG Pause: Already-paused DAG\n" : "DAG Pause: Freezing all work...\n");
		dm.paused = true;
	} else if (dm.dag->IsHalted()) {
		debug_printf(DEBUG_NORMAL, "DAGMan is already halted\n");
	} else {
		debug_printf(DEBUG_NORMAL, "Halting DAGMan progess...\n");
		dm.dag->Halt();
		dm.update_ad = true;
	}

	std::string reason;
	if (request.LookupString("HaltReason", reason)) {
		debug_printf(DEBUG_NORMAL, "%s reason: %s\n", pause ? "Pause" : "Halt", reason.c_str());
	}
}

static void command_resume(Dagman& dm) {
	// Resume clear both pause and halt (in case both have been set for some reason)
	if (dm.paused) {
		debug_printf(DEBUG_NORMAL, "DAG Un-Pause: Resuming work...\n");
		dm.paused = false;
	}

	if (dm.dag->IsHalted()) {
		debug_printf(DEBUG_NORMAL, "Resuming DAG progress...\n");
		dm.dag->UnHalt();
		dm.update_ad = true;
	}
}

static void command_set_throttles(const ClassAd& request, ClassAd& response, Dagman& dm) {
	Throttles new_throttles = dm.throttles;

	// Lookup throttles in request ad
	for (size_t i = 0; i < static_cast<size_t>(Throttle::_SIZE); i++) {
		int new_value = 0;
		if (request.LookupInteger(THROTTLE_ATTR[i], new_value)) { new_throttles[i] = new_value; }
	}

	// Update throttles (with admin limits)
	dm.SetThrottles(new_throttles);

	// Fill response ad with all set throttles
	for (size_t i = 0; i < static_cast<size_t>(Throttle::_SIZE); i++) { response.InsertAttr(THROTTLE_ATTR[i], dm.throttles[i]); }
}

bool handle_command_generic(const ClassAd& request, ClassAd& response, Dagman& dm) {
	int cmd = 0;
	if (!request.LookupInteger("DagCommand", cmd)) {
		response.InsertAttr(ATTR_ERROR_STRING, "No DAG command provided in request");
		return false;
	}

	std::string error;

	if (cmd <= (int)DAG_GENERIC_CMD::MIN || cmd >= (int)DAG_GENERIC_CMD::MAX) {
		formatstr(error, "Unknown DAG command (%d) provided", cmd);
	} else {
		switch (static_cast<DAG_GENERIC_CMD>(cmd)) {
		case DAG_GENERIC_CMD::HALT:
			command_halt(request, dm);
			break;
		case DAG_GENERIC_CMD::RESUME:
			command_resume(dm);
			break;
		case DAG_GENERIC_CMD::SET_THROTTLES:
			command_set_throttles(request, response, dm);
			break;
		default:
			formatstr(error, "DAG command (%d) not implemented", cmd);
			break;
		}
	}

	if (!error.empty()) { response.InsertAttr(ATTR_ERROR_STRING, error); }

	return error.empty();
}
