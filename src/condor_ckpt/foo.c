#include <stdio.h>

#define DATA_ALIGN 0x400
#if 1
#define ALIGN(x)	(((x)+DATA_ALIGN-1)/DATA_ALIGN*DATA_ALIGN)
#else
#define ALIGN(x)	(((x)+DATA_ALIGN-1) & ~(DATA_ALIGN-1))
#endif

int
main( int argc, char *argv[] )
{
	char	**ptr;

	for( ptr=++argv; *ptr; ptr++ ) {
		do_it( *ptr );
	}
	return 0;
}

do_it( char *str )
{
	int		val;
	int		result;

	val = atoi(str);

	result = ALIGN(val);
	printf( "%d rounds up to %d\n", val, result );

}
