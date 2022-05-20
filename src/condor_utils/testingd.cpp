// Require to be a daemon core daemon.
#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"

// For dprintf().
#include "condor_debug.h"

#include <stdio.h>

void
main_init( int, char * [] ) {
	dprintf( D_ALWAYS, "main_init()\n" );

	char const * publicSinfulString = daemonCore->InfoCommandSinfulStringMyself( false );
	fprintf( stdout, "%s\n", publicSinfulString ? publicSinfulString : "(null)" );
	Sinful publicSinful( publicSinfulString );
	ASSERT( publicSinful.valid() );
	fprintf( stdout, "%s\n", publicSinful.getV1String() );

	char const * privateSinfulString = daemonCore->InfoCommandSinfulStringMyself( true );
	fprintf( stdout, "%s\n", privateSinfulString ? privateSinfulString : "(null)" );
	Sinful privateSinful( privateSinfulString );
	ASSERT( privateSinful.valid() );
	fprintf( stdout, "%s\n", privateSinful.getV1String() );

	DC_Exit( 0 );
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
	set_mySubSystem( "TESTING", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;

	return dc_main( argc, argv );
}
