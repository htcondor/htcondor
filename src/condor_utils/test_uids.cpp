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
	gid_t groups[NGROUPS_MAX];
	pid_t pid;
	FILE* fp;
	int i, num_groups;

	ruid = getuid();
	euid = geteuid();
	rgid = getgid();
	egid = getegid();
	num_groups = getgroups( NGROUPS_MAX, groups );

	pid = getpid();

	char filename[64];


	printf( "real uid: %d\n", (int)ruid );
	printf( "real gid: %d\n", (int)rgid );
	printf( "effective uid: %d\n", (int)euid );
	printf( "effective gid: %d\n", (int)egid );
	printf( "groups: " );
	for( i=0; i<num_groups; i++ ) {
		printf( "%d ", (int)groups[i] );
	}
	printf( "\n" );

	sprintf( filename, "/tmp/test-uids.%d", (int)pid );
	printf( "trying to write info to %s\n", filename );

	fp = safe_fopen_wrapper( filename, "w" );
	if( ! fp ) {
		fprintf( stderr, "Can't open %s: errno %d (%s)\n",
				 filename, errno, strerror(errno) );
		return 1;
	}
	fprintf( fp, "real uid: %d\n", (int)ruid );
	fprintf( fp, "real gid: %d\n", (int)rgid );
	fprintf( fp, "effective uid: %d\n", (int)euid );
	fprintf( fp, "effective gid: %d\n", (int)egid );
	fprintf( fp, "groups: " );
	for( i=0; i<num_groups; i++ ) {
		fprintf( fp, "%d ", (int)groups[i] );
	}
	fprintf( fp, "\n" );

	fclose( fp );
	return 0;
}
