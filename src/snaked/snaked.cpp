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


    std::string snake_command_table;
    param( snake_command_table, "SNAKE_COMMAND_TABLE" );

    // std::views::split() is basically useless over std::string and/or
    // std::string_view in C++20 because there's no way to std::strings
    // or std::string_views back.  (I would have expect that's the _only_
    // thing you'd get back, but apparently you get _ranges_, which you
    // can't convert back to either.
    for( const auto & line : StringTokenIterator(snake_command_table, "\n" ) ) {
        auto parts = split(line, " ");
        ASSERT(parts.size() == 3 );
        const std::string & command = parts[0];
        const std::string & permission = parts[1];
        const std::string & which_python_function = parts[2];

        int c = getCommandNum(command.c_str());
        ASSERT(c != -1);

        DCpermission p = getPermissionFromString(permission.c_str());
        ASSERT(p != -1);

        std::function f = [=] (int c, Stream * s) {
            return global_snake->CallPythonCommandHandler(
                which_python_function.c_str(),
                    c, s
            );
        };
        std::string f_description;
        formatstr(f_description,
            "CallPythonCommandHandler(%s, ...)",
            which_python_function.c_str()
        );

        const bool dont_force_authentication = false;
        daemonCore->Register_Command(
            c, command.c_str(),
            f, f_description.c_str(),
            p, dont_force_authentication, STANDARD_COMMAND_PAYLOAD_TIMEOUT
        );
    }
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
