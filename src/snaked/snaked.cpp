#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "dc_coroutines.h"
#include "sig_name.h"
#include "condor_environ.h"

#include "snake.h"

// We need to preserve this across the first call into daemon core.
const char * condor_inherit = NULL;

// We need to know this when we're looking up configuration.
std::string genus;

// This is dumb, but daemon core doesn't delete the Service object
// associated with a cpphandler when the command is cancelled, so
// we have to keep track of it ourselves.
Snake * global_snake = NULL;
std::vector<int> registered_commands;

// We can't change the update interval on reconfig if we don't know its ID.
int update_collector_tid = -1;
void update_collector( int /* timerID */ ) {
    global_snake->CallUpdateDaemonAd(genus);
}

void
main_config() {
    dprintf( D_ALWAYS, "main_config()\n" );

    // Strictly speaking, most of this should be in snake.cpp, because
    // the snaked shouldn't have to know any of these implementation details.

    std::string param_name;
    formatstr(param_name, "%s_UPDATE_INTERVAL", genus.c_str());
    int update_collector_interval = param_integer(
        param_name.c_str(), 5 * MINUTE
    );
    if( update_collector_tid != -1 ) {
        daemonCore->Reset_Timer( update_collector_tid, 0, update_collector_interval );
    } else {
        update_collector_tid = daemonCore->Register_Timer(
            0, update_collector_interval,
            update_collector, "update_collector"
        );
    }

    // It's not good enough to just unregister all the commands we're
    // registering this time to avoid collisions; the admin may have removed
    // a command from config, in which case it's a doubly bad idea to call it.
    for( int command : registered_commands ) {
        daemonCore->Cancel_Command(command);
    }
    registered_commands.clear();

    // The above avoids one logical error (the registered commands not matching
    // the configured command table), but there may be timers and sockets to
    // cancel, as well, if the reconfig came in during an await(), and we'd
    // also like to clean up any outstanding coroutines.
    //
    // Since the AwaitableDeadlineSocket's destructor cleans up properly,
    // we just need to destroy the coroutine.  Conveniently, doing so
    // triggers the AwaitableDeadlineSocket's destructor (as it has just
    // gone out of scope).
    for( auto [socket, service] : daemonCore->findSocketsAndServicesByDescription(
        "AwaitableDeadlineSocket::socket"
    ) ) {
        condor::dc::AwaitableDeadlineSocket * awaitable = dynamic_cast<condor::dc::AwaitableDeadlineSocket *>(service);
        if( awaitable == NULL ) {
            dprintf( D_ALWAYS, "Don't register sockets that aren't AwaitableDeadlineSockets with the name 'AwaitableDeadlineSocket::socket'!\n" );
            continue;
        }

        // This destroys the coroutine associated with this awaitable.  There
        // might be more than one awaitable associated with that coroutine,
        // but destroy() checks the handle for validity.
        //
        awaitable->destroy();
        delete socket;
    }


    //
    // As the preceeding, except for AwaitableDeadlineSignals.
    //
    for( auto [signal, service] : daemonCore->findSignalsAndServicesByDescription(
        "AwaitableDeadlineSignal::signal"
    ) ) {
        // dprintf( D_ALWAYS, "Found pending service %p for signal %d\n", service, signal );
        condor::dc::AwaitableDeadlineSignal * awaitable = dynamic_cast<condor::dc::AwaitableDeadlineSignal *>(service);
        if( awaitable == NULL ) {
            dprintf( D_ALWAYS, "Don't register sockets that aren't AwaitableDeadlineSockets with the name 'AwaitableDeadlineSocket::socket'!\n" );
            continue;
        }

        // This destroys the coroutine associated with this awaitable.  There
        // might be more than one awaitable associated with that coroutine,
        // but destroy() checks the handle for validity.
        //
        awaitable->destroy();
    }


    if( global_snake != NULL ) {
        delete global_snake;
        global_snake = NULL;
    }

    std::string snake_path;
    formatstr( param_name, "%s_SNAKE_PATH", genus.c_str() );
    param( snake_path, param_name.c_str() );
    ASSERT(! snake_path.empty());
    global_snake = new Snake( snake_path );
    if(! global_snake->init()) {
        EXCEPT( "Failed to initialize snake, aborting.\n" );
    }


    formatstr( param_name, "%s_SNAKE_COMMAND_TABLE", genus.c_str() );
    std::string snake_command_table;
    param( snake_command_table, param_name.c_str() );

    // std::views::split() is basically useless over std::string and/or
    // std::string_view in C++20 because there's no way to get std::strings
    // or std::string_views back.  (I would have expect that's the _only_
    // thing you'd get back, but apparently you get _ranges_, which you
    // can't convert back to either.)
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
            return global_snake->CallCommandHandler(
                which_python_function.c_str(),
                c, s
            );
        };
        std::string f_description;
        formatstr(f_description,
            "CallCommandHandler(%s, ...)",
            which_python_function.c_str()
        );

        const bool dont_force_authentication = false;
        daemonCore->Register_Command(
            c, command.c_str(),
            f, f_description.c_str(),
            p, dont_force_authentication, STANDARD_COMMAND_PAYLOAD_TIMEOUT
        );

        registered_commands.push_back(c);
    }


    //
    // If SNAKE_POLLING is set, register a 0-second timer to call the
    // generator immediately.  The timer handler will invoke a coroutine
    // which handles registering the signal handler and (another) timer.
    //
    formatstr( param_name, "%s_SNAKE_POLLING", genus.c_str() );
    std::string snake_polling;
    param( snake_polling, param_name.c_str() );
    if(! snake_polling.empty()) {
        auto parts = split(snake_polling, " ");
        ASSERT(parts.size() == 2 );
        const std::string & which_python_function = parts[0];
        ASSERT(! which_python_function.empty());
        const std::string & which_signal_name = parts[1];
        int which_signal = signalNumber(which_signal_name.c_str());
        ASSERT(which_signal != -1);

        std::function f = [=] (int /* timerID */) {
            global_snake->CallPollingHandler(
                which_python_function.c_str(), which_signal
            );
        };
        std::string f_description;
        formatstr(f_description,
            "CallPollingGenerator(%s, ...)",
            which_python_function.c_str()
        );

        const int timer_once = 0;
        const int timer_immediately = 0;
        int timerID = daemonCore->Register_Timer(
            timer_immediately, timer_once,
            f, f_description.c_str()
        );
        ASSERT(timerID != -1);
    }
}


void
main_init( int, char * [] ) {
	dprintf( D_ALWAYS, "main_init()\n" );

    // Reset after daemon core unsets it so that set_ready_state() will work.
    SetEnv( ENV_CONDOR_INHERIT, condor_inherit );

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
	for( int i = 0; i < argc; ++i ) {
		if( strcmp( argv[i], "-genus" ) == 0 ) {
			++i;
			if( i == argc ) {
				dprintf( D_ALWAYS, "You must specify -genus <config-name>.\n" );
				return -1;
			}
			genus = argv[i];
		}
	}
	if( genus.empty() ) {
		dprintf( D_ALWAYS, "You must specify -genus <config-name>.\n" );
		return -1;
	}
	set_mySubSystem( genus.c_str(), true, SUBSYSTEM_TYPE_DAEMON );


    // The Python bindings' implementation of set_ready_state(), which
    // we definitely want to work, requires that CONDOR_INHERIT be set
    // in the environment; daemon core, of course, unsets it after
    // properly handling it.  So we preserve it here and reset it after
    // handing off control daemon core but before we run any Python code.
    condor_inherit = GetEnv( ENV_CONDOR_INHERIT );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;

	return dc_main( argc, argv );
}
