#include "condor_common.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "daemon.h"
#include "dc_starter.h"

int sendCmd( const char* starter_addr );


int
main( int argc, char* argv[] )
{
	config();
	switch( argc ) {
	case 2:
		return sendCmd( argv[1] );
		break;

	default:
		fprintf( stderr, "Usage: %s starter_address\n",
				 argv[0] );
		break;
	}
	return 1;
}

int
sendCmd( const char* starter_addr )
{
	DCStarter d( starter_addr );

	ClassAd req;
	ClassAd reply;
	ReliSock rsock;
	
	if( ! d.reconnect(&req, &reply, &rsock) ) {
		fprintf( stderr, "command failed: (%s) %s\n",
				 getCAResultString(d.errorCode()), d.error() );
	}

	printf( "reply ad:\n" );
	reply.fPrint( stdout );

	return 0;
}
