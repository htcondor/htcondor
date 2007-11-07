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


#define _CONDOR_ALLOW_FOPEN 1

#if defined(HPUX)
/*#include "condor_common.h"*/
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#else
#include "condor_common.h"
/*#include <stdio.h>*/
/*#include <fcntl.h>*/
/*#include <time.h>*/
/*#include <unistd.h>*/
/*#include <stdlib.h>*/
/*#include <string.h>*/
/*#include <sys/types.h>*/
/*#include <sys/stat.h>*/
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

FILE *fdp ;

void do_it( const char *name );

int
main( int argc, char *argv[] )
{
	int		i;

	for( i=1; i<argc; i++ ) {
		printf("Arg number %d is %s\n",i,argv[i] );
	}

	fdp = fopen( argv[1], "w+");

	printf( "Normal End Of Job\n" );
	exit( 0 );
}

void
do_it( const char *name )
{
	printf("In doit............\n");
}
