#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "daemon.h"
#include "dc_collector.h"

#define E_SUCCESS         0
#define E_FIXTURE_FAILURE 1
#define E_IO_FAILURE      2
#define E_SERVER_FAILURE  3
#define E_INTERNAL_ERROR -1

int
test_until_stage( int stage, int iteration ) {
    int rv = -1;
    if( stage > 7 ) {
        fprintf( stderr, "Invalid stage specified: %d, aborting.\n", stage );
        DC_Exit(E_INTERNAL_ERROR);
    }

    CondorError error;
    Daemon * ccbServer = new Daemon( DT_COLLECTOR, "127.0.0.1", NULL );
    Sock * socketToBroker = ccbServer->startCommand(
        CCB_BATCH_REQUEST, Stream::reli_sock,
        DEFAULT_CEDAR_TIMEOUT, & error );
    if(! socketToBroker ) {
        fprintf( stderr, "test_until_stage(%d, %d): startCommand() failed: %s\n", stage, iteration, error.getFullText().c_str() );

        delete ccbServer;
        return E_FIXTURE_FAILURE;
    }


    // Test 1: failure to send a ClassAd or end-of-message.
    if( stage == 1 ) {
        socketToBroker->close();

        delete ccbServer;
        return E_SUCCESS;
    }


    // Test 2: send an invalid ClassAd.
    // Test 3: send a valid ClassAd with a different count of CCB IDs and connect IDs.
    // Test 4: send a valid ClassAd with an invalid CCB ID.
    // Test 5: send a valid ClassAd with an unregistered CCB ID, then hang up.
    // Test 6: send a valid ClassAd with a registered CCB ID, then hang up.
    ClassAd command;
    switch( stage ) {
        case 2:
            break;

        case 3:
            command.Assign( ATTR_CCBID, "ccbID1" );
            command.Assign( ATTR_CLAIM_ID, "connectID1,connectID2" );
            command.Assign( ATTR_NAME, "ccbNameString" );
            command.Assign( ATTR_MY_ADDRESS, "returnAddress" );
            break;

        case 4:
            command.Assign( ATTR_CCBID, "ccbID1" );
            command.Assign( ATTR_CLAIM_ID, "connectID1" );
            command.Assign( ATTR_NAME, "ccbNameString" );
            command.Assign( ATTR_MY_ADDRESS, "returnAddress" );
            break;

        case 5:
        case 7:
            command.Assign( ATTR_CCBID, "127.0.0.1:9618?addrs=127.0.0.1-9618#9999" );
            command.Assign( ATTR_CLAIM_ID, "connectID1" );
            command.Assign( ATTR_NAME, "ccbNameString" );
            command.Assign( ATTR_MY_ADDRESS, "returnAddress" );
            break;

        case 6:
            command.Assign( ATTR_CCBID, "256.0.0.1:9618?addrs=256.0.0.0.1-9618#1" );
            command.Assign( ATTR_CLAIM_ID, "connectID1" );
            command.Assign( ATTR_NAME, "ccbNameString" );
            command.Assign( ATTR_MY_ADDRESS, "returnAddress" );
            break;

        default:
            fprintf( stderr, "Unknown stage specified.\n" );
            return -1;
    }

    socketToBroker->encode();
    rv = putClassAd( socketToBroker, command );
    if(! rv) {
        fprintf( stderr, "test_until_stage(%d, %d): putClassAd() failed, aborting\n", stage, iteration );
        return E_IO_FAILURE;
    }
    rv = socketToBroker->end_of_message();
    if(! rv) {
        fprintf( stderr, "test_until_stage(%d, %d): end_of_message() failed on send, aborting\n", stage, iteration );
        return E_IO_FAILURE;
    }

    if( stage <= 6 ) {
        socketToBroker->close();

        delete ccbServer;
        return E_SUCCESS;
    }


    // We do NOT test sending a valid ClassAd with a registered CCB ID and
    // then properly waiting for the reply; we'll test that when do the end-
    // to-end test; we'll look for server-side memory leaks in the usual
    // (successful) case then, at scale.


    // Test 7: send a valid ClassAd with an unregistered CCB ID and then
    //         properly wait for the reply.
    ClassAd reply;
    socketToBroker->decode();
    rv = getClassAd( socketToBroker, reply );
    if(! rv) {
        fprintf( stderr, "test_until_stage(%d, %d): getClassAd() failed, aborting\n", stage, iteration );
        return E_IO_FAILURE;
    }
    rv = socketToBroker->end_of_message();
    if(! rv) {
        fprintf( stderr, "test_until_stage(%d, %d): end_of_message() failed on receive, aborting\n", stage, iteration );
        return E_IO_FAILURE;
    }

    std::string failedConnectIDsString;
    if(! reply.LookupString( ATTR_CLAIM_ID, failedConnectIDsString )) {
        fprintf( stderr, "test_until_stage(%d, %d): failed to look up ATTR_CLAIME ID in response, aborting\n", stage, iteration );
        return E_SERVER_FAILURE;
    }

    if( failedConnectIDsString != "connectID1" ) {
        fprintf( stderr, "test_until_stage(%d, %d): ATTR_CLAIM_ID was wrong, aborting\n", stage, iteration );
        return E_SERVER_FAILURE;
    }

    return E_SUCCESS;
}

