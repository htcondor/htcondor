// Require to be a daemon core daemon.
#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"

// For dprintf().
#include "condor_debug.h"

void
main_init( int argc, char ** argv ) {
	dprintf( D_ALWAYS, "main_init()\n" );

	ASSERT(argc == 3);
	std::string the_attr(argv[1]);
	ASSERT(! the_attr.empty());
	std::string the_value(argv[2]);
	ASSERT(! the_value.empty());

	std::function f = [=] (int, Stream * s) {
	    ClassAd classAd;
	    classAd.InsertAttr( "MyType", "Machine" );
	    classAd.InsertAttr( the_attr, the_value );

	    s->encode();
	    s->put(1);
	    putClassAd(s, classAd);
	    s->put(0);
	    s->end_of_message();

	    return CLOSE_STREAM;
	};

    bool dont_force_authentication = false;
	daemonCore->Register_Command(
	    QUERY_STARTD_ADS, "QUERY_STARTD_ADS",
	    f, "f",
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
	set_mySubSystem( "TEST_DC_STD_FUNCTION", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;

	return dc_main( argc, argv );
}
