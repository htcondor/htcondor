#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"

static char *_FileName_ = __FILE__;

main( argc, argv )
int		argc;
char	*argv[];
{
	char	**ptr;

	config( argv[0], 0 );

	for( ptr = ++argv; *ptr; ptr++ ) {
		do_it( *ptr );
	}
	sleep( 2 );
}

do_it( name )
char	*name;
{
	dprintf( D_ALWAYS, "%s: %d K free\n", name, free_fs_blocks(name) );
}

int
SetSyscalls( foo )
int	foo;
{
	return foo;
}
