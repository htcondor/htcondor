/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "debug.h"
#include "condor_getmnt.h"

static init();
static char * remote_part( char *mnt_pt, char *name );
static char * xlate_link(  char *name );
static char	* compress( char *path );

static struct fs_data	FS_Buf[ NMOUNT ];
static int				N_Sys;
static char				Hostname[512];
static int				InitDone;

/*
** Translate a name which may cross a mount point.
*/
external_name( name, buf, bufsize )
const char	*name;
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
		(void)getcwd(pathname,_POSIX_PATH_MAX);
	} else if( name[0] == '/' ) {
		(void)strcpy( pathname, name );
	} else {
		(void)getcwd(pathname,_POSIX_PATH_MAX);
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
		dprintf( D_ALWAYS, "stat failed in external_name: %s\n",
				 strerror(errno) );
		return -1;
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
			if( (char *)strchr((const char *)fs->devname,':') ) {	/* it's a mount point */
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
		dprintf( D_ALWAYS, "gethostname failed in extern_path.c: %s\n",
				 strerror(errno) );
		return;
	}

	if( (N_Sys=getmnt(&start,FS_Buf,sizeof(FS_Buf),NOSTAT_MANY,0)) < 0 ) {
		dprintf( D_ALWAYS, "getmnt failed in extern_path.c: %s\n",
				 strerror(errno));
		return;
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
				dprintf(D_ALWAYS, "lstat failed in xlate_link: %s\n",
						strerror(errno));
			}
			return NULL;
		}
		if( (st_buf.st_mode & S_IFMT) == S_IFLNK ) {
			done = FALSE;
			if( (count=readlink(path,maps_to,sizeof(maps_to))) < 0 ) {
				dprintf(D_ALWAYS, "readlink failed in xlate_link: %s\n",
						strerror(errno));
				return NULL;
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