int
test_main( int /* argc */, char ** /* argv */, int iteration ) {
    int rv;

    rv = test_until_stage(1, iteration);
    if( rv != E_SUCCESS ) {
        fprintf( stderr, "Test 1 FAILED.\n" );
        return rv;
    }

    rv = test_until_stage(2, iteration);
    if( rv != E_SUCCESS ) {
        fprintf( stderr, "Test 2 FAILED.\n" );
        return rv;
    }

    rv = test_until_stage(3, iteration);
    if( rv != E_SUCCESS ) {
        fprintf( stderr, "Test 3 FAILED.\n" );
        return rv;
    }

    rv = test_until_stage(4, iteration);
    if( rv != E_SUCCESS ) {
        fprintf( stderr, "Test 4 FAILED.\n" );
        return rv;
    }

    rv = test_until_stage(5, iteration);
    if( rv != E_SUCCESS ) {
        fprintf( stderr, "Test 5 FAILED.\n" );
        return rv;
    }

    rv = test_until_stage(6, iteration);
    if( rv != E_SUCCESS ) {
        fprintf( stderr, "Test 6 FAILED.\n" );
        return rv;
    }

    rv = test_until_stage(7, iteration);
    if( rv != E_SUCCESS ) {
        fprintf( stderr, "Test 7 FAILED.\n" );
        return rv;
    }

    return E_SUCCESS;
}

void
main_config() {}

int _argc;
char ** _argv;

void
main_init( int /* argc */, char ** /* argv */ ) {
    unsigned int iterations = 1000;
    for( unsigned int i = 0; i < iterations; ++i ) {
        int rv = test_main( _argc, _argv, i );
        if( rv != E_SUCCESS ) { DC_Exit( rv ); }
    }
	DC_Exit( E_SUCCESS );
}

void
main_pre_command_sock_init() {
}

void
main_pre_dc_init( int /* argc */, char ** /* argv */ ) {
}

void
main_shutdown_fast() {
	DC_Exit( 0 );
}

void
main_shutdown_graceful() {
	DC_Exit( 0 );
}


int
main( int argc, char ** argv ) {
	set_mySubSystem( "TOOL", SUBSYSTEM_TYPE_TOOL );

	// This is dumb, but easier than fighting daemon core about parsing.
	_argc = argc;
	_argv = (char **)malloc( argc * sizeof( char * ) );
	for( int i = 0; i < argc; ++i ) {
		_argv[i] = strdup( argv[i] );
	}

	// This is also dumb, but less dangerous than (a) reaching into daemon
	// core to set a flag and (b) hoping that my command-line arguments and
	// its command-line arguments don't conflict.
	char ** dcArgv = (char **)malloc( 6 * sizeof( char * ) );
	dcArgv[0] = argv[0];
	// Force daemon core to run in the foreground.
	dcArgv[1] = strdup( "-f" );
	// Disable the daemon core command socket.
	dcArgv[2] = strdup( "-p" );
	dcArgv[3] = strdup(  "0" );

	dcArgv[4] = strdup( "-log" );
    std::string testLogDir = "test.log.d";
	mkdir( testLogDir.c_str(), 0755 );
	dcArgv[5] = strdup( testLogDir.c_str() );

	argc = 6;
	argv = dcArgv;

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	return dc_main( argc, argv );
}
