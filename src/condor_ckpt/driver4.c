#define _POSIX_SOURCE

#include <stdio.h>

void ckpt();
void restart();

int		I;


int
main( int argc, char *argv[] )
{
	int		i;
	int		j;


	if( argc > 1 && strcmp(argv[1],"__restart__") == 0 ) {
		restart();
	}
	for( i=0; i<10; i++,I++ ) {
		for( j = 0; j < argc; j++ ) {
			printf( "Argv[%d] is \"%s\"\n", j, argv[j] );
		}
		printf( "i = %d\n", i );
		printf( "I = %d\n", I );
		ckpt();
	}
	exit( 0 );
}
