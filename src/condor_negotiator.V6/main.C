#include "condor_common.h"
#include "matchmaker.h"

// for daemon core
char *mySubSystem = "NEGOTIATOR";

// the matchmaker object
Matchmaker matchMaker;

int main_init (int, char *[])
{
	// read in params
	matchMaker.initialize ();
	return TRUE;
}

int main_shutdown_graceful()
{
	exit(0);
}


int main_shutdown_fast()
{
	exit(0);
}



int main_config ()
{
	return (matchMaker.reinitialize ());
}
