/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include "condor_sys.h"
#include "ckpt_file.h"
#include "debug.h"

double	get_time();
extern int	Syscalls;

#define CHUNK_SIZE 4096

/*
** Transfer a local file to a remote machine.
*/
int
send_file( const char *local, const char *remote, int perm )
{

	int		remote_fd, local_fd;
	char	buf[ CHUNK_SIZE ];
	int		nbytes, bytes_to_go;
	size_t	len;
	double	start, finish;

	dprintf( D_ALWAYS, "Entering send_file( %s, %s, 0%o )\n",local,remote,perm);

		/* Start timing the transfer */
	start = get_time();

		/* Open the local file */
	if( (local_fd=open(local,O_RDONLY,0)) < 0 ) {
		dprintf( D_FULLDEBUG, "Failed to open \"%s\" locally, errno = %d\n",
														local, errno);
		return -1;
	}

		/* Find length of the file */
	len = lseek( local_fd, 0, 2 );
	lseek( local_fd, 0, 0 );

		/* Open the remote file */
	remote_fd = open_file_stream( remote, O_WRONLY | O_CREAT | O_TRUNC, &len );
	if( remote_fd < 0 ) {
		dprintf( D_ALWAYS, "Failed to open \"%s\" remotely, errno = %d\n",
															remote, errno );
	}

		/* transfer the data */
	dprintf( D_ALWAYS, "File length is %d\n", len );
	for(bytes_to_go = len; bytes_to_go; bytes_to_go -= nbytes ) {
		nbytes = MIN( CHUNK_SIZE, bytes_to_go );
		nbytes = read( local_fd, buf, nbytes );
		if( nbytes < 0 ) {
			dprintf( D_ALWAYS, "Can't read fd %d, errno = %d\n",
														local_fd, errno );
		}
		if( write(remote_fd,buf,nbytes) != nbytes ) {
			dprintf( D_ALWAYS, "Can't write fd %d, errno = %d\n",
														remote_fd, errno );
			return -1;
		}
	}

		/* Clean up */
	(void)close( local_fd );
	(void)close( remote_fd );

		/* report */
	finish = get_time();
	dprintf( D_ALWAYS,"Send_file() transferred %d bytes, %d bytes/second\n",
										len, (int)(len/(finish-start)) );

	return len;
}

/*
** Transfer a remote file to the local machine.
*/
int
get_file( const char *remote, const char *local, int mode )
{
	int		remote_fd, local_fd;
	char	buf[ CHUNK_SIZE ];
	int		nbytes, bytes_to_go;
	size_t	len;
	double	start, finish;

	dprintf( D_ALWAYS, "Entering get_file( %s, %s, 0%o )\n", remote,local,mode);

	start = get_time();

		/* open the remote file */
	remote_fd = open_file_stream( remote, O_RDONLY, &len );
	if( remote_fd < 0 ) {
		dprintf( D_ALWAYS, "open_file_stream(%s,O_RDONLY,0x%x) failed\n",
														remote_fd, &len );
	}

		/* open the local file */
	if( (local_fd=open(local,O_WRONLY|O_CREAT|O_TRUNC,mode)) < 0 ) {
		dprintf( D_FULLDEBUG, "Failed to open \"%s\" locally, errno = %d\n",
														local, errno);
		return -1;
	}

		/* transfer the data */
	for(bytes_to_go = len; bytes_to_go; bytes_to_go -= nbytes ) {
		nbytes = MIN( CHUNK_SIZE, bytes_to_go );
		nbytes = read( remote_fd, buf, nbytes );
		if( nbytes < 0 ) {
			dprintf( D_ALWAYS, "Can't read fd %d, errno = %d\n",
														remote_fd, errno );
		}
		if( write(local_fd,buf,nbytes) != nbytes ) {
			dprintf( D_ALWAYS, "Can't write fd %d, errno = %d\n",
														local_fd, errno );
			return -1;
		}
	}

		/* clean up */
	(void)close( local_fd );
	(void)close( remote_fd );

	finish = get_time();

	dprintf( D_ALWAYS,"Get_file() transferred %d bytes, %d bytes/second\n",
										len, (int)(len/(finish-start)) );

	return len;
}

#include <sys/time.h>
double
get_time()
{
	struct timeval	tv;
	int				scm;

	if( gettimeofday( &tv, 0 ) < 0 ) {
		perror( "gettimeofday()" );
		exit( 1 );
	}

	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}


#if 0
char *
find_physical_host( const char *path, int flags )
{
	static char answer[ MAXPATHLEN ];
	char		dir[ MAXPATHLEN ];
	char		*ptr, *index(), *rindex();

		/* Try to find the pathname as given */
	/* if( extern_name(path,answer,sizeof(answer)) >= 0 ) { */
	if( REMOTE_syscall(CONDOR_extern_name,path,answer,sizeof(answer)) >= 0 ) {
		if( ptr=index(answer,':') ) {
			*ptr = '\0';
		}
		return answer;
	}

	if( !(flags & O_CREAT) ) {
		return NULL;
	}

		/* He's trying to creat the file, look for the parent directory */
	strcpy( dir, path );
    if( ptr=rindex(dir,'/') ) {
		if( ptr == dir ) {
			strcpy( dir, "/" );
		} else {
			*ptr = '\0';
		}
	} else {
		REMOTE_syscall( CONDOR_getwd, dir );
	}

	/* if( extern_name(dir,answer,sizeof(answer)) >= 0 ) { */
	if( REMOTE_syscall(CONDOR_extern_name,dir,answer,sizeof(answer)) >= 0 ) {
		if( ptr=index(answer,':') ) {
			*ptr = '\0';
		}
		return answer;
	} else {
		return NULL;
	}
}
#endif
