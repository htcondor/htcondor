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
#include "fcntl.h"
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
#include "condor_sys.h"

#include "condor_debug.h"
static char *_FileName_ = __FILE__;


OpenFileTable	*FileTab;
static char				Condor_CWD[ _POSIX_PATH_MAX ];
static int				MaxOpenFiles;

#define DEBUGGING

#if defined(DEBUGGING)
#	if defined( OSF1 )
		extern "C" void srandom( int );
		extern "C" int random();
#	else
		extern "C" void srandom( int );
		extern "C" long random();
#	endif
#endif

char * shorten( char *path );
extern "C" void Set_CWD( const char *working_dir );

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

	MaxOpenFiles = sysconf(_SC_OPEN_MAX);
	file = new File[ MaxOpenFiles ];

	// getcwd( Condor_CWD, sizeof(Condor_CWD) );
	PreOpen( 0, TRUE, FALSE );
	PreOpen( 1, FALSE, TRUE );
	PreOpen( 2, FALSE, TRUE );
#if 0
	PreOpen( RSC_SOCK, TRUE, TRUE );
	PreOpen( CLIENT_LOG, FALSE, TRUE );

	SetSyscalls( SYS_MAPPED | SYS_LOCAL );
#endif

#if defined(DEBUGGING)
	srandom( 0 );
#endif
}


void
OpenFileTable::Display()
{
	int		i;
	int		scm;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

	dprintf( D_ALWAYS, "%4s %3s %3s %3s %3s %3s %3s %8s %6s %6s %s\n",
		"   ", "Dup", "Pre", "Rem", "Sha", "Rd", "Wr", "Offset", "RealFd", "DupOf", "Pathname" );
	for( i=0; i<MaxOpenFiles; i++ ) {
		if( !file[i].isOpen() ) {
			continue;
		}
		dprintf( D_ALWAYS, "%4d ", i );
		file[i].Display();
	}

	SetSyscalls( scm );
}

