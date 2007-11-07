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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_universe.h"

/*
Warning: This table should stay in sync
with the symbols in condor_universe.h
*/


typedef struct {
	const char	*uc;
	const char	*ucfirst;
} UniverseName;

static const UniverseName names [] =
{
	{ "NULL","NULL" },
	{ "STANDARD", "Standard" },
	{ "PIPE", "Pipe" },
	{ "LINDA", "Linda" },
	{ "PVM", "PVM" },
	{ "VANILLA", "Vanilla" },
	{ "PVMD", "PVMD" },
	{ "SCHEDULER", "Scheduler" },
	{ "MPI", "MPI" },
	{ "GLOBUS", "Globus" },
	{ "JAVA", "Java" },
	{ "PARALLEL", "Parallel" },
	{ "LOCAL", "Local" },
	{ "VM", "VM" },
};

const char*
CondorUniverseName( int u )
{
	if( u<=CONDOR_UNIVERSE_MIN || u>=CONDOR_UNIVERSE_MAX ) {
		return "UNKNOWN";
	} else {
		return names[u].uc;
	}
}

const char*
CondorUniverseNameUcFirst( int u )
{
	if( u<=CONDOR_UNIVERSE_MIN || u>=CONDOR_UNIVERSE_MAX ) {
		return "Unknown";
	} else {
		return names[u].ucfirst;
	}
}

int
CondorUniverseNumber( const char* univ )
{
	if( ! univ ) {
		return 0;
	}

	if( stricmp(univ,"standard") == MATCH ) {
		return CONDOR_UNIVERSE_STANDARD;
	}
	if( stricmp(univ,"pipe") == MATCH ) {
		return CONDOR_UNIVERSE_PIPE;
	}
	if( stricmp(univ,"linda") == MATCH ) {
		return CONDOR_UNIVERSE_LINDA;
	}
	if( stricmp(univ,"pvm") == MATCH ) {
		return CONDOR_UNIVERSE_PVM;
	}
	if( stricmp(univ,"vanilla") == MATCH ) {
		return CONDOR_UNIVERSE_VANILLA;
	}
	if( stricmp(univ,"pvmd") == MATCH ) {
		return CONDOR_UNIVERSE_PVMD;
	}
	if( stricmp(univ,"scheduler") == MATCH ) {
		return CONDOR_UNIVERSE_SCHEDULER;
	}
	if( stricmp(univ,"mpi") == MATCH ) {
		return CONDOR_UNIVERSE_MPI;
	}
		// grid replaces globus, but keeping globus for backwards compat
	if( stricmp(univ,"globus") == MATCH ||
		stricmp(univ,"grid") == MATCH ) {
		return CONDOR_UNIVERSE_GRID;
	}
	if( stricmp(univ,"java") == MATCH ) {
		return CONDOR_UNIVERSE_JAVA;
	}
	if( stricmp(univ,"parallel") == MATCH ) {
		return CONDOR_UNIVERSE_PARALLEL;
	}
	if( stricmp(univ,"local") == MATCH ) {
		return CONDOR_UNIVERSE_LOCAL;
	}
	if( stricmp(univ,"vm") == MATCH ) {
		return CONDOR_UNIVERSE_VM;
	}
	return 0;
}


BOOLEAN
universeCanReconnect( int universe )
{
	switch (universe) {
	case CONDOR_UNIVERSE_STANDARD:
	case CONDOR_UNIVERSE_PVM:
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_LOCAL:
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_GRID:
		return FALSE;
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_JAVA:
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_VM:
		return TRUE;
	default:
		EXCEPT( "Unknown universe (%d) in universeCanReconnect()", universe );
	}
	return FALSE;
}
