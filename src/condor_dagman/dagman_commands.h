#ifndef DAGMAN_COMMANDS_H
#define DAGMAN_COMMANDS_H

/*
#include "condor_common.h"
#include "types.h"
#include "debug.h"
#include "script.h"
*/

// pause DAGMan's event-processing so changes can be made safely
bool PauseDag();
// resume DAGMan's normal event-processing
bool ResumeDag();

bool AddNode( const char *name, const char* cmd, const char *precmd,
			  const char *postcmd );
bool IsValidNodeName( const char *name, MyString &whynot );

/*
bool RemoveNode( const char *name );
bool AddDependency( const char *parentName, const *childName );
bool RemoveDependency( const char *parentName, const *childName );
*/

#endif	// ifndef DAGMAN_COMMANDS_H
