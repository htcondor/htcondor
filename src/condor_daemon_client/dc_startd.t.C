#include "condor_common.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "daemon.h"
#include "dc_startd.h"

int sendCmd( const char* job_id, const char* name, const char* pool );

int
main( int argc, char* argv[] )
{
	config();
	switch( argc ) {
	case 2:
		return sendCmd( argv[1], NULL, NULL );
		break;

	case 3:
		return sendCmd( argv[1], argv[2], NULL );
		break;

	case 4:
		return sendCmd( argv[1], argv[2], argv[3] );
		break;

	default:
		fprintf( stderr, "Usage: %s global_job_id [startd_name] [pool]\n",
				 argv[0] );
		break;
	}
	return 1;
}

int
sendCmd( const char* job_id, const char* name, const char* pool )
{
	DCStartd d( name, pool );
	if( ! d.locate() ) {
		fprintf( stderr, "Can't locate startd: %s\n", d.error() );
		return 1;
	}

	ClassAd reply;
	if( ! d.locateStarter(job_id, &reply) ) {
		fprintf( stderr, "locateStarter() failed: %s\n", d.error() );
		fprintf( stderr, "reply ad:\n" );
		reply.fPrint( stderr );
		return 1;
	}

	printf( "locateStarter() worked!\n" );
	printf( "reply ad:\n" );
	reply.fPrint( stdout );
	return 0;
}
