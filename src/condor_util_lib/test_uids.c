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

#include "condor_common.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

int
main( int argc, char* argv[] )
{
	uid_t ruid;
	uid_t euid;
	gid_t rgid;
	gid_t egid;
	pid_t pid;
	FILE* fp;

	ruid = getuid();
	euid = geteuid();
	rgid = getgid();
	egid = getegid();

	pid = getpid();

	char filename[64];


	printf( "real uid: %d\n", (int)ruid );
	printf( "real gid: %d\n", (int)rgid );
	printf( "effective uid: %d\n", (int)euid );
	printf( "effective gid: %d\n", (int)egid );

	sprintf( filename, "/tmp/test-uids.%d", (int)pid );
	printf( "trying to write info to %s\n", filename );

	fp = fopen( filename, "w" );
	if( ! fp ) {
		fprintf( stderr, "Can't open %s: errno %d (%s)\n",
				 filename, errno, strerror(errno) );
		return 1;
	}
	fprintf( fp, "real uid: %d\n", (int)ruid );
	fprintf( fp, "real gid: %d\n", (int)rgid );
	fprintf( fp, "effective uid: %d\n", (int)euid );
	fprintf( fp, "effective gid: %d\n", (int)egid );

	fclose( fp );
	return 0;
}
