#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "dc_coroutines.h"
using namespace condor;


cr::Generator<bool>
test_function(int i) {
    for( ; i >= 0; --i ) {
        co_yield true;
    }
    co_yield false;
}


int main( int /* argc */, char ** /* argv */ ) {
    auto generator = test_function(3);
    while( generator() ) {
        fprintf( stderr, "generator() returned true\n" );
    }

    return 0;
}

#if defined(LATER)
int
test_main( int /* argv */, char ** /* argv */ ) {
	// Initialize the config settings.
	config_ex( CONFIG_OPT_NO_EXIT );

	// Allow uid switching if root.
	set_priv_initialize();

	// Initialize the config system.
	config();


	//
	// Test AwaitableDeadlineReaper.
	//
	test_00();

	// Wait for the test(s) to finish running.
	return 0;
}


//
// Stubs for daemon core.
//

void main_config() { }

void main_pre_command_sock_init() { }

void main_pre_dc_init( int /* argc */, char ** /* argv */ ) { }

void main_shutdown_fast() { DC_Exit( 0 ); }

void main_shutdown_graceful() { DC_Exit( 0 ); }

void
main_init( int argc, char ** argv ) {
	int rv = test_main( argc, argv );
	if( rv ) { DC_Exit( rv ); }
}


//
// main()
//
int
main( int argc, char * argv[] ) {
	set_mySubSystem( "TOOL", false, SUBSYSTEM_TYPE_TOOL );

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	return dc_main( argc, argv );
}
#endif
