#include "condor_common.h"
#include "condor_debug.h"
#include "view_server.h"

//-------------------------------------------------------------

// about self
char* mySubSystem = "COLLECTOR";		// used by Daemon Core

// the heart of the collector ...
CollectorDaemon* Daemon;

//-------------------------------------------------------------

void usage(char* name)
{
	dprintf(D_ALWAYS,"Usage: %s [-f] [-b] [-t] [-p <port>]\n",name );
	exit( 1 );
}

//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
	// handle collector-specific command line args
	if(argc > 2)
	{
		usage(argv[0]);
	}
	
	Daemon=new ViewServer();
	Daemon->Init();
	return TRUE;
}

//-------------------------------------------------------------

int main_config( bool is_full )
{
	Daemon->Config();
	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	Daemon->Exit();
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	Daemon->Shutdown();
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}

