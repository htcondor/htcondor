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

// dagman_commands.C

#include "condor_common.h"
#include "dagman_main.h"
#include "debug.h"
#include "parse.h"
#include "dagman_commands.h"
#include "submit_utils.h"

static void
command_halt(const ClassAd& request, Dagman& dm) {
	bool pause = false;

	if (request.LookupBool("IsPause", pause) && pause) {
		debug_printf(DEBUG_NORMAL, dm.paused ? "DAG Pause: Already-paused DAG\n" : "DAG Pause: Freezing all work...\n");
		dm.paused = true;
	} else {
		debug_printf(DEBUG_NORMAL, dm.dag->IsHalted() ? "DAGMan is already halted\n" : "Halting DAGMan progess...\n");
		dm.dag->Halt();
		dm.update_ad = true;
	}

	std::string reason;
	if (request.LookupString("HaltReason", reason)) {
		debug_printf(DEBUG_NORMAL, "%s reason: %s\n", pause ? "Pause" : "Halt", reason.c_str());
	}
}

static std::string
command_unhalt(const ClassAd& request, Dagman& dm) {
	bool pause = false;

	if (request.LookupBool("IsPause", pause) && pause) {
		if ( ! dm.paused) {
			return "DAG is not currently paused";
		}
		debug_printf(DEBUG_NORMAL, "DAG Un-Pause: Resuming work...\n");
		dm.paused = false;
	} else {
		if ( ! dm.dag->IsHalted()) {
			return "DAG is not currently halted";
		}
		debug_printf(DEBUG_NORMAL, "Resuming DAG progress (unhalt)...\n");
		dm.dag->UnHalt();
		dm.update_ad = true;
	}

	return "";
}

bool
handle_command_generic(const ClassAd& request, ClassAd& response, Dagman& dm) {
	int cmd = 0;
	if ( ! request.LookupInteger("DagCommand", cmd)) {
		response.InsertAttr("FailureReason", "No DAG command provided in request");
		return false;
	}

	std::string error;

	switch (cmd) {
		case 1:
			{
				std::string echo = "Goodbye";
				response.LookupString("Echo", echo);

				debug_printf(DEBUG_NORMAL, "[%d] Hello World and %s\n", cmd, echo.c_str());

				response.InsertAttr("Response", "Hello... I am busy leave me alone");
				response.InsertAttr("Pid", daemonCore->getpid());
			}
			break;
		case 2:
			command_halt(request, dm);
			break;
		case 3:
			error = command_unhalt(request, dm);
			break;
		default:
			error = "Unknown DAG command provided: " + std::to_string(cmd);
			break;
	}

	if ( ! error.empty()) {
		response.InsertAttr("FailureReason", error);
	}

	return error.empty();
}

Node*
AddNode( Dag *dag, const char *name,
		 const char* directory,
		 const char* submitFileOrSubmitDesc,
		 bool noop,
		 bool done, NodeType type,
		 std::string &failReason )
{
	std::string why;
	if( !IsValidNodeName( dag, name, why ) ) {
		failReason = why;
		return NULL;
	}
	if( !IsValidSubmitName( submitFileOrSubmitDesc, why ) ) {
		failReason = why;
		return NULL;
	}
	if( done && type == NodeType::FINAL ) {
		formatstr( failReason, "Warning: FINAL Node %s cannot be set to DONE\n",
					name );
        debug_printf( DEBUG_QUIET, "%s", failReason.c_str() );
		(void)check_warning_strictness( DAG_STRICT_1, false );
		done = false;
	}
	if( done && type == NodeType::SERVICE ) {
		formatstr( failReason, "Warning: SERVICE node %s cannot be set to DONE\n",
					name );
        debug_printf( DEBUG_QUIET, "%s", failReason.c_str() );
		(void)check_warning_strictness( DAG_STRICT_1, false );
		done = false;
	}
	Node* node = new Node( name, directory, submitFileOrSubmitDesc );
	if( !node ) {
		dprintf( D_ALWAYS, "ERROR: out of memory!\n" );
			// we already know we're out of memory, so filling in
			// FailReason will likely fail, but give it a shot...
		failReason = "out of memory!";
		return NULL;
	}
	node->SetNoop( noop );
	if( done ) {
		dag->AddPreDoneNode(node);
	}
	node->SetType( type );

	// Check to see if submitFileOrSubmitDescName refers to a file or inline submit description
	if (dag->InlineDescriptions.contains(submitFileOrSubmitDesc)) {
		node->SetInlineDesc(dag->InlineDescriptions[submitFileOrSubmitDesc]);
	}

	ASSERT( dag != NULL );
	if( !dag->Add( *node ) ) {
		failReason = "unknown failure adding ";
		failReason += ( node->GetType() == NodeType::FINAL )? "Final " : "";
		failReason += ( node->GetType() == NodeType::SERVICE )? "SERVICE " : "";
		failReason += "node to DAG";
		delete node;
		return NULL;
	}
	failReason = "n/a";
	return node;
}

bool
SetNodeDagFile( Dag *dag, const char *nodeName, const char *dagFile, 
            std::string &whynot )
{
	Node *node = dag->FindNodeByName( nodeName );
	if ( node ) {
		node->SetDagFile( dagFile );
		return true;
	} else {
		whynot = "Node " + std::string(nodeName) + " not found!";
		return false;
	}
}

bool
IsValidNodeName( Dag *dag, const char *name, std::string &whynot )
{
	if( name == NULL ) {
		whynot = "missing node name";
		return false;
	}
	if( strlen( name ) == 0 ) {
		whynot = "empty node name (name == \"\")";
		return false;
	}
	if( isReservedWord( name ) ) {
		whynot = "invalid node name: '" + std::string(name) + "'" +
			"is a DAGMan reserved word";
		return false;
	}
	ASSERT( dag != NULL );
	if( dag->NodeExists( name ) ) {
		whynot = "node name '" + std::string(name) + "' already exists in DAG";
		return false;
	}
	return true;
}

bool
IsValidSubmitName( const char *name, std::string &whynot )
{
	if( name == NULL ) {
		whynot = "missing submit file name";
		return false;
	}
	if( strlen( name ) == 0 ) {
		whynot = "empty submit file name (name == \"\")";
		return false;
	}
	return true;
}
