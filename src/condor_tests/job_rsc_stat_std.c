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
