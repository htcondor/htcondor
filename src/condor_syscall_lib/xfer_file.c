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
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include "condor_sys.h"
#include "ckpt_file.h"
#include "debug.h"

double	get_time();
extern int	Syscalls;

#if defined(__STDC__)
int try_via_afs( char *, int, int );
#else
int try_via_afs();
#endif

/*
** Transfer a local file to a remote machine.
*/
send_file( local, remote, perm )
char	*local;
char	*remote;
int		perm;
{

	int		scm;
	int		remote_fd, local_fd;
	int		answer;
	char	buf[4096];
	int		nbytes;
	int		bytes_written;
	int		count = 0;
	double	start, finish;
	int		mode = O_WRONLY | O_CREAT | O_TRUNC;

	dprintf( D_ALWAYS, "Entering send_file( %s, %s, 0%o )\n",local,remote,perm);

	if( (remote_fd = try_via_afs(remote,mode,perm)) >= 0 ) {
		dprintf( D_ALWAYS, "Transferring file via AFS\n" );
	} else if( (remote_fd = try_via_nfs(remote,mode,perm)) >= 0 ) {
		dprintf( D_ALWAYS, "Transferring file via NFS\n" );
	} else {
		dprintf( D_ALWAYS, "Transferring file via CONDOR_send_file\n" );
		answer =  REMOTE_syscall( CONDOR_send_file, local, remote, perm );
		return answer;
	}

	start = get_time();


	if( (local_fd=open(local,O_RDONLY,0)) < 0 ) {
		dprintf( D_FULLDEBUG, "Failed to open \"%s\" locally, errno = %d\n",
														local, errno);
		return -1;
	}

	/*
	display_syscall_mode( __LINE__, __FILE__ );
	dprintf( D_ALWAYS, "Local file \"%s\": fd = %d\n", local, local_fd );
	dprintf( D_ALWAYS, "Remote file \"%s\": fd = %d\n", remote, remote_fd );
	DumpOpenFds();
	*/

	for(;;) {
		
		/* dprintf( D_ALWAYS, "Attempting to read from fd %d\n", local_fd ); */
		nbytes = read( local_fd, buf, sizeof(buf) );
		/* dprintf( D_ALWAYS, "nbytes = %d\n", nbytes ) */;

		if( nbytes <= 0 ) {
			break;
		}
		if( (bytes_written = write(remote_fd,buf,nbytes)) == nbytes ) {
			count += nbytes;
			/* dprintf( D_ALWAYS, "Wrote %d bytes to fd %d\n",
													nbytes, remote_fd ); */
		} else {
			dprintf( D_ALWAYS, "Can't write fd %d, errno = %d\n",
														remote_fd, errno );
			return -1;
		}
	}

	(void)close( local_fd );
	(void)close( remote_fd );
	finish = get_time();

	if( nbytes < 0 ) {
		dprintf( D_ALWAYS, "Can't read fd %d, errno = %d\n", local_fd, errno );
	}
	if( nbytes == 0 ) {
		dprintf( D_ALWAYS,"Send_file() transferred %d bytes, %d bytes/second\n",
										count, (int)(count/(finish-start)) );
	}

	return nbytes;
}

