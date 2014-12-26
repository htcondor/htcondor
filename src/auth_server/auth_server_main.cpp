
#include "auth_server.h"

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "condor_debug.h"

using namespace htcondor;

AuthServer *server = NULL;

void
main_init(int /*argc*/, char * /*argv*/ [])
{
	dprintf(D_ALWAYS, "Initializing auth server.\n");
	server = new AuthServer();
	server->Reconfig();
}

void
main_config()
{
	dprintf(D_ALWAYS, "Configuring auth server.\n");
	server->Reconfig();
}

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	server->CleanUp();
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	server->CleanUp();
	DC_Exit(0);
}


void
main_pre_command_sock_init()
{
	printf("Turn off want send child alive.\n");
	daemonCore->WantSendChildAlive( false );
	daemonCore->setNeverUseCCB(true);
}


int
main(int argc, char *argv[])
{
	// Parse the subsystem type from the arguments; this allows us to emulate our
	// parent's security behavior.
	std::string mytype;
	std::string mysubsys;
	for (int idx=0; idx<argc; idx++)
	{
		if (!strcmp(argv[idx], "-type"))
		{
			if (++idx == argc) {EXCEPT("Argument required for -type");}
			mytype = argv[idx];
		}
		if (!strcmp(argv[idx], "-subsys"))
		{
			if (++idx == argc) {EXCEPT("Argument required for -subsys");}
			mysubsys = argv[idx];
		}
	}
	if (!mysubsys.size()) {EXCEPT("-subsys argument required.");}
	SubsystemInfo subsysInfo(mysubsys.c_str());
	SubsystemType mySubsysType = SUBSYSTEM_TYPE_AUTO;
	if (mytype.size())
	{
		mySubsysType = subsysInfo.setTypeFromName(mytype.c_str());
	}
	set_mySubSystem(mysubsys.c_str(), mySubsysType);

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_pre_command_sock_init = main_pre_command_sock_init;
	return dc_main( argc, argv );
}

