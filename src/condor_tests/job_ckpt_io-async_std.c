/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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



/*
  This program is intended to test the response of programs to a checkpoint
  signal while in the middle of a remote I/O operation.  It does intense
  reads and writes, so that any time it get an asynchronous checkpoint
  request, chances are high that it will be in the middle of such an
  operation.  Actual experience has shown it to be quite good at picking
  up bugs in this area.
*/

#define _POSIX_SOURCE

#if defined(IRIX)
#include "condor_common.h"
#else
/*#include "condor_common.h"*/
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#endif

int		Data[ 4096 ];

#define MATCH  0

void init_data( int *data, unsigned len );
void do_it( int data[], int fd, unsigned len );

int
main( int argc, char *argv[] )
{
	int		fd;
	int		i;
	int		count;

	if( argc != 2 ) {
		fprintf( stderr, "Usage %s seconds\n", argv[0] );
		exit( 1 );
	}

	count = atoi( argv[1] );
	if( count <= 0 ) {
		fprintf( stderr, "Seconds must be positive\n" );
		exit( 1 );
	}

	if( (fd=open("tmp",O_CREAT|O_TRUNC|O_RDWR,0664)) < 0 ) {
		perror( "tmp" );
		exit( 1 );
	}

	init_data( Data, sizeof(Data) );

	for( i=0; i < count; i++ ) {
		do_it( Data, fd, sizeof(Data) );
		printf( "%d ", i );
		fflush( stdout );
	}
	printf( "\nNormal End Of Job\n" );
	exit( 0 );
}

void
init_data( int data[], unsigned len )
{
	int		i, lim;

	lim = len / sizeof(int);
	for( i=0; i<lim; i++ ) {
		data[i] = i;
	}
}

void
do_it( int data[], int fd, unsigned len )
{
	char	*buf = (char*)malloc(len), *ptr;
	int		i, bytes_read;
	unsigned count;

	for( i=0; i<100; i++ ) {
		if( lseek(fd,0,0) < 0 ) {
			perror( "lseek" );
			exit( 1 );
		}
		if( write(fd,data,len) < len ) {
			perror( "write" );
			exit( 1 );
		}
		if( lseek(fd,0,0) < 0 ) {
			perror( "lseek" );
			exit( 1 );
		}
		ptr = buf;
		count = 0;
		while ( (bytes_read = read(fd, ptr, len-count)) > 0 ) {
			ptr += bytes_read;
			count += bytes_read;
		}
		if (count < len) {
			perror(  "read" );
			exit( 1 );
		}
		if( memcmp(buf,data,len) != MATCH ) {
			printf( "Data Mismatch\n" );
			exit( 1 );
		}
	}
	free( buf );
}
