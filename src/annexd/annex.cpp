#include "condor_common.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "daemon.h"
#include "dc_annexd.h"

int
main( int argc, char ** argv ) {
	config();
	set_mySubSystem( "TOOL", SUBSYSTEM_TYPE_TOOL );

	// FIXME: Make this the -debug option.
	dprintf_set_tool_debug( "TOOL", 0 );

	// FIXME: Add -name, -pool options.
	const char * pool = NULL;
	std::string annexDaemonName;
	param( annexDaemonName, "ANNEXD_NAME" );
	DCAnnexd annexd( annexDaemonName.c_str(), pool );
	annexd.setSubsystem( "GENERIC" );


	ClassAd spotFleetRequest;
	CondorClassAdFileIterator ccafi;
	if(! ccafi.begin( stdin, false, CondorClassAdFileParseHelper::Parse_json )) {
		fprintf( stderr, "Failed to start parsing spot fleet request.\n" );
		return 1;
	} else {
		int numAttrs = ccafi.next( spotFleetRequest );
		if( numAttrs <= 0 ) {
			fprintf( stderr, "Failed to parse spot fleet request, found no attributes.\n" );
			return 1;
		} /* else if( numAttrs > ... */
	}
	// fPrintAd( stderr, spotFleetRequest );


	if(! annexd.locate( Daemon::LOCATE_FOR_LOOKUP )) {
		char * error = annexd.error();
		if( error ) {
			fprintf( stderr, "%s: Can't locate annex daemon (%s).\n", argv[0], error );
		} else {
			fprintf( stderr, "%s: Can't locate annex daemon.\n", argv[0] );
		}
		return 2;
	}
	dprintf( D_FULLDEBUG, "Found annex daemon at '%s'.\n", annexd.addr() );


	ClassAd reply;
	if(! annexd.sendBulkRequest( & spotFleetRequest, & reply )) {
		char * error = annexd.error();
		if( error ) {
			fprintf( stderr, "Failed to send bulk request to daemon: '%s'.\n", error );
		} else {
			fprintf( stderr, "Failed to send bulk request to daemon.\n" );
		}
		return 3;
	}
	fprintf( stdout, "Sent bulk request command to daemon.\n" );

	return 0;
}
