#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"

/* Using daemoncore, you get the benefits of a logging system with dprintf
	and you can read config files automatically. To start testing
	your daemon, run it with "-f -t" until you start specifying
	a config file to use(the daemon will automatically look in
	/etc/condor_config, ~condor/condor_config, or the env var
	CONDOR_CONFIG if -t is not specifed).  -f means run in the
	foreground, -t means print the debugging output to the terminal.
*/

//-------------------------------------------------------------

// about self
char* mySubSystem = "DCSKEL";		// used by Daemon Core

//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
	dprintf(D_ALWAYS, "main_init() called\n");
	return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool is_full )
{
	dprintf(D_ALWAYS, "main_config() called\n");
	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int argc, char* argv[] )
{
		// dprintf isn't safe yet...
}

