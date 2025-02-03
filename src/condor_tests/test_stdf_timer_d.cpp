// Require to be a daemon core daemon.
#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"

// For dprintf().
#include "condor_debug.h"


void
c_function(int /* timerID */) {
    dprintf( D_ALWAYS, "c function timer fired.\n" );
    DC_Exit(1);
}

void
main_init( int, char ** ) {
	dprintf( D_ALWAYS, "main_init()\n" );

    StdTimerHandler std_function = [=](int) {
        dprintf( D_ALWAYS, "std::function timer fired.\n" );
        DC_Exit(0);
    };

    const int once = 0;
    daemonCore->Register_Timer(
        20, once,
        std_function, "std::function timer"
    );

    daemonCore->Register_Timer(
        40, once,
        c_function,
        "c function timer"
    );
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

int
main( int argc, char * argv [] ) {
	set_mySubSystem( "TEST_STDF_TIMER_D", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;

	return dc_main( argc, argv );
}
