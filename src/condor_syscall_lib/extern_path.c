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
/*
** Translate local file names which may contain symbolic links and cross
** mount points into external names of the form hostname:pathname.  
*/

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include "debug.h"
#include "condor_getmnt.h"

#define TRUE 	1
#define FALSE	0

static init();
static char * remote_part( char *mnt_pt, char *name );
static char * xlate_link(  char *name );
static char	* compress( char *path );

extern int		errno;

char	*index();
char	*getwd(), *malloc(), *strcat(), *strcpy();
#if !defined(AIX32) && !defined(OSF1) &&!defined(HPUX9)
char	*sprintf();
#endif

static struct fs_data	FS_Buf[ NMOUNT ];
static int				N_Sys;
static char				Hostname[512];
static int				InitDone;




/*
** Translate a name which may cross a mount point.
*/
external_name( name, buf, bufsize )
char	*name;
char	*buf;
int		bufsize;
{
	int					i;
	char				*local_name, *remote_name, *mount_pt;
	struct stat			st_buf;
	char				my_buf[MAXPATHLEN+1];
	char				pathname[MAXPATHLEN+1];
	struct fs_data_req	*fs;
	dev_t				file_dev;

	dprintf( D_NFS, "Entering external_name( %s )\n", name );

	if( !InitDone ) {
		dprintf( D_NFS, "external_name: initializing mount table\n" );
		init();
	}

	if( !name[0] ) {
		(void)getwd(pathname);
	} else if( name[0] == '/' ) {
		(void)strcpy( pathname, name );
	} else {
		(void)getwd(pathname);
		(void)strcat( pathname, "/" );
		(void)strcat( pathname, name );
	}

	dprintf( D_NFS, "external_name: pathname = \"%s\"\n", pathname );
		

		/* Get rid of symbolic links */
	if( (local_name=xlate_link(pathname)) == NULL ) {
		return -1;
	}

	dprintf( D_NFS, "external_name: local_name = \"%s\"\n", local_name );

		/* Find out what "device" file is on */
	if( stat(local_name,&st_buf) < 0 ) {
		perror( "stat" );
		exit( 1 );
	}
	file_dev = st_buf.st_dev;

		/* Search for the device in the mount table */
	for( i=0; i<N_Sys; i++ ) {
		fs = &FS_Buf[i].fd_req;
		if( file_dev == fs->dev ) {	/* found file system */
			dprintf( D_NFS,
				"external_name: fs->devname = \"%s\"\n", fs->devname
			);
			dprintf( D_NFS, "external_name: fs->path = \"%s\"\n", fs->path );
			if( index(fs->devname,':') ) {	/* it's a mount point */
				if( (mount_pt=xlate_link(fs->path)) == NULL ) {
					return -1;
				}
				remote_name = remote_part( mount_pt, local_name );
				dprintf( D_NFS,
					"external_name: remote_name = \"%s\"\n", remote_name
				);
				if( remote_name ) {
					(void)sprintf( my_buf, "%s%s", fs->devname, remote_name );
				} else {
					(void)sprintf( my_buf, "%s", fs->devname );
				}
			} else {					/* local device */
				(void)sprintf( my_buf, "%s:%s", Hostname, local_name );
			}
			strncpy( buf, my_buf, bufsize );
			dprintf( D_NFS, "external_name returning 0, answer \"%s\"\n", buf );
			return 0;
		}
	}
	dprintf( D_NFS, "external_name returning -1\n" );
	return -1;
}

static
init()
{
	int		start = 0;

	if( gethostname(Hostname,sizeof(Hostname)) < 0 ) {
		perror( "gethostname" );
		exit( 1 );
	}

	if( (N_Sys=getmnt(&start,FS_Buf,sizeof(FS_Buf),NOSTAT_MANY,0)) < 0 ) {
		perror( "getmnt" );
		exit( 1 );
	}

	InitDone = TRUE;

}

/*
** Given a mount point and some pathname which crosses that mount point,
** separate out and return the part of the name which is on the other
** side of the mount point.  Note: if we are asked about the mount point
** itself, the part returned will be NULL.
*/
static char *
remote_part( mnt_pt, name )
char	*mnt_pt;
char	*name;
{
	char	*x, *y;

	dprintf( D_NFS,
		"Entering remote_part( mnt_pt = \"%s\", name = \"%s\" )\n",
		mnt_pt, name 
	);

	for( x=mnt_pt, y=name; *x && *x == *y; x++, y++ )
		;

	dprintf( D_NFS, "remote_part: returning \"%s\"\n", y );

	return y;
}


/*
** Translate a name which may contain symbolic links into a pathame
** containing no symbolic links.
*/
static char *
xlate_link(  name )
char	*name;
{
	struct stat		st_buf;
	char			*ptr, *end, *dst, *cur;
	char			save;
	int				count;
	char			path[MAXPATHLEN+1];
	char			maps_to[MAXPATHLEN+1];
	char			*up;
	int				done = TRUE;

	up = MALLOC( MAXPATHLEN + 1 );
	up[0] = '/';
	up[1] = '\0';
	cur = name + 1;

	for(;;) {
		for( dst=path, end=up; *end; *dst++ = *end++ )
			;

		for( ptr=cur; *ptr && *ptr != '/'; *dst++ = *ptr++ )
			;
		*dst = '\0';

		
		if( lstat(path,&st_buf) < 0 ) {
			if( errno == EFAULT ) {
				perror( "stat" );
				exit( 1 );
			} else {
				return NULL;
			}
		}
		if( (st_buf.st_mode & S_IFMT) == S_IFLNK ) {
			done = FALSE;
			if( (count=readlink(path,maps_to,sizeof(maps_to))) < 0 ) {
				perror( "readlink" );
				exit( 1 );
			}
			maps_to[count] = '\0';
			if( maps_to[0] == '/' ) {
				(void)strcpy( up, maps_to );
			} else {
				(void)strcat( up, maps_to );
			}
		} else {
			save = *ptr;
			*ptr = '\0';
			(void)strcat( up, cur );
			*ptr = save;
		}
		if( *ptr == '/' ) {
			(void)strcat( up, "/" );
			cur = ptr + 1;
		} else {
			if( done ) {
				return compress(up);
			} else {
				return xlate_link( compress(up) );
			}
		}
	}
}

/*
** Given a pathname, compress out occurances of "/.." and return a
** pointer to the result.  N.B. this operation is done in place,
** the original string is lost.
*/
static char	*
compress( path )
char	*path;
{
	char	*src, *dst;

	for( src=path; *src; src++ ) {
		if( src[0] == '/' && src[1] == '.' && src[2] == '.' &&
										(src[3] == '/' || src[3] == '\0') ) {
			for( dst=src - 1; *dst != '/'; dst-- )
				;
			src += 3;
			while( *src ) {
				*dst++ = *src++;
			}
			*dst = '\0';
			src = path;
		}
	}
	return path;
}
