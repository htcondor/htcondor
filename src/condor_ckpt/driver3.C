#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "image.h"

int
main( int argc, char *argv[] )
{
	int		i;
	char	buf[1024];


	if( argc > 1 ) {
		restart();
	}
	for( i=0; i<10; i++ ) {
		gethostname( buf, sizeof(buf) );
		printf( "\nExecuting on \"%s\"\n", buf );
		printf( "i = %d\n", i );
		ckpt();
	}
	return 13;
}
