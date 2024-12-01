// Require to be a daemon core daemon.
#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"

// For dprintf().
#include "condor_debug.h"
// For dc::AwaitableDeadlineSocket.
#include "dc_coroutines.h"

condor::dc::void_coroutine
test_awaitable_deadline_socket(ReliSock * sock) {
	std::string testID;
	std::string buffer;


	sock->decode();

	dprintf( D_ALWAYS, "Reading first string...\n" );
	ASSERT(sock->get(testID));
	dprintf( D_ALWAYS, "Read first string: '%s'\n", testID.c_str() );

	dprintf( D_ALWAYS, "Reading first EOM...\n" );
	ASSERT(sock->end_of_message());

	sock->encode();

	dprintf( D_ALWAYS, "Writing second string...\n" );
	buffer = "first string payload";
	ASSERT(sock->put(buffer));

	dprintf( D_ALWAYS, "Writing second EOM...\n" );
	ASSERT(sock->end_of_message());


	dprintf( D_ALWAYS, "Waiting for third string.\n" );
	condor::dc::AwaitableDeadlineSocket hotOrNot;
	hotOrNot.deadline( sock, 20 );
	auto [socket, timed_out] = co_await(hotOrNot);
	ASSERT(socket = sock);

	if( timed_out ) {
		dprintf( D_ALWAYS, "[%s] Timed out waiting for third string.\n", testID.c_str() );
		co_return;
	}


	sock->decode();

	dprintf( D_ALWAYS, "Reading third string...\n" );
	ASSERT(sock->get(buffer));
	dprintf( D_ALWAYS, "Read third string: '%s'\n", buffer.c_str() );

	dprintf( D_ALWAYS, "Reading third EOM...\n" );
	ASSERT(sock->end_of_message());


	dprintf( D_ALWAYS, "[%s] Exchange complete.\n", testID.c_str() );
	delete sock;

	co_return;
}


int
test_awaitable_deadline_socket_f( int i, Stream * s ) {
	ASSERT(i == QUERY_STARTD_ADS);
	ReliSock * r = dynamic_cast<ReliSock *>(s);
	ASSERT(r != NULL );
	test_awaitable_deadline_socket(r);
	return KEEP_STREAM;
}

void
main_init( int, char ** ) {
	dprintf( D_ALWAYS, "main_init()\n" );

	bool dont_force_authentication = false;
	daemonCore->Register_Command(
		QUERY_STARTD_ADS, "QUERY_STARTD_ADS",
		test_awaitable_deadline_socket_f, "test_awaitable_deadline_socket_f",
		READ, dont_force_authentication, STANDARD_COMMAND_PAYLOAD_TIMEOUT
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
	set_mySubSystem( "AWAITABLE_DEADLINE_SOCKET", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;

	return dc_main( argc, argv );
}
