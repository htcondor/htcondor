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
