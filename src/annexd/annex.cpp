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
	const char * fileName = NULL;
	for( int i = 1; i < argc; ++i ) {
		if( is_dash_arg_prefix( argv[i], "pool", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				pool = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -pool requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "name", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				name = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "debug", 1 ) ) {
			dprintf_set_tool_debug( "TOOL", 0 );
			continue;
		} else if( argv[i][0] == '-') {
			fprintf( stderr, "%s: unrecognized option '%s'.\n", argv[0], argv[i] );
			return 1;
		} else {
			fileName = argv[i];
			continue;
		}
	}

	if( fileName == NULL ) {
		fprintf( stderr, "%s: you must specify a file containing an annex specification.\n", argv[0] );
		return 1;
	}

	FILE * file = NULL;
	bool closeFile = true;
	if( strcmp( fileName, "-" ) == 0 ) {
		file = stdin;
		closeFile = false;
	} else {
		file = safe_fopen_wrapper_follow( fileName, "r" );
		if( file == NULL ) {
			fprintf( stderr, "Unable to open annex specification file '%s'.\n", fileName );
			return 1;
		}
	}


	std::string annexDaemonName;
	param( annexDaemonName, "ANNEXD_NAME" );
	if( name == NULL ) { name = annexDaemonName.c_str(); }
	DCAnnexd annexd( name, pool );
	annexd.setSubsystem( "GENERIC" );


	ClassAd spotFleetRequest;
	CondorClassAdFileIterator ccafi;
	if(! ccafi.begin( file, closeFile, CondorClassAdFileParseHelper::Parse_json )) {
		fprintf( stderr, "Failed to start parsing spot fleet request.\n" );
		return 2;
	} else {
		int numAttrs = ccafi.next( spotFleetRequest );
		if( numAttrs <= 0 ) {
			fprintf( stderr, "Failed to parse spot fleet request, found no attributes.\n" );
			return 2;
		} else if( numAttrs > 11 ) {
			fprintf( stderr, "Failed to parse spot fleet reqeust, found too many attributes.\n" );
			return 2;
		}
	}


	if(! annexd.locate( Daemon::LOCATE_FOR_LOOKUP )) {
		char * error = annexd.error();
		if( error && error[0] != '\0' ) {
			fprintf( stderr, "%s: Can't locate annex daemon (%s).\n", argv[0], error );
		} else {
			fprintf( stderr, "%s: Can't locate annex daemon.\n", argv[0] );
		}
		return 3;
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
		return 4;
	}

	int requestVersion = -1;
	reply.LookupInteger( "RequestVersion", requestVersion );
	if( requestVersion != 1 ) {
		fprintf( stderr, "Daemon's reply had missing or unknown RequestVersion (%d).\n", requestVersion );
		return 5;
	}

	std::string bulkRequestID;
	reply.LookupString( "BulkRequestID", bulkRequestID );
	if( bulkRequestID.empty() ) {
		fprintf( stderr, "Daemon's reply did not include bulk request ID.\n" );
		return 5;
	} else {
		fprintf( stdout, "%s\n", bulkRequestID.c_str() );
	}

	return 0;
}
