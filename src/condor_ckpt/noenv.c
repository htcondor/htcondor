#define _POSIX_SOURCE
#include <stdio.h>

int
main( argc, argv )
int		argc;
char	*argv[];
{
	char	*path = argv[1];
	char 	**args = &argv[1];
	char	*foo = NULL;
	char	**envp = &foo;

	execve( path, args, envp );
}
