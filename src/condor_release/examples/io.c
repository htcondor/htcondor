/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

/*
  This program is intended to test the response of programs to a checkpoint
  signal while in the middle of a remote I/O operation.  It does intense
  reads and writes, so that any time it get an asynchronous checkpoint
  request, chances are high that it will be in the middle of such an
  operation.  Actual experience has shown it to be quite good at picking
  up bugs in this area.
*/

#define _POSIX_SOURCE

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
