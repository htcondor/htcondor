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
#include <sys/types.h>
#include <sys/file.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(IRIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "x_fake_ckpt.h"

#if defined(Solaris)
      #include <fcntl.h>
#endif

struct fd_rec {
	int		fd;
	off_t	eof;
};

struct fd_rec	Fd_Rec[256];
int		N_Fd_Recs;

void check_close( int fd );
void init_fd_rec( int fd );
void check_fd_recs();


int
main( int argc, char **argv )
{
	int		i, fd, dummy;

	check_close( 15 );
	check_close( 16 );
	check_close( 17 );
	check_close( 18 );
	check_close( 19 );
	check_close( 20 );

	dummy = open("/dev/zero", O_RDONLY, 0);

	for( i=1; i<argc; i++ ) {
		if( (fd=open(argv[i],O_RDONLY,0)) < 0 ) {
			fprintf( stderr, "Can't open \"%s\"\n", argv[i] );
			exit( 1 );
		}
		/*
		printf( "Opened \"%s\" as fd %d\n", argv[i], fd );
		*/
		init_fd_rec( fd );
	}
	printf( "Opened %d files\n", i-1 );
	fflush( stdout );

	printf( "About to checkpoint\n" );
	fflush( stdout );
	ckpt_and_exit();
	printf( "Returned from checkpoint\n" );
	fflush( stdout );
	check_fd_recs();
	close(dummy);

	printf( "About to checkpoint\n" );
	fflush( stdout );
	ckpt_and_exit();
	printf( "Returned from checkpoint\n" );
	fflush( stdout );
	check_fd_recs();

	printf( "Normal End of Job\n" );
	exit( 0 );
}

void init_fd_rec( int fd )
{
	off_t			end;
	struct fd_rec	*fd_rec;

	end = lseek( fd, 0, 2 );
	fd_rec = &Fd_Rec[ N_Fd_Recs ];
	fd_rec->fd = fd;
	fd_rec->eof = end;
	N_Fd_Recs += 1;
	/*
	printf( "Eof = %d\n", end );
	*/
}

void check_fd_recs()
{
	int				i;
	struct fd_rec	*fd_rec;
	off_t			cur, end;

	for( i=0; i<N_Fd_Recs; i++ ) {
		fd_rec = &Fd_Rec[ i ];
		cur = lseek( fd_rec->fd, 0, 1 );
		end = lseek( fd_rec->fd, 0, 2 );
		if( cur != end ) {
			printf( "ERROR on fd %d, cur = %d, end = %d\n", i,
					(int)cur, (int)end );
			exit( 1 );
		}
		if( end != fd_rec->eof ) {
			printf( "ERROR end = %d, previously was %d\n", (int)end,
					(int)fd_rec->eof );
			exit( 1 );
		}
	}
	printf( "OK\n" );
}

extern int	errno;
void check_close( int fd )
{
	int	status;
	status = close( fd );
	if( status >= 0 || errno != EBADF ) {
		printf( "Error: close(%d) = %d, errno = %d\n", fd, status, errno );
		exit( 1 );
	}
}
