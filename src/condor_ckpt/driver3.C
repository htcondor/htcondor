#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "condor_fix_fcntl.h"
#include "image.h"

extern "C" int machine_name( char *, int );

int
main( int argc, char *argv[] )
{
	int		i;
	char	buf[1024];


	if( argc > 1 ) {
		restart();
	}
	for( i=0; i<10; i++ ) {
		machine_name( buf, sizeof(buf) );
		printf( "\nExecuting on \"%s\"\n", buf );
		printf( "i = %d\n", i );
		ckpt();
	}

		// Must exit with a status rather than return status for AIX 3.2
	exit( 13 );

		// but, must have a return to match our prototype...
	return 0;	// Can never get here
}
