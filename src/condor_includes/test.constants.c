#define _POSIX_SOURCE

#include "condor_constants.h"

int
main( argc, argv )
int		argc;
char	*argv[];
{
	printf( "TRUE = %d\n", TRUE );
	printf( "FALSE = %d\n", FALSE );
	printf( "MINUTE = %d\n", MINUTE );
	printf( "HOUR = %d\n", HOUR );
	printf( "DAY = %d\n", DAY );
	return 0;
}
