// dagman_commands.C

#include "condor_common.h"
#include "dagman_main.h"
#include "debug.h"
#include "parse.h"
#include "dagman_commands.h"

bool
PauseDag() {
	if( G.paused == true ) {
			// maybe this should be a warning rather than an error,
			// but it probably indicates that the caller doesn't know
			// what the h*** is going on, so I'd rather catch it here
		debug_printf( DEBUG_NORMAL, "ERROR: PauseDag() called on an "
					  "already-paused DAG\n" );
		return false;
	}
	debug_printf( DEBUG_NORMAL, "DAGMan event-processing paused...\n" );
	G.paused = true;
	return true;
}

bool
ResumeDag() {
	if( G.paused != true ) {
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
	G.paused = false;
	return true;
}

bool
AddNode( const char *name, const char* cmd, const char *precmd,
		 const char *postcmd, MyString &failReason )
{
	MyString why;
	if( !IsValidNodeName( name, why ) ) {
		failReason = "invalid node name: " + why;
		return false;
	}
	if( !cmd ) {
		failReason = "missing cmd (NULL)";
		return false;
	}
	Job* node = new Job( name, cmd );
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
	failReason = "n/a";
	return true;
}

bool
IsValidNodeName( const char *name, MyString &whynot )
{
	if( name == NULL ) {
		whynot = "name == NULL";
		return false;
	}
	if( strcmp( name, "" ) == 0 ) {
		whynot = "name is empty";
		return false;
	}
	if( isKeyWord( name ) ) {
		whynot.sprintf( "'%s' is a DAGMan keyword", name );
		return false;
	}
	ASSERT( G.dag != NULL );
	if( G.dag->NodeExists( name ) ) {
		whynot.sprintf( "node '%s' already exists in DAG", name );
		return false;
	}
	return true;
}
