/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

bool
PauseDag(Dagman &dm)
{
	if( dm.paused == true ) {
			// maybe this should be a warning rather than an error,
			// but it probably indicates that the caller doesn't know
			// what the h*** is going on, so I'd rather catch it here
		debug_printf( DEBUG_NORMAL, "ERROR: PauseDag() called on an "
					  "already-paused DAG\n" );
		return false;
	}
	debug_printf( DEBUG_NORMAL, "DAGMan event-processing paused...\n" );
	dm.paused = true;
	return true;
}

bool
ResumeDag(Dagman &dm)
{
	if( dm.paused != true ) {
		debug_printf( DEBUG_NORMAL, "ERROR: ResumeDag() called on an "
					  "un-paused DAG\n" );
		return false;
	}
		// TODO: if/when we have a DAG sanity-checking routine, this
		// might be a good place to call it; for now, it's the
		// responsibility of the user modifying the DAG (and to some
		// degree, the dag modification routines) to make sure they
		// get everything right...
	debug_printf( DEBUG_NORMAL, "DAGMan event-processing resuming...\n" );
	dm.paused = false;
	return true;
}

bool
AddNode( Dag *dag, Job::job_type_t type, const char *name,
		 const char* directory,
		 const char* submitFile,
		 const char *precmd, const char *postcmd, bool noop,
		 bool done, bool isFinal,
		 MyString &failReason )
{
	MyString why;
	if( !IsValidNodeName( dag, name, why ) ) {
		failReason = why;
		return false;
	}
	if( !IsValidSubmitFileName( submitFile, why ) ) {
		failReason = why;
		return false;
	}
	if( done && isFinal) {
		failReason.formatstr( "Warning: FINAL Job %s cannot be set to DONE\n",
					name );
        debug_printf( DEBUG_QUIET, "%s", failReason.Value() );
		(void)check_warning_strictness( DAG_STRICT_1, false );
		done = false;
	}
	Job* node = new Job( type, name, directory, submitFile );
	if( !node ) {
		dprintf( D_ALWAYS, "ERROR: out of memory!\n" );
			// we already know we're out of memory, so filling in
			// FailReason will likely fail, but give it a shot...
		failReason = "out of memory!";
		return false;
	}
	if( precmd ) {
		if( !node->AddPreScript( precmd, why ) ) {
			failReason = "failed to add PRE script: " + why;
			delete node;
			return false;
		}
	}
	if( postcmd ) {
		if( !node->AddPostScript( postcmd, why ) ) {
			failReason = "failed to add POST script: " + why;
			delete node;
			return false;
		}
	}
	node->SetNoop( noop );
	if( done ) {
		node->SetStatus( Job::STATUS_DONE );
	}
	node->SetFinal( isFinal );
	ASSERT( dag != NULL );
	if( !dag->Add( *node ) ) {
		failReason = "unknown failure adding ";
		failReason += isFinal? "Final " : "";
		failReason += "node to DAG";
		delete node;
		return false;
	}
	failReason = "n/a";
	return true;
}

bool
SetNodeDagFile( Dag *dag, const char *nodeName, const char *dagFile, 
            MyString &whynot )
{
	Job *job = dag->FindNodeByName( nodeName );
	if ( job ) {
		job->SetDagFile( dagFile );
		return true;
	} else {
		whynot = MyString( "Node " ) + nodeName + " not found!";
		return false;
	}
}

bool
IsValidNodeName( Dag *dag, const char *name, MyString &whynot )
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
		whynot.formatstr( "invalid node name: '%s' is a DAGMan reserved word",
					name );
		return false;
	}
	ASSERT( dag != NULL );
	if( dag->NodeExists( name ) ) {
		whynot.formatstr( "node name '%s' already exists in DAG", name );
		return false;
	}
	return true;
}

bool
IsValidSubmitFileName( const char *name, MyString &whynot )
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
