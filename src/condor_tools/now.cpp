#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "dc_schedd.h"

int usage( const char * self ) {
	fprintf( stderr, "usage: %s <to-job> <from-job>\n", self );
	return 1;
}

int main( int argc, char ** argv ) {
	if( argc != 3 ) { return usage( argv[0] ); }

	char * endptr;
	PROC_ID vid, bid;

	vid.cluster = strtol( argv[2], & endptr, 10 );
	if( endptr == argv[2] ) { return usage( argv[0] ); }
	if( endptr[0] == '\0' ) { vid.proc = 0; }
	else if( endptr[0] == '.' ) { vid.proc = strtol( ++endptr, NULL, 10 ); }
	else { return usage( argv[0] ); }

	bid.cluster = strtol( argv[1], & endptr, 10 );
	if( endptr == argv[1] ) { return usage( argv[0] ); }
	if( endptr[0] == '\0' ) { bid.proc = 0; }
	else if( endptr[0] == '.' ) { bid.proc = strtol( ++endptr, NULL, 10 ); }
	else{ return usage( argv[0] ); }

	config();

	DCSchedd schedd( NULL /* schedd name */, NULL /* pool name */ );
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
