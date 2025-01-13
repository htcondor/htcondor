#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "dc_coroutines.h"
using namespace condor;


dc::void_coroutine
test_00() {
    pid_t parent_pid = getpid();

    pid_t child_pid = fork();
    ASSERT( child_pid != -1 );

    if( child_pid == 0 ) {
        sleep(10);
        kill( parent_pid, SIGUSR1 );
        DC_Exit(0);
    }

    dc::AwaitableDeadlineSignal ads;
    ASSERT(ads.deadline( SIGUSR1, 20 ));

    dc::AwaitableDeadlineSignal ads2;
    ASSERT(ads2.deadline( SIGUSR1, 20 ));

    auto [the_signal, timed_out] = co_await( ads );
    ASSERT(the_signal == SIGUSR1);
    ASSERT(! timed_out);

    time_t now = time(NULL);
    auto [second_signal, second_to] = co_await( ads2 );
    ASSERT(the_signal == SIGUSR1);
    ASSERT(! second_to);

    // Make sure that we didn't actually block.
    ASSERT( (time(NULL) - now) < 5 );

    dc::AwaitableDeadlineSignal ads3;
    ASSERT(ads3.deadline( SIGUSR1, 10 ));

    auto [third_signal, third_to] = co_await(ads3);
    ASSERT(third_to);

    dprintf( D_TEST, "Passed test 00.\n" );

    DC_Exit( 0 );
}


dc::void_coroutine
test_components() {
    dc::AwaitableDeadlineSocket wibble;
    Sock * sock = NULL;


    // Start with making sure that the IDs are unique.
    auto destroyer = []() -> void { return; };

    auto handler0 = [=](int /* signal */) -> int {
        dprintf( D_TEST, "Handler #0 called.\n" );
        return 0;
    };
    int signalID0 = daemonCore->Register_Signal( SIGUSR2, "SIGUSR2",
        handler0, "registration check 0", destroyer
    );

    auto handler1 = [=](int /* signal */) -> int {
        dprintf( D_TEST, "Handler #1 called.\n" );
        return 0;
    };
    int signalID1 = daemonCore->Register_Signal( SIGUSR2, "SIGUSR2",
        handler1, "registration check 1", destroyer
    );

    ASSERT(signalID0 != signalID1);

    // This will trigger the registration checks above, but only after
    // we return into the event loop, which will happen in test_00().
    ASSERT(kill( getpid(), SIGUSR2 ) == 0);

    wibble.deadline( sock, 1 );
    std::ignore = co_await(wibble);
    dprintf( D_TEST, "--- separator ---\n" );


    // Then test partial cancellation...
    daemonCore->Cancel_Signal( SIGUSR2, signalID0 );
    ASSERT(kill( getpid(), SIGUSR2 ) == 0);

    wibble.deadline( sock, 1 );
    std::ignore = co_await(wibble);
    dprintf( D_TEST, "--- separator ---\n" );


    // ... and ID re-use.
    auto handler2 = [=](int /* signal */) -> int {
        dprintf( D_TEST, "Handler #2 called.\n" );
        return 0;
    };
    int signalID2 = daemonCore->Register_Signal( SIGUSR2, "SIGUSR2",
        handler2, "registration check 2", destroyer
    );
    ASSERT(signalID0 == signalID2);
    ASSERT(kill( getpid(), SIGUSR2 ) == 0);

    wibble.deadline( sock, 1 );
    std::ignore = co_await(wibble);
    dprintf( D_TEST, "--- separator ---\n" );


    // Repeate for the second slot, just in case.
    daemonCore->Cancel_Signal( SIGUSR2, signalID1 );
    ASSERT(kill( getpid(), SIGUSR2 ) == 0);

    wibble.deadline( sock, 1 );
    std::ignore = co_await(wibble);
    dprintf( D_TEST, "--- separator ---\n" );


    // ... and ID re-use.
    auto handler3 = [=](int /* signal */) -> int {
        dprintf( D_TEST, "Handler #3 called.\n" );
        return 0;
    };
    int signalID3 = daemonCore->Register_Signal( SIGUSR2, "SIGUSR2",
        handler3, "registration check 3", destroyer
    );
    ASSERT(signalID1 == signalID3);
    ASSERT(kill( getpid(), SIGUSR2 ) == 0);

    wibble.deadline( sock, 1 );
    std::ignore = co_await(wibble);
    dprintf( D_TEST, "--- separator ---\n" );

    //
    // Test AwaitableDeadlineSignal.
    //
    test_00();
}


//
// test_main()
//

int
test_main( int /* argv */, char ** /* argv */ ) {
    // Initialize the config settings.
    config_ex( CONFIG_OPT_NO_EXIT );

    // Allow uid switching if root.
    set_priv_initialize();

    // Initialize the config system.
    config();


    //
    // Explicitly test a few components before testing
    // AwaitableDeadlineSignal proper.
    //
    test_components();


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
