/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

/* Generate a coredump. */

#if defined( Solaris27 ) || defined( Solaris26 )
#define __EXTENSIONS__
#endif
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

