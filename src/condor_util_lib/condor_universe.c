
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

const char * CondorUniverseName( int u )
{
	if( u<=CONDOR_UNIVERSE_MIN || u>=CONDOR_UNIVERSE_MAX ) {
		return "UNKNOWN";
	} else {
		return names[u];
	}
}
