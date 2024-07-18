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


/*
  This program is intended to test the response of programs to a checkpoint
  signal while in the middle of a remote I/O operation.  It does intense
  reads and writes, so that any time it get an asynchronous checkpoint
  request, chances are high that it will be in the middle of such an
  operation.  Actual experience has shown it to be quite good at picking
  up bugs in this area.
*/

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

int		Data[ 4096 ];

#define MATCH  0

void init_data( int *data, unsigned len );
void do_it( int data[], int fd, unsigned len );

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
	printf( "Be sure the job checkpointed at least once.\n" );
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
	char	*buf = malloc(len);
	int		i;

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
		if( read(fd,buf,len) != len ) {
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
