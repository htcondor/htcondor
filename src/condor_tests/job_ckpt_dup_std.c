/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 

#include <stdio.h>
#include <sys/file.h>
#include <stdlib.h>
#include <unistd.h>


#if defined(Solaris) || defined(IRIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "x_fake_ckpt.h"

void abort_me(void);
void check( int a, int b );

void usage( char *my_name )
{
	fprintf( stderr, "Usage: %s filename\n", my_name );
	exit( 1 );
}

int
main( int argc, char **argv )
{
	int		a, b, c, x, d;

#undef WAIT_FOR_DEBUGGER
#if defined(WAIT_FOR_DEBUGGER)
	int 	wait_up = 1;
	while( wait_up )
		;
#endif

	if( argc != 2 ) {
		usage( argv[0] );
	}

		/* NOTE: comments with specific fd numbers are only valid if
		   you don't use -_condor_aggravate_bugs */  

	x = open( argv[1], O_RDONLY, 0 );		/* should get 3 */
	a = open( argv[1], O_RDONLY, 0 );		/* should get 4 */
	b = open( argv[1], O_RDONLY, 0 );		/* should get 5 */
	c = dup( b );							/* 6 becomes dup of 5 */
	close( a );								/* deallocate 4 */
	a = dup( b );							/* 4 becomes dup of 5 */
	close( b );								/* deallocate 5 */
	close( x );								/* deallocate 3 */
    d = dup2(c,15);

	/*********************************************************
	**
	** File table should look like this now:
	**
	**	0: Stdin
	**	1: Stdout
	**	2: Stderr
	**	3: Not Open
	**	4: argv[1]		(a)
	**	5: Not Open
	**	6: Dup of 4		(b)
	** 15: Dup of 6
	**
	*********************************************************/

		/* We can't check absolute fd values, since we might be
		   running in -_condor_aggravate_bugs mode */

	/* at this point a & c should dups, but not the same fd*/
	if( a == c ) {
		printf( "Error: a and c are the same: %d\n", a );
		abort_me();
		exit( 1 );
	} 

	/* d should be a dup with fd 15 */
	if( d != 15 ) {
		printf( "Error: d's not 15, it's %d\n", d );
		abort_me();
		exit( 1 );
	} 

	printf( "OK\n" );
	check( a, c );
	check( c, d );

	ckpt_and_exit();
#undef WAIT_FOR_DEBUGGER
#if defined(WAIT_FOR_DEBUGGER)
    {
	int 	wait_up = 1;
	while( wait_up )
		;
    }
#endif
	check( a, c );
	check( c, d );

	ckpt_and_exit();
	check( a, c );
	check( c, d );

	printf( "Normal End of Job\n" );
	exit( 0 );
}

/*
** We're given 2 file descriptors which are claimed to be dup's.  Seek
** one to the end, then check that the other is also seek'd to
** the same place.  For good measure, seek the other one to the beginning,
** and check to see that the first got moved to the beginning also.
*/
void check( int a, int b )
{
	int		end,result;

	end = lseek( a, 0, 2 );
	if( lseek(b,0,1) != end ) {
		printf( "Error: file %d is not a dup of file %d\n", a, b );
		abort_me();
		exit( 1 );
	}

	result = lseek( b, 0, 0 );
	if( lseek(a,0,1) != 0 ) {
		printf( "Error: file %d is not a dup of file %d\n", a, b );
		abort_me();
		exit( 1 );
	}
	printf( "OK\n" );
}

void abort_me(void)
{
	int i;
	int j;
	char	*foo = (char *)0;

	fflush( stdout );
	fflush( stderr );
	j = 0;
	i = 1 / j;
	*foo = 'a';
	exit( 13 );
}
