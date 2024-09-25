#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "snake.h"


// This is dumb, but daemon core doesn't delete the Service object
// associated with a cpphandler when the command is cancelled, so
// we have to keep track of it ourselves.
Snake * global_snake = NULL;


void
main_config() {
	dprintf( D_ALWAYS, "main_config()\n" );

    daemonCore->Cancel_Command( QUERY_STARTD_ADS );
    if( global_snake != NULL ) {
        delete global_snake;
        global_snake = NULL;
    }

	std::string snake_path;
	param( snake_path, "SNAKE_PATH" );
	ASSERT(! snake_path.empty());
	global_snake = new Snake( snake_path );
	if(! global_snake->init()) {
		EXCEPT( "Failed to initialize snake, aborting.\n" );
	}

	// For now, just register the collector command whose payload style
	// we know how to handle.
    auto lambda = [] (int c, Stream * s) {
        return global_snake->CallPythonCommandHandler("handleCommand", c, s);
	};
	daemonCore->Register_CommandWithPayload(
		QUERY_STARTD_ADS, "QUERY_PYTHON_FUNCTION",
		lambda,
		"lambda",
		READ
	);
}


void
main_init( int, char * [] ) {
	dprintf( D_ALWAYS, "main_init()\n" );

    main_config();
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
	set_mySubSystem( "SNAKED", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;

	return dc_main( argc, argv );
}