/*
** Transfer a remote file to the local machine.
*/
get_file( remote, local, mode )
char	*remote;
char	*local;
int		mode;
{
	int		scm;
	int		remote_fd, local_fd;
	int		answer;
	char	buf[4096];
	int		nbytes;
	int		count = 0;
	double	start, finish;
	char	external[ MAXPATHLEN ];

	dprintf( D_ALWAYS, "Entering get_file( %s, %s, 0%o )\n", remote,local,mode);

	if( (remote_fd = try_via_afs(remote,O_RDONLY,0)) >= 0 ) {
		dprintf( D_ALWAYS, "Transferring file via AFS\n" );
	} else if( (remote_fd = try_via_nfs(remote,O_RDONLY,0)) >= 0 ) {
		dprintf( D_ALWAYS, "Transferring file via NFS\n" );
	} else {
		dprintf( D_ALWAYS, "Transferring file via CONDOR_get_file\n" );
		answer =  REMOTE_syscall( CONDOR_get_file, remote, local, mode );
		return answer;
	}


		/* Use NFS or AFS to transfer the file */
	start = get_time();

	if( (local_fd=open(local,O_WRONLY|O_CREAT|O_TRUNC,mode)) < 0 ) {
		dprintf( D_FULLDEBUG, "Failed to open \"%s\" locally, errno = %d\n",
														local, errno);
		return -1;
	}

	for(;;) {

		/* dprintf( D_ALWAYS, "Attempting to read from fd %d\n", remote_fd ); */
		nbytes = read( remote_fd, buf,sizeof(buf) );
		/* dprintf( D_ALWAYS, "nbytes = %d\n", nbytes ); */

		if( nbytes <= 0 ) {
			break;
		}
		if( write(local_fd,buf,nbytes) == nbytes ) {
			count += nbytes;
		} else {
			dprintf( D_ALWAYS, "Can't write fd %d, errno = %d\n",
														local_fd, errno );
			return -1;
		}
	}

	(void)close( local_fd );
	(void)close( remote_fd );

	finish = get_time();

	if( nbytes < 0 ) {
		dprintf( D_ALWAYS, "Can't read fd %d, errno = %d\n", remote_fd, errno );
	}
	if( nbytes == 0 ) {
		dprintf( D_ALWAYS,"Get_file() transferred %d bytes, %d bytes/second\n",
										count, (int)(count/(finish-start)) );
	}

	return nbytes;
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

/*
** Here we try to open the file via NFS, but avoid an obvious cases where
** NFS would be a loss.  If the file is physically located on the submitting
** machine, block transferring the file via the shadow will be better than
** NFS.  This will be even more true if we are writing the file. 
**
** On the other hand, if the file is on the submitting machine, and we're
** that machine, then what appears to be NFS access would really be local.
** In that case we're better off than using the shadow, so do it locally,
** which is still called "NFS".
*/
try_via_nfs( path, flags, mode )
char	*path;
int		flags;
int		mode;
{
#if 1
	return -1;
#else
	int		scm;
	char	external[ MAXPATHLEN ];
	static char	submitting_host[ MAXPATHLEN ];
	static char	this_host[ MAXPATHLEN ];
	int		submitting_host_len;
	int		answer = 0;
	char	*file_host, *find_physical_host();

	dprintf( D_FULLDEBUG, "Entering try_via_nfs\n" );


	if( submitting_host[0] == '\0' ) {

		REMOTE_syscall( CONDOR_gethostname, submitting_host,
													sizeof(submitting_host) );
		dprintf( D_FULLDEBUG, "submitting_host = \"%s\"\n", submitting_host );

		gethostname( this_host, sizeof(this_host) );
		dprintf( D_FULLDEBUG, "this_host = \"%s\"\n", this_host );

		submitting_host_len = strlen( submitting_host );
	}

	if( (file_host = find_physical_host(path,flags)) == NULL ) {
		dprintf( D_ALWAYS, "Can't find physical location of file \"%s\"\n",
																		path );
		return -1;
	}
	dprintf( D_FULLDEBUG, "at point 2\n" );
	dprintf( D_FULLDEBUG, "this_host = \"%s\"\n", this_host );
	dprintf( D_FULLDEBUG, "submitting_host = \"%s\"\n", submitting_host );
	dprintf( D_FULLDEBUG, "file_host = \"%s\"\n", file_host );

	if( strcmp(submitting_host,file_host) == 0 ) {
		dprintf(D_FULLDEBUG,"File is physically located on submitting host\n" );
		if( strcmp(submitting_host,this_host) == 0 ) {
			dprintf( D_FULLDEBUG, "But we are the submitting host\n" );
			answer = unmapped_nfs_open( path, flags, mode );
			dprintf( D_FULLDEBUG, "answer=%d\n", answer );
		} else {
			answer = -1;	/* Avoid NFS */
		}
	} else {
		dprintf( D_FULLDEBUG,
			 "File is NOT physically located on submitting host\n" );
		answer = unmapped_nfs_open( path, flags, mode );
		dprintf( D_FULLDEBUG, "answer=%d\n", answer );
	}

	return answer;
#endif
}

char *
find_physical_host( path, flags )
char	*path;
int		flags;
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

#if 0	/* This never got working, but we should do it some day... */
#if defined(SUNOS40) || defined(SUNOS41) || defined(CMOS)

/*
** Transfer a local core file to a remote machine.
*/
send_core( local, remote, mode )
char	*local;
char	*remote;
int		mode;
{
	int		scm;
	int		remote_fd, local_fd;
	int		answer;
	char	buf[4096];
	int		nbytes;
	int		count = 0;
	double	start, finish;

	struct core core;
	struct stat status;
	char	buf2[PAGSIZ];
	int		realStackSize, stackOffset;
	int		read_err = 0;		/* error found if any */
	int		write_err = 0;		/* error found if any */
	int		bytes_to_go;
	int		len;
	int		ck = 0;


	dprintf( D_ALWAYS, "Entering send_core( %s, %s, 0%o )\n",local,remote,mode);

	remote_fd = try_via_nfs( remote, O_WRONLY|O_CREAT|O_TRUNC, mode );
	if( remote_fd < 0 ) {
		dprintf( D_ALWAYS, "Transferring file via CONDOR_send_core\n" );
		answer =  REMOTE_syscall( CONDOR_send_core, local, remote, mode );
		return answer;
	}


	dprintf( D_ALWAYS, "Transferring file via NFS\n" );
	scm = SetSyscalls( SYS_LOCAL | SYS_RECORDED );

	start = get_time();


	if( (local_fd=open(local,O_RDONLY,0)) < 0 ) {
		dprintf( D_FULLDEBUG, "Failed to open \"%s\" locally, errno = %d\n",
														local, errno);
		(void)SetSyscalls( scm );
		return -1;
	}

	ck = read(local_fd, (char *) &core, sizeof(core)) != sizeof(core);
	ck = (write(remote_fd, (char *) &core, sizeof(core)) != sizeof(core)) || ck;
	if (ck)
		dprintf(D_ALWAYS, "Error copy core\n");
	else
		dprintf(D_ALWAYS, "Done copy core\n");

	for (bytes_to_go = core.c_dsize; !read_err && !write_err && bytes_to_go;
			bytes_to_go -= len) {
		len = bytes_to_go < sizeof(buf) ? bytes_to_go : sizeof(buf);
		if (read(local_fd, buf, len) != len) {
			read_err = 1;
			break;
		}
		if (write(remote_fd, buf, len) != len) {
			write_err = 1;
			break;
		}
	}
	if (!read_err && !write_err)
		dprintf(D_ALWAYS, "Done copy data area\n");
	else
		dprintf(D_ALWAYS, "Error copy data area\n");


	/*
	 *  Find the stackOffset and realStackSize
	 */
	lstat(local, &status);
	realStackSize = status.st_blocks * DEV_BSIZE -
			(status.st_size - core.c_ssize);
	
	/* lseek to the estimate bottom of real stack area */
	lseek(local_fd, -2 * PAGSIZ - realStackSize, L_XTND);

	/* read upward and shrink the real stack size to get a closer value */
	for ( ; realStackSize > 0; realStackSize -= PAGSIZ) {
		if (read(local_fd, buf2, sizeof(buf2)) != sizeof(buf2) ) {
			EXCEPT("read(local_fd %d, buf2 0x%x, %d)",
					local_fd, buf2, sizeof(buf2));
		}
		if (!is_zero(buf2, sizeof(buf2)))
			break;
	}
	stackOffset = core.c_ssize - realStackSize;

	/* local_fd is already at the right place after above operation */
    lseek(remote_fd, stackOffset, L_INCR);

	/* now do the rest of the copying */
	for (bytes_to_go = realStackSize; !read_err && !write_err && bytes_to_go;
			bytes_to_go -= len) {
		len = bytes_to_go < sizeof(buf) ? bytes_to_go : sizeof(buf);
		if (read(local_fd, buf, len) != len) {
			read_err = 1;
			break;
		}
		if (write(remote_fd, buf, len) != len) {
			write_err = 1;
			break;
		}
	}
	dprintf(D_ALWAYS, "-Done copy stack area\n");
	dprintf(D_ALWAYS, "status.st_size = %d, lseek = %d\n", status.st_size,
			lseek(local_fd, 0, L_XTND));
	lseek(remote_fd, status.st_size, L_SET);


	(void)close( local_fd );
	(void)close( remote_fd );
	finish = get_time();

	if( read_err ) {
		dprintf(D_ALWAYS, "Can't read fd %d, errno = %d\n", local_fd, errno );
	}
	if( write_err ) {
		dprintf(D_ALWAYS, "Can't write fd %d, errno = %d\n", remote_fd, errno );
	}
	if( !read_err && !write_err ) {
		dprintf(D_ALWAYS,"Send_file() transferred %d bytes, %f bytes/second\n",
										count, count / (finish - start) );
	}

	(void)SetSyscalls( scm );
	if (read_err || write_err)
		return -1;
	return 0;
}
#endif defined(SUNOS40) || defined(SUNOS41) || defined(CMOS)
#endif


#define MATCH	0 /* for strcmp */

int
try_via_afs( remote, mode, perm )
char *remote;
int mode;
int perm;
{
	int		fd;

	if( strncmp("/afs",remote,4) != MATCH ) {
		return -1;
	}

	if( (fd = open(remote,mode,perm)) < 0 ) {
		dprintf( D_ALWAYS, "\"%s\" is an AFS file, but open failed\n", remote );
		return -1;
	}

	dprintf( D_ALWAYS, "Opened \"%s\" via AFS\n", remote );
	return fd;
}
