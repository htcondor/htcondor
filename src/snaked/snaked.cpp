#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "snake.h"


void
main_init( int, char * [] ) {
	dprintf( D_ALWAYS, "main_init()\n" );

	std::string snake_path;
	param( snake_path, "SNAKE_PATH" );
	ASSERT(! snake_path.empty());
	Snake * snake = new Snake( snake_path );
	if(! snake->init()) {
		EXCEPT( "Failed to initialize snake, aborting.\n" );
	}

	// For now, just register the collector command whose payload style
	// we know how to handle.
	daemonCore->Register_CommandWithPayload(
		QUERY_STARTD_ADS, "QUERY_PYTHON_FUNCTION",
		(CommandHandlercpp) & Snake::HandleUnregisteredCommand,
		"Snake::HandleUnregisteredCommand",
		snake,
		READ
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
	set_mySubSystem( "SNAKED", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;

	return dc_main( argc, argv );
}
