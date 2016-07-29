#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"

void
doPolling() {
	dprintf( D_ALWAYS, "doPolling()\n" );
	time_t pollingBegan = time( NULL );

	/* TODO */
}

void
main_init( int /* argc */, char ** /* argv */ ) {
	dprintf( D_ALWAYS, "main_init()\n" );

	// For use by the command-line tool.
	// daemonCore->RegisterCommand... ( ... )

	// Load our hard state and register timer(s) as appropriate.
	// ...

	// For now, just poll AWS regularly.
	// Units appear are seconds.
	unsigned delay = 0;
	unsigned period = param_integer( "ANNEX_POLL_INTERVAL", 300 );
	daemonCore->Register_Timer( delay, period, & doPolling, "poll the cloud" );
}

void
main_config() {
	dprintf( D_ALWAYS, "main_config()\n" );
}

void
main_shutdown_fast() {
	dprintf( D_ALWAYS, "main_shutdown_fast()\n" );
	DC_Exit( 0 );
}

void
main_shutdown_graceful() {
	dprintf( D_ALWAYS, "main_shutdown_graceful()\n" );
	DC_Exit( 0 );
}

void
main_pre_dc_init( int /* argc */, char ** /* argv */ ) {
	dprintf( D_ALWAYS, "main_pre_dc_init()\n" );
}

void
main_pre_command_sock_init() {
	dprintf( D_ALWAYS, "main_pre_command_sock_init()\n" );
}

int
main( int argc, char ** argv ) {
	set_mySubSystem( "ANNEXD", SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	return dc_main( argc, argv );
}
