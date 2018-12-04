#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "dc_schedd.h"

int usage( const char * self ) {
	fprintf( stderr,
"Usage: %s [-name <scheduler> [-pool <central-manager]] [-debug]\n"
"       %*s <job-to-run-now> <job-to-vacate>\n"
"   or: %s -help\n",
		self, (int)strlen( self ), "", self );
	return 1;
}

int main( int argc, char ** argv ) {

	set_priv_initialize(); // allow uid switching if root
	config();

	if( argc < 3 ) {
		return usage( argv[0] );
	}

	PROC_ID vid, bid;
	const char * pool = NULL, * name = NULL;

	int i = 1;
	for( ; i < argc; ++i ) {
		if( strcmp( argv[i], "-help" ) == 0 ) {
			return usage( argv[0] );
		} else if( strcmp( argv[i], "-pool" ) == 0 ) {
			++i; if( i < argc ) { pool = argv[i]; } else { return usage( argv[0] ); }
		} else if( strcmp( argv[i], "-name" ) == 0 ) {
			++i; if( i < argc ) { name = argv[i]; } else { return usage( argv[0] ); }
		} else if( strcmp( argv[i], "-debug" ) == 0 ) {
			dprintf_set_tool_debug( "TOOL", 0 );
		} else {
			break;
		}
	}

	char * endptr;

	bid.cluster = strtol( argv[i], & endptr, 10 );
	if( endptr == argv[i] ) { return usage( argv[0] ); }
	if( endptr[0] == '\0' ) { bid.proc = 0; }
	else if( endptr[0] == '.' ) { bid.proc = strtol( ++endptr, NULL, 10 ); }
	else{ return usage( argv[0] ); }
	++i;

	vid.cluster = strtol( argv[i], & endptr, 10 );
	if( endptr == argv[i] ) { return usage( argv[0] ); }
	if( endptr[0] == '\0' ) { vid.proc = 0; }
	else if( endptr[0] == '.' ) { vid.proc = strtol( ++endptr, NULL, 10 ); }
	else { return usage( argv[0] ); }
	++i;


	DCSchedd schedd( name, pool );
	if( schedd.locate() == false ) {
		fprintf( stderr, "Unable to locate schedd, aborting.\n" );
		return 1;
	}

	ClassAd reply;
	std::string errorMessage;
	if(! schedd.reassignSlot( vid, bid, reply, errorMessage )) {
		fprintf( stderr, "Unable to reassign slot: %s.\n", errorMessage.c_str() );
		return 1;
	}

	return 0;
}
