#define _POSIX_SOURCE

#include "condor_common.h"
#include "user_log.h"

main( int argc, char *argv[] )
{
	int		i;
	LP		*lp;

	int		cluster, proc, subproc;

	if( argc != 4 ) {
		fprintf( stderr, "Usage: %s cluster proc subproc\n", argv[0] );
		exit( 1 );
	}

	cluster = atoi( argv[1] );
	proc = atoi( argv[2] );
	subproc = atoi( argv[3] );

	lp =  InitUserLog( "condor", "/tmp/condor", cluster, proc, subproc );
	PutUserLog( lp, "At line %d in %s\n", __LINE__, __FILE__ );
	PutUserLog(lp, "At line %d in %s\n", __LINE__, __FILE__ );

	BeginUserLogBlock( lp );
	for( i=0; i<10; i++ ) {
		PutUserLog( lp, "This is line %d of output\n", i );
	}
	EndUserLogBlock( lp );

	exit( 0 );
}

extern "C" int SetSyscalls( int mode ) { return mode; }

