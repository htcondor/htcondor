
#include "condor_common.h"
#include "condor_universe.h"

/*
Warning: This table should stay in sync
with the symbols in condor_universe.h
*/

static char *names[] = {
	"NULL",
	"STANDARD",
	"PIPE",
	"LINDA",
	"PVM",
	"VANILLA",
	"PVMD",
	"SCHEDULER",
	"MPI",
	"GLOBUS",
	"JAVA",
	"PARALLEL",
};

const char*
CondorUniverseName( int u )
{
	if( u<=CONDOR_UNIVERSE_MIN || u>=CONDOR_UNIVERSE_MAX ) {
		return "UNKNOWN";
	} else {
		return names[u];
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
	if( stricmp(univ,"globus") == MATCH ) {
		return CONDOR_UNIVERSE_GLOBUS;
	}
	if( stricmp(univ,"java") == MATCH ) {
		return CONDOR_UNIVERSE_JAVA;
	}
	if( stricmp(univ,"parallel") == MATCH ) {
		return CONDOR_UNIVERSE_MPI;
	}
	return 0;
}