void
File::Display()
{
	dprintf( D_ALWAYS, "%3c ", isDup() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isPreOpened() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isRemoteAccess() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isShadowSock() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isReadable() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isWriteable() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%8d ", offset );
	dprintf( D_ALWAYS, "%6d ", real_fd );
	if( isDup() ) {
		dprintf( D_ALWAYS, "%6d ", dup_of );
	} else {
		dprintf( D_ALWAYS, "%6s ", "" );
	}
	if( strlen(pathname) > 25 ) {
		dprintf( D_ALWAYS, "\"...%s\"\n", shorten(pathname) );
	} else {
		dprintf( D_ALWAYS, "\"%s\"\n", pathname );
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
		real_fd = REMOTE_syscall( CONDOR_open, path, flags, mode );
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
	file[user_fd].real_fd = real_fd;
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
	for( i=0; i<MaxOpenFiles; i++ ) {
		if( file[i].isOpen() && file[i].isDup() && file[i].dup_of == fd ) {
			was_duped = TRUE;
			new_base_file = i;
			break;
		}
	}

		// simple case, no dups - just clean up
	if( !was_duped )  {
		if( file[fd].isRemoteAccess() ) {
			rval = REMOTE_syscall( CONDOR_close, file[fd].real_fd );
		} else {
			rval = syscall( SYS_close, file[fd].real_fd );
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
	for( i=0; i<MaxOpenFiles; i++ ) {
		if( file[i].isOpen() && file[i].isDup() && file[i].dup_of == fd ) {
			file[i].dup_of = new_base_file;
		}
	}

		// Mark the original fd as closed.  Careful
		// don't really close it or delete the pathname -
		// those are in use by duplicates.
	file[fd].open = FALSE;

	return 0;
}


int
OpenFileTable::PreOpen( int fd, BOOL readable, BOOL writeable )
{
		// Make sure fd not already open
	if( file[fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[fd].readable = readable;
	file[fd].writeable = writeable;

		// Record reasonable values for everything else
	file[fd].open = TRUE;
	file[fd].duplicate = FALSE;
	file[fd].pre_opened = TRUE;
	file[fd].remote_access = RemoteSysCalls();
	file[fd].shadow_sock = FALSE;
	file[fd].offset = 0;
	file[fd].real_fd = fd;
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

	return file[user_fd].real_fd;
}

int
OpenFileTable::IsLocalAccess( int user_fd )
{
	return !file[user_fd].isRemoteAccess();
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
	if( new_fd < 0 || new_fd >= MaxOpenFiles ) {
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

	for( i=0; i<MaxOpenFiles; i++ ) {
		f = &file[i];
		if( f->isOpen() && !f->isDup() ) {
			if( f->isRemoteAccess() ) {
				pos = REMOTE_syscall( CONDOR_lseek, f->real_fd, (off_t)0,SEEK_CUR);
			} else {
				pos = syscall( SYS_lseek, f->real_fd, (off_t)0, SEEK_CUR);
			}
			if( pos < (off_t)0 && f->isPreOpened() ) {
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
  differend real_fd now than it did at the time of the checkpoint.  Here
  we go through the open file table, and fix up the real_fd of any
  duplicates of this file.
*/
void
OpenFileTable::fix_dups( int user_fd  )
{
	int		i;

	for( i=0; i<MaxOpenFiles; i++ ) {
		if( file[i].isOpen() && file[i].isDup() && file[i].dup_of == user_fd ) {
			file[i].real_fd = file[user_fd].real_fd;
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
	int		scm;

	if( RemoteSysCalls() ) {
		scm = SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
	} else {
		scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	}

#if defined(DEBUGGING)
	 /*
	When we are operating in remote mode, we could potentially get
	different sets of real file descriptor numbers to correspond
	with the user's fd numbers after each migration.  Here we
	simulate that even in local mode so that we can more thoroughly
	test the file descriptor mapping stuff.
	 */
	int		n_files;
	n_files = random() % 5;

	for( i=0; i<n_files; i++ ) {
		(void)open("/dev/null",O_RDONLY);
	}
#endif
		

	for( i=0; i<MaxOpenFiles; i++ ) {
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
			f->real_fd = open( f->pathname, mode );
			if( f->real_fd < 0 ) {
				perror( "open" );
				abort();
			}

				// No need to seek if offset is 0, but more importantly, this
				// fd could be a tty if offset is 0.
			if( f->offset != 0 ) {
				if( RemoteSysCalls() ) {
					pos = REMOTE_syscall( CONDOR_lseek,
						  f->real_fd, f->offset, SEEK_SET );
				} else {
					pos = syscall( SYS_lseek, f->real_fd, f->offset, SEEK_SET );
				}

				if( pos != f->offset ) {
					perror( "lseek" );
					abort();
				}
			}

			fix_dups( i );
		}
	}
	if( RemoteSysCalls() ) {
		SetSyscalls( SYS_REMOTE | SYS_MAPPED );
	} else {
		// SetSyscalls( SYS_LOCAL | SYS_MAPPED );
		SetSyscalls( scm);
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
		return FileTab->DoOpen( path, flags, creat_mode );
	} else {
		if( LocalSysCalls() ) {
			return syscall( SYS_open, path, flags, creat_mode );
		} else {
			return REMOTE_syscall( CONDOR_open, path, flags, creat_mode );
		}
	}
}
#endif

#if defined(OSF1)
	int
	_open( const char *path, int flags, ... )
	{
		va_list ap;
		int		creat_mode = 0;

		if( flags & O_CREAT ) {
			va_start( ap, flags );
			creat_mode = va_arg( ap, int );
			return open( path, flags, creat_mode );
		} else {
			return open( path, flags );
		}

	}

	/* Force isatty() to be undefined so programs that use it get it from
	   the condor library rather than libc.a.
	*/
	int
	not_used()
	{
		return isatty(0);
	}
#endif

#if defined( SYS_close )
int
close( int fd )
{
	if( MappingFileDescriptors() ) {
		return FileTab->DoClose( fd );
	} else {
		if( LocalSysCalls() ) {
			return syscall( SYS_close, fd );
		} else {
			return REMOTE_syscall( CONDOR_close, fd );
		}
	}
}
#endif

#if defined(SYS_dup)
int
dup( int old )
{
	if( MappingFileDescriptors() ) {
		return FileTab->DoDup( old );
	}

	if( LocalSysCalls() ) {
		return syscall( SYS_dup, old );
	} else {
		return REMOTE_syscall( CONDOR_dup, old );
	}
}
#endif

#if defined(SYS_dup2)
int
dup2( int old, int new_fd )
{
	int		rval;

	if( MappingFileDescriptors() ) {
		rval =  FileTab->DoDup2( old, new_fd );
		return rval;
	}

	if( LocalSysCalls() ) {
		rval =  syscall( SYS_dup2, old, new_fd );
	} else {
		rval =  REMOTE_syscall( CONDOR_dup2, old, new_fd );
	}

	return rval;
}
#endif

extern "C" void DisplaySyscallMode();

void
DumpOpenFds()
{

	FileTab->Display();
	DisplaySyscallMode();
}


int
MapFd( int user_fd )
{
	return FileTab->Map( user_fd );
}

void
SaveFileState()
{
	FileTab->Save();
}

void
RestoreFileState()
{
	FileTab->Restore();
}

void
Set_CWD( const char *working_dir )
{
	strcpy( Condor_CWD, working_dir );
}

int
LocalAccess( int user_fd )
{
	return FileTab->IsLocalAccess( user_fd );
}

int
pre_open( int fd, BOOL readable, BOOL writeable )
{
	return FileTab->PreOpen( fd, readable, writeable );
}

void
init_file_table()
{
	FileTab = new OpenFileTable();
}


} // end of extern "C"
