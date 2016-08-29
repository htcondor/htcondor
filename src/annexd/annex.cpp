#include "condor_common.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "match_prefix.h"
#include "daemon.h"
#include "dc_annexd.h"

int
main( int argc, char ** argv ) {
	config();
	set_mySubSystem( "TOOL", SUBSYSTEM_TYPE_TOOL );

	const char * pool = NULL;
	const char * name = NULL;
	for( int i = 1; i < argc; ++i ) {
		if( is_dash_arg_prefix( argv[i], "pool", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				pool = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -pool requires an argument.\n", argv[0] );
				exit( 1 );
			}
		} else if( is_dash_arg_prefix( argv[i], "name", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				name = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -name requires an argument.\n", argv[0] );
				exit( 1 );
			}
		} else if( is_dash_arg_prefix( argv[i], "debug", 1 ) ) {
			dprintf_set_tool_debug( "TOOL", 0 );
			continue;
		} else {
			fprintf( stderr, "%s: unrecognized option '%s'.\n", argv[0], argv[i] );
			exit( 1 );
		}
	}

	std::string annexDaemonName;
	param( annexDaemonName, "ANNEXD_NAME" );
	if( name == NULL ) { name = annexDaemonName.c_str(); }
	DCAnnexd annexd( name, pool );
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
		} else if( numAttrs > 11 ) {
			fprintf( stderr, "Failed to parse spot fleet reqeust, found too many attributes.\n" );
			return 1;
		}
	}


	if(! annexd.locate( Daemon::LOCATE_FOR_LOOKUP )) {
		char * error = annexd.error();
		if( error && error[0] != '\0' ) {
			fprintf( stderr, "%s: Can't locate annex daemon (%s).\n", argv[0], error );
		} else {
			fprintf( stderr, "%s: Can't locate annex daemon.\n", argv[0] );
		}
		return 2;
	}
	dprintf( D_FULLDEBUG, "Found annex daemon at '%s'.\n", annexd.addr() );


	ClassAd reply;
	fprintf( stdout, "Sending bulk request command to daemon...\n" );
	if(! annexd.sendBulkRequest( & spotFleetRequest, & reply )) {
		char * error = annexd.error();
		if( error && error[0] != '\0' ) {
			fprintf( stderr, "Failed to send bulk request to daemon: '%s'.\n", error );
		} else {
			fprintf( stderr, "Failed to send bulk request to daemon.\n" );
		}
		return 3;
	}

	// FIXME: Print out the bulk request ID.
	

	return 0;
}
