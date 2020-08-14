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


 

/* Generate a coredump. */

#if defined( LINUX )
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int
main()
{
	int i;
	int j;
	char	*foo = (char *)0;

	j = 0;

	printf( "About to divide by zero\n");
	fflush( stdout );
	fsync( fileno(stdout) );

	i = 1 / j;

	printf( "Error: didn't coredump from divide by zero!!!\n");
	fflush( stdout );
	fsync( fileno(stdout) );

	*foo = 'a';

	printf( "Error: didn't coredump from dereferencing NULL!!!\n");
	fflush( stdout );
	fsync( fileno(stdout) );

	/* let's try this */
	abort();

	exit( 1 );
}

