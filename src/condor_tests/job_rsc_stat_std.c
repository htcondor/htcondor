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


#define _CONDOR_ALLOW_OPEN 1

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

void display_stat( const char *name, struct stat *buf );
void do_it( const char *name );
int stat_eq( struct stat *buf1, struct stat *buf2 );

int
main( int argc, char *argv[] )
{
	int		i;

	for( i=1; i<argc; i++ ) {
		do_it( argv[i] );
	}

	printf( "Normal End Of Job\n" );
	exit( 0 );
}

void
do_it( const char *name )
{
	struct stat	buf1;
	struct stat	buf2;
	int		fd;

		/* Get the data with stat() */
	memset( &buf1, '\0', sizeof(buf1) );
	if( stat(name,&buf1) < 0 ) {
		perror( "stat" );
		exit( __LINE__ );
	}

		/* Get the data with lstat */
	memset( &buf2, '\0', sizeof(buf2) );
	if( lstat(name,&buf2) < 0 ) {
		perror( "stat" );
		exit( __LINE__ );
	}

	if( !stat_eq(&buf1,&buf2) ) {
		fprintf( stderr, "Warning: stat & lstat buffers differ\n" );
		fprintf( stderr, "   -- ( %s is perhaps a symbolic link? )\n",name);
	}
	printf("Output from lstat():\n");
	display_stat( name, &buf2 );

		/* Get the data with fstat() */
	memset( &buf2, '\0', sizeof(buf2) );
	if( (fd=open(name,O_RDONLY)) < 0 ) {
		perror( name );
		exit( __LINE__ );
	}
	if( fstat(fd,&buf2) < 0 ) {
		perror( "fstat" );
		exit( __LINE__ );
	}
	close( fd );

	if( !stat_eq(&buf1,&buf2) ) {
		fprintf( stderr, "Error, stat and fstat buffers differ\n" );
		exit( __LINE__ );
	}

	printf("Output from stat():\n");
	display_stat( name, &buf1 );
}

void
display_stat( const char *name, struct stat *buf )
{
	printf( "%s:\n", name );
	printf( "  st_mode = 0%o\n", buf->st_mode & 0777 );
	printf( "  st_ino = %ld\n", (long)buf->st_ino );
	printf( "  st_dev = %d\n", (int)buf->st_dev );
	printf( "  st_nlink = %d\n", (int)buf->st_nlink );
	printf( "  st_uid = %d\n", (int)buf->st_uid );
	printf( "  st_gid = %d\n", (int)buf->st_gid );
	printf( "  st_size = %ld\n", (long)buf->st_size );
	printf( "  st_atime = %d\n", (int)buf->st_atime );
	printf( "  st_mtime = %d\n", (int)buf->st_mtime );
	printf( "  st_ctime = %d\n", (int)buf->st_ctime );
}

#define COMP(field) 						\
	if( buf1->field != buf2->field ) {		\
		printf( "%s differs\n", #field );	\
		return FALSE;						\
	}

int
stat_eq( struct stat *buf1, struct stat *buf2 )
{
	COMP(st_mode) 
	COMP(st_ino)
	COMP(st_dev)
	COMP(st_nlink)
	COMP(st_uid)
	COMP(st_gid)
	COMP(st_size)
/* The access time is likely to change between stats */
/*	COMP(st_atime) */
	COMP(st_mtime)
	COMP(st_ctime)

	return TRUE;
}
