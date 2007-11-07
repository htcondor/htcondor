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
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
	FILE* f_in;
	int floc;
	f_in = fopen( "x_data.in", "r" );
	if( !f_in ) {
		printf( "cannot open file \"x_data.in\"\n" );
	} else {
		printf( "about to call ftell()\n" );
		floc = ftell( f_in );
		if( floc != 0 )	{
			printf( "ftell fails, return value = %d, error = %d\n", 
					floc, errno );
			exit(1);

		} else {
			printf( "ftell succeeds, return value = %d\n", floc );
		}
		if ( fseek(f_in,4L,SEEK_SET) < 0 ) {
			printf( "fseek fails, return value = %d, error = %d\n", 
					floc, errno );
			exit(1);
		}
		floc = ftell( f_in );
		if( floc != 4 )	{
			printf( "ftell fails, return value = %d, error = %d\n", 
					floc, errno );
			exit(1);

		} else {
			printf( "ftell succeeds, return value = %d\n", floc );
		}

	}
	exit(0);
}
