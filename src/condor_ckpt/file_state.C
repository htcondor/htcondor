/*************************************************************
**
** Manage a table of open file descriptors.
**
** Includes stubs for those system calls which affect the table of
** open file descriptors such as open(), close(), dup(), dup2(), etc.
**
** Includes C interface routine MapFd() used by other system call
** stubs which don't affect the open file table.
**
** Also included is the C routine DumpOpenFds() for debugging.
**
*************************************************************/

#define _POSIX_SOURCE

#include <stdio.h>
#include "condor_fix_fcntl.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "condor_syscalls.h"
#include <stdarg.h>
#include <sys/types.h>
#include "file_state.h"
#include "condor_constants.h"
#include "file_table_interf.h"
#include <assert.h>

static const int FI_RSC =			0; 		/* access via remote sys calls   */
static const int FI_OPEN =			1<<0;	/* file is open             	 */
static const int FI_DUP =			1<<1;	/* file is a dup of 'fi_fdno'    */
static const int FI_PREOPEN =		1<<2;	/* file was opened previously    */
static const int FI_NFS =			1<<3;	/* access via direct sys calls   */
static const int FI_DIRECT =		1<<3;
static const int FI_WELL_KNOWN =	1<<4;	/* connection to shadow          */

static OpenFileTable	FileTab;
static char				Condor_CWD[ _POSIX_PATH_MAX ];

char * shorten( char *path );

extern int errno;

File::File()
{
	open = FALSE;
	pathname = 0;
}

File::~File()
{
	delete [] pathname;
	pathname = NULL;
}

OpenFileTable::OpenFileTable()
{
	SetSyscalls( SYS_UNMAPPED | SYS_LOCAL );
	getcwd( Condor_CWD, sizeof(Condor_CWD) );
	PreOpen( 0 );
	PreOpen( 1 );
	PreOpen( 2 );

	SetSyscalls( SYS_MAPPED | SYS_LOCAL );
}


void
OpenFileTable::Display()
{
	int		i;

	printf( "%4s %3s %3s %3s %3s %3s %3s %8s %6s %6s %s\n",
		"   ", "Dup", "Pre", "Rem", "Sha", "Rd", "Wr", "Offset", "FdNum", "DupOf", "Pathname" );
	for( i=0; i<_POSIX_OPEN_MAX; i++ ) {
		if( !file[i].isOpen() ) {
			continue;
		}
		printf( "%4d ", i );
		file[i].Display();
	}
}

void
File::Display()
{
	printf( "%3c ", isDup() ? 'T' : 'F' );
	printf( "%3c ", isPreOpened() ? 'T' : 'F' );
	printf( "%3c ", isRemoteAccess() ? 'T' : 'F' );
	printf( "%3c ", isShadowSock() ? 'T' : 'F' );
	printf( "%3c ", isReadable() ? 'T' : 'F' );
	printf( "%3c ", isWriteable() ? 'T' : 'F' );
	printf( "%8d ", offset );
	printf( "%6d ", fd_num );
	if( isDup() ) {
		printf( "%6d ", dup_of );
	} else {
		printf( "%6s ", "" );
	}
	if( strlen(pathname) > 25 ) {
		printf( "\"...%s\"\n", shorten(pathname) );
	} else {
		printf( "\"%s\"\n", pathname );
	}
}

int
OpenFileTable::DoOpen( const char *path, int flags, int mode )
{
	int	user_fd;
	int	real_fd;
	char	buf[ _POSIX_PATH_MAX ];

		// Try to open the file
	if( LocalSysCalls() ) {
		real_fd = syscall( SYS_open, path, flags, mode );
	} else {
		real_fd = REMOTE_syscall( SYS_open, path, flags, mode );
	}

		// Stop here if there was an error
	if( real_fd < 0 ) {
		return -1;
	}

		// find an unused fd
	if( (user_fd = find_avail(0)) < 0 ) {
		errno = EMFILE;
		return -1;
	}

		// set the file access mode
	switch( flags & O_ACCMODE ) {
	  case O_RDONLY:
		file[user_fd].readable = TRUE;
		file[user_fd].writeable = FALSE;
		break;
	  case O_WRONLY:
		file[user_fd].readable = FALSE;
		file[user_fd].writeable = TRUE;
		break;
	  case O_RDWR:
		file[user_fd].readable = TRUE;
		file[user_fd].writeable = TRUE;
		break;
	  default:
		fprintf( stderr, "Opened file in unknown mode\n" );
		abort();
	}

	file[user_fd].open = TRUE;
	file[user_fd].duplicate = FALSE;
	file[user_fd].pre_opened = FALSE;
	file[user_fd].remote_access = RemoteSysCalls();
	file[user_fd].shadow_sock = FALSE;
	file[user_fd].offset = 0;
	file[user_fd].fd_num = real_fd;
	if( path[0] == '/' ) {
		file[user_fd].pathname = string_copy( path );
	} else {
		sprintf( buf, "%s/%s", Condor_CWD, path );
		file[user_fd].pathname = string_copy( buf );
	}
	return user_fd;
}

