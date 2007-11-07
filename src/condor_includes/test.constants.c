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
