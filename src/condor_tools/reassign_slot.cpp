#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "dc_schedd.h"

int usage( const char * self ) {
	fprintf( stderr, "usage: %s <from-job> <to-job>\n", self );
	return 1;
}

int main( int argc, char ** argv ) {
	if( argc != 3 ) { return usage( argv[0] ); }

	char * endptr;
	PROC_ID vid, bid;

	vid.cluster = strtol( argv[1], & endptr, 10 );
	if( endptr == argv[1] ) { return usage( argv[0] ); }
	if( endptr[0] == '\0' ) { vid.proc = 0; }
	else if( endptr[0] == '.' ) { vid.proc = strtol( ++endptr, NULL, 10 ); }
	else { return usage( argv[0] ); }

	bid.cluster = strtol( argv[2], & endptr, 10 );
	if( endptr == argv[2] ) { return usage( argv[0] ); }
	if( endptr[0] == '\0' ) { bid.proc = 0; }
	else if( endptr[0] == '.' ) { bid.proc = strtol( ++endptr, NULL, 10 ); }
	else{ return usage( argv[0] ); }

	config();

	DCSchedd schedd( NULL /* schedd name */, NULL /* pool name */ );
	if( schedd.locate() == false ) {
		fprintf( stderr, "Unable to locate schedd, aborting.\n" );
		return 1;
	}

	// hack, hack, hack; should be a method on DCSchedd, if we cared.
	ReliSock sock;
	CondorError errorStack;
	schedd.connectSock( & sock, 20, & errorStack );
	schedd.startCommand( REASSIGN_SLOT, & sock, 20, & errorStack );
	schedd.forceAuthentication( & sock, & errorStack );

	ClassAd request;
	request.Assign( "Victim" ATTR_CLUSTER_ID, vid.cluster );
	request.Assign( "Victim" ATTR_PROC_ID, vid.proc );
	request.Assign( "Beneficiary" ATTR_CLUSTER_ID, bid.cluster );
	request.Assign( "Beneficiary" ATTR_PROC_ID, bid.proc );

	sock.encode();
	putClassAd( & sock, request );
	sock.end_of_message();

	ClassAd reply;

	sock.decode();
	getClassAd( & sock, reply );
	sock.end_of_message();

	bool result;
	reply.LookupBool( ATTR_RESULT, result );
	if(! result) {
		std::string errorString;
		reply.LookupString( ATTR_ERROR_STRING, errorString );
		fprintf( stderr, "%s\n", errorString.c_str() );
		return 1;
	}

	return 0;
}
