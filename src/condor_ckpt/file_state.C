#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "file_state.h"
#include "c_proto.h"

static OpenFileTable	FileTab;
static char				Condor_CWD[ _POSIX_PATH_MAX ];
static int				SyscallMode;

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
	getcwd( Condor_CWD, sizeof(Condor_CWD) );
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
OpenFileTable::RecordOpen( int fd, const char *path, int flags, int method )
{
	int	i;
	char	buf[ _POSIX_PATH_MAX ];

		// find an unused fd
	if( (i = find_avail(0)) < 0 ) {
		errno = EMFILE;
		return -1;
	}

		// set the file access mode
	switch( flags & O_ACCMODE ) {
	  case O_RDONLY:
		file[i].readable = TRUE;
		file[i].writeable = FALSE;
		break;
	  case O_WRONLY:
		file[i].readable = FALSE;
		file[i].writeable = TRUE;
		break;
	  case O_RDWR:
		file[i].readable = TRUE;
		file[i].writeable = TRUE;
		break;
	  default:
		errno = EINVAL;
		return -1;
	}

	file[i].open = TRUE;
	file[i].duplicate = FALSE;
	file[i].pre_opened = FALSE;
	file[i].remote_access = method == FI_RSC;
	file[i].shadow_sock = FALSE;
	file[i].offset = 0;
	file[i].fd_num = fd;
	if( path[0] == '/' ) {
		file[i].pathname = string_copy( path );
	} else {
		sprintf( buf, "%s/%s", Condor_CWD, path );
		file[i].pathname = string_copy( buf );
	}
	return i;
}

void
OpenFileTable::RecordClose( int fd )
{

	BOOL	was_duped;
	int		new_base_file;
	int		i;

		// See if this file is a dup of another
	if( file[fd].isDup() ) {
		file[fd].open = FALSE;
		return;
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
		file[fd].open = FALSE;
		delete [] file[fd].pathname;
		file[fd].pathname = NULL;
		return;
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

		// close the original fd
		// careful - don't delete the pathname
	file[fd].open = FALSE;
}

int
OpenFileTable::RecordPreOpen( int fd )
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
	file[fd].remote_access = TRUE;
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
	if( SyscallMode == SYS_UNMAPPED ) {
		return user_fd;
	}

		// Trying to map a file that isn't open
	if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	return file[user_fd].fd_num;
}

OpenFileTable::RecordDup2( int orig_fd, int new_fd )
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
		// AIX programs which depend on that behavior are hosed...
	if( file[new_fd].isOpen() ) {
		close( new_fd );
	}

		// make new fd a duplicate of the original one
	file[new_fd] = file[orig_fd];
	file[new_fd].duplicate = TRUE;
	file[new_fd].dup_of = orig_fd;
}

OpenFileTable::RecordDup( int user_fd )
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

	return RecordDup2( user_fd, new_fd );
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

void DumpOpenFds()
{
	FileTab.Display();
}

int
MarkFileOpen( int fd, const char *path, int flags, int method )
{
	return FileTab.RecordOpen( fd, path, flags, method );
}

int
PreOpen( int fd )
{
	return FileTab.RecordPreOpen( fd );
}

void
MarkFileClosed( int fd )
{
	FileTab.RecordClose( fd );
}

int
SetSyscalls( int mode )
{
	int	answer;
	answer = SyscallMode;
	SyscallMode = mode;
	return answer;
}

MapFd( int user_fd )
{
	return FileTab.Map( user_fd );
}

int DoDup( int user_fd )
{
	return FileTab.RecordDup( user_fd );
}

int DoDup2( int orig_fd, int new_fd )
{
	return FileTab.RecordDup2( orig_fd, new_fd );
}

} // end of extern "C"