int
OpenFileTable::DoClose( int fd )
{

	BOOL	was_duped;
	int		new_base_file;
	int		i;
	int		rval;

	if( !file[fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

		// See if this file is a dup of another
	if( file[fd].isDup() ) {
		file[fd].open = FALSE;
		return 0;
	}

		// Look for another file which is a dup of this one
	was_duped = FALSE;
	for( i=0; i<_POSIX_OPEN_MAX; i++ ) {
		if( file[i].isOpen() && file[i].isDup() && file[i].dup_of == fd ) {
			was_duped = TRUE;
			new_base_file = i;
			break;
		}
	}

		// simple case, no dups - just clean up
	if( !was_duped )  {
		if( file[fd].isRemoteAccess() ) {
			rval = REMOTE_syscall( SYS_close, file[fd].fd_num );
		} else {
			rval = syscall( SYS_close, file[fd].fd_num );
		}
		file[fd].open = FALSE;
		delete [] file[fd].pathname;
		file[fd].pathname = NULL;
		return rval;
	}

		// make this one the non-duplicate fd
	file[new_base_file].duplicate = FALSE;

		// make all other fd's which were dups of the one we're
		// closing point to the new non-duplicate fd
	for( i=0; i<_POSIX_OPEN_MAX; i++ ) {
		if( file[i].isOpen() && file[i].isDup() && file[i].dup_of == fd ) {
			file[i].dup_of = new_base_file;
		}
	}

		// Mark the original fd as closed.  Careful
		// don't really close it or delete the pathname -
		// those are in use by duplicates.
	file[fd].open = FALSE;

	return rval;
}


int
OpenFileTable::PreOpen( int fd )
{
		// Make sure fd not already open
	if( file[fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

		// Only deal with stdin, stdout, and stderr
	switch( fd ) {
	  case 0:
		file[fd].readable = TRUE;
		file[fd].writeable = FALSE;
		break;
	  case 1:
	  case 2:
		file[fd].readable = FALSE;
		file[fd].writeable = TRUE;
		break;
	  default:
		errno = EBADF;
		return -1;
	}

		// Record reasonable values for everything else
	file[fd].open = TRUE;
	file[fd].duplicate = FALSE;
	file[fd].pre_opened = TRUE;
	file[fd].remote_access = RemoteSysCalls();
	file[fd].shadow_sock = FALSE;
	file[fd].offset = 0;
	file[fd].fd_num = fd;
	file[fd].dup_of = 0;
	file[fd].pathname = string_copy( "(Pre-Opened)" );

	return fd;
}

int
OpenFileTable::find_avail( int start )
{
	int		i;

	for( i=start; i<_POSIX_PATH_MAX; i++ ) {
		if( !file[i].isOpen() ) {
			return i;
		}
	}
	return -1;
}

OpenFileTable::Map( int user_fd )
{
		// See if we are in "mapped" mode
	if( ! MappingFileDescriptors() ) {
		return user_fd;
	}

		// Trying to map a file that isn't open
	if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	return file[user_fd].fd_num;
}

int
OpenFileTable::DoDup2( int orig_fd, int new_fd )
{
		// error if orig_fd not open
	if( ! file[orig_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

		// error if new_fd out of range for file descriptors
	if( new_fd < 0 || new_fd >= _POSIX_OPEN_MAX ) {
		errno = EBADF;
		return -1;
	}

		// POSIX.1 says do it this way, AIX does it differently - any
		// AIX programs which depend on that behavior are now hosed...
	if( file[new_fd].isOpen() ) {
		DoClose( new_fd );
	}

		// make new fd a duplicate of the original one
	file[new_fd] = file[orig_fd];
	file[new_fd].duplicate = TRUE;
	file[new_fd].dup_of = orig_fd;

	return new_fd;
}

int
OpenFileTable::DoDup( int user_fd )
{
	int		new_fd;

	if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	if( (new_fd=find_avail(0)) < 0 ) {
		errno = EMFILE;
		return -1;
	}

	return DoDup2( user_fd, new_fd );
}

void
OpenFileTable::Save()
{
	int		i;
	off_t	pos;
	File	*f;

	for( i=0; i<_POSIX_OPEN_MAX; i++ ) {
		f = &file[i];
		if( f->isOpen() && !f->isDup() ) {
			if( f->isRemoteAccess() ) {
				pos = REMOTE_syscall( SYS_lseek, f->fd_num, (off_t)0, SEEK_CUR);
			} else {
				pos = syscall( SYS_lseek, f->fd_num, (off_t)0, SEEK_CUR);
			}
			if( pos < (off_t)0 && f->isPreOpened() && errno == ESPIPE ) {
				pos = (off_t)0;		// probably it's a tty
			}
			if( pos < (off_t)0 ) {
				perror( "lseek" );
				abort();
			}
			f->offset = pos;
		}
	}
}

/*
  The given user_fd has been re-opened after a checkpoint, and may have a
  differend fd_num now than it did at the time of the checkpoint.  Here
  we go through the open file table, and fix up the fd_num of any
  duplicates of this file.
*/
void
OpenFileTable::fix_dups( int user_fd  )
{
	int		i;

	for( i=0; i<_POSIX_OPEN_MAX; i++ ) {
		if( file[i].isOpen() && file[i].isDup() && file[i].dup_of == user_fd ) {
			file[i].fd_num = file[user_fd].fd_num;
		}
	}
}

void
OpenFileTable::Restore()
{
	int		i;
	off_t	pos;
	File	*f;
	mode_t	mode;

	if( RemoteSysCalls() ) {
		SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
	} else {
		SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	}
		

	for( i=0; i<_POSIX_OPEN_MAX; i++ ) {
		f = &file[i];
		if( f->isOpen() && !f->isDup() && !f->isPreOpened() ) {
			if( f->isWriteable() && f->isReadable() ) {
				mode = O_RDWR;
			} else if( f->isWriteable() ) {
				mode = O_WRONLY;
			} else if( f->isReadable() ) {
				mode = O_RDONLY;
			} else {
				mode = (mode_t)0;
			}
				// This is not quite right yet - we need to learn whether
				// the file is opened locally or remotely this time around,
				// and set f->remote_access accordingly.
			f->fd_num = open( f->pathname, mode );
			if( f->fd_num < 0 ) {
				perror( "open" );
				abort();
			}

				// No need to seek if pos is 0, but more importantly, this
				// fd could be a tty if pos is 0.
			if( pos != 0 ) {
				pos = lseek( f->fd_num, f->offset, SEEK_SET );
			}

			if( pos != f->offset ) {
				perror( "lseek" );
				abort();
			}
			fix_dups( i );
		}
	}
	if( RemoteSysCalls() ) {
		SetSyscalls( SYS_REMOTE | SYS_MAPPED );
	} else {
		SetSyscalls( SYS_LOCAL | SYS_MAPPED );
	}
}


/*
  Given a string, create and return a pointer to a copy of it.  We use
  the C++ operator new to create the new string.
*/
char *
string_copy( const char *str )
{
	char	*answer;

	answer = new char [ strlen(str) + 1 ];
	strcpy( answer, str );
	return answer;
}

void
string_delete( char *str )
{
	char	*ptr;

#define DEBUGGING
	// put garbage in the string, so we notice if we delete somthing
	// we shouldn't
#if defined(DEBUGGING )
	for( ptr=str; *ptr; ptr++ ) {
		*ptr = '!';
	}
#endif
	delete [] str;
}

char *
shorten( char *path )
{
	char	*ptr;

	if( ptr = strrchr(path,'/') ) {
		return ptr;
	} else {
		return path;
	}
}

extern "C" {


#if defined( SYS_open )
int
open( const char *path, int flags, ... )
{
	va_list ap;
	int		creat_mode = 0;

	if( flags & O_CREAT ) {
		va_start( ap, flags );
		creat_mode = va_arg( ap, int );
	}

	if( MappingFileDescriptors() ) {
		return FileTab.DoOpen( path, flags, creat_mode );
	} else {
		if( LocalSysCalls() ) {
			return syscall( SYS_open, path, flags, creat_mode );
		} else {
			return REMOTE_syscall( SYS_open, path, flags, creat_mode );
		}
	}
}
#endif

#if defined( SYS_close )
int
close( int fd )
{
	if( MappingFileDescriptors() ) {
		return FileTab.DoClose( fd );
	} else {
		if( LocalSysCalls() ) {
			return syscall( SYS_close, fd );
		} else {
			return REMOTE_syscall( SYS_close, fd );
		}
	}
}
#endif

#if defined(SYS_dup)
int
dup( int old )
{
	if( MappingFileDescriptors() ) {
		return FileTab.DoDup( old );
	}

	if( LocalSysCalls() ) {
		return syscall( SYS_dup, old );
	} else {
		return REMOTE_syscall( SYS_dup, old );
	}
}
#endif

#if defined(SYS_dup2)
int
dup2( int old, int new_fd )
{
	int		rval;

	if( MappingFileDescriptors() ) {
		rval =  FileTab.DoDup2( old, new_fd );
		return rval;
	}

	if( LocalSysCalls() ) {
		rval =  syscall( SYS_dup2, old, new_fd );
	} else {
		rval =  REMOTE_syscall( SYS_dup2, old, new_fd );
	}

	return rval;
}
#endif


void
DumpOpenFds()
{
	FileTab.Display();
}


int
MapFd( int user_fd )
{
	return FileTab.Map( user_fd );
}

void
SaveFileState()
{
	FileTab.Save();
}

void
RestoreFileState()
{
	FileTab.Restore();
}

} // end of extern "C"
