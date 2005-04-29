/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
		 const char* cmd,
		 const char *precmd, const char *postcmd, bool done,
		 MyString &failReason )
{
	MyString why;
	if( !IsValidNodeName( dag, name, why ) ) {
		failReason = why;
		return false;
	}
	if( !IsValidSubmitFileName( cmd, why ) ) {
		failReason = why;
		return false;
	}
	Job* node = new Job( type, name, cmd );
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
	if( done ) {
		node->SetStatus( Job::STATUS_DONE );
	}
	ASSERT( dag != NULL );
	if( !dag->Add( *node ) ) {
		failReason = "unknown failure adding node to DAG";
		delete node;
		return false;
	}
	failReason = "n/a";
	return true;
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
	if( isKeyWord( name ) ) {
		whynot.sprintf( "invalid node name: '%s' is a DAGMan keyword", name );
		return false;
	}
	ASSERT( dag != NULL );
	if( dag->NodeExists( name ) ) {
		whynot.sprintf( "node name '%s' already exists in DAG", name );
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
