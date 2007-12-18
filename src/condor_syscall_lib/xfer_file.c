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
#include "condor_sys.h"
#include "condor_debug.h"

#include "condor_macros.h"
#include "condor_open.h"
#include "util_lib_proto.h"

double	get_time();
extern int	Syscalls;

extern int open_file_stream( const char *file, int flags, size_t *len );

/* remote systems calls we use in this file */
extern int REMOTE_CONDOR_extern_name(char *path, char *buf, int bufsize);
extern int REMOTE_CONDOR_getwd(char *path_name);


#define CHUNK_SIZE 4096


/*
** Transfer a local file to a remote machine.
*/
int
send_a_file( const char *local, const char *remote, int perm )
{

	int		remote_fd, local_fd;
	char	buf[ CHUNK_SIZE + 50 ];
	int		nbytes, bytes_to_go;
	size_t	len;
	double	start, finish;

	dprintf( D_ALWAYS, "Entering send_a_file( %s, %s, 0%o )\n",
		local,remote,perm);

		/* Start timing the transfer */
	start = get_time();

		/* Open the local file */
	if( (local_fd=safe_open_wrapper(local,O_RDONLY,0)) < 0 ) {
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
		return -1;
	}

		/* transfer the data */
	dprintf( D_ALWAYS, "File length is %lu\n", (unsigned long)len );
	for(bytes_to_go = len; bytes_to_go; bytes_to_go -= nbytes ) {
		nbytes = MIN( CHUNK_SIZE, bytes_to_go );
		nbytes = read( local_fd, buf, nbytes );
		if( nbytes < 0 ) {
			dprintf( D_ALWAYS, "Can't read fd %d, errno = %d\n",
														local_fd, errno );
			(void)close( local_fd );
			(void)close( remote_fd );
			return -1;
		}
		if( write(remote_fd,buf,nbytes) != nbytes ) {
			dprintf( D_ALWAYS, "Can't write fd %d, errno = %d\n",
														remote_fd, errno );
			(void)close( local_fd );
			(void)close( remote_fd );
			return -1;
		}
	}

		/* Clean up */
	(void)close( local_fd );
	(void)close( remote_fd );

		/* report */
	finish = get_time();
	dprintf( D_ALWAYS,"Send_file() transferred %lu bytes, %d bytes/second\n",
										(unsigned long)len, 
										(int)(len/(finish-start)) );

	return len;
}

/*
** Transfer a remote file to the local machine.
*/
int
get_file( const char *remote, const char *local, int mode )
{
	int		remote_fd, local_fd;
	char	buf[ CHUNK_SIZE + 50];
	int		nbytes, bytes_to_go;
	size_t	len;
	double	start, finish;

	dprintf( D_ALWAYS, "Entering get_file( %s, %s, 0%o )\n", remote,local,mode);

	start = get_time();

		/* open the remote file */
	remote_fd = open_file_stream( remote, O_RDONLY, &len );
	if( remote_fd < 0 ) {
		dprintf( D_ALWAYS, "Failed to open \"%s\" remotely, errno = %d\n",
				 remote, errno );
		return -1;
	}

		/* open the local file */
	if( (local_fd=safe_open_wrapper(local,O_WRONLY|O_CREAT|O_TRUNC,mode)) < 0 ) 
	{
		dprintf( D_FULLDEBUG, "Failed to open \"%s\" locally, errno = %d\n",
														local, errno);
		return -1;
	}

		/* transfer the data */
	for(bytes_to_go = len; bytes_to_go; bytes_to_go -= nbytes ) {
		nbytes = MIN( CHUNK_SIZE, bytes_to_go );
		nbytes = read( remote_fd, buf, nbytes );
		if( nbytes <= 0 ) {
			dprintf( D_ALWAYS, "Can't read fd %d, errno = %d\n",
														remote_fd, errno );
			(void)close( local_fd );
			(void)close( remote_fd );
			return -1;
		}
		if( write(local_fd,buf,nbytes) != nbytes ) {
			dprintf( D_ALWAYS, "Can't write fd %d, errno = %d\n",
														local_fd, errno );
			(void)close( local_fd );
			(void)close( remote_fd );
			return -1;
		}
	}

		/* clean up */
	(void)close( local_fd );
	(void)close( remote_fd );

	finish = get_time();

	dprintf( D_ALWAYS,"Get_file() transferred %lu bytes, %d bytes/second\n",
										(unsigned long) len, 
										(int)(len/(finish-start)) );

	return len;
}

#include <sys/time.h>
double
get_time()
{
	struct timeval	tv;

	if( gettimeofday( &tv, 0 ) < 0 ) {
		dprintf( D_ALWAYS, "gettimeofday failed in get_time(): %s\n",
				 strerror(errno) );
		return 0.0;
	}

	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}


#if 0
char *
find_physical_host( const char *path, int flags )
{
	static char answer[ MAXPATHLEN ];
	char		dir[ MAXPATHLEN ];
	char		*ptr, *strchr(), *strrchr(); /* 9/25 ..dhaval */

		/* Try to find the pathname as given */
	/* if( extern_name(path,answer,sizeof(answer)) >= 0 ) { */
	if( REMOTE_CONDOR_extern_name(path,answer,sizeof(answer)) >= 0 ) {
		if( ptr=strchr(answer,':') ) { /* dhaval 9/25 */
			*ptr = '\0';
		}
		return answer;
	}

	if( !(flags & O_CREAT) ) {
		return NULL;
	}

		/* He's trying to creat the file, look for the parent directory */
	strcpy( dir, path );
    if( ptr=strrchr(dir,'/') ) { /* dhaval 9/25 */
		if( ptr == dir ) {
			strcpy( dir, "/" );
		} else {
			*ptr = '\0';
		}
	} else {
		REMOTE_CONDOR_getwd( dir );
	}

	/* if( extern_name(dir,answer,sizeof(answer)) >= 0 ) { */
	if( REMOTE_CONDOR_extern_name(dir,answer,sizeof(answer)) >= 0 ) {
		if( ptr=strchr(answer,':') ) { /* dhaval 9/25 */
			*ptr = '\0';
		}
		return answer;
	} else {
		return NULL;
	}
}
#endif
