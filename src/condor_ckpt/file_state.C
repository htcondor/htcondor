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

#include "condor_common.h"
#include "condor_syscalls.h"
#include <stdarg.h>
#include "file_state.h"
#include "file_table_interf.h"
#include "condor_sys.h"
#include "condor_file_info.h"

#if defined(LINUX)
#include <sys/socketcall.h>
#	if defined(GLIBC)
#		define SYS_SOCKET SOCKOP_socket
#	endif
#endif

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

#include "image.h"
extern Image MyImage;

extern "C" {
extern int ioserver_open(const char *path, int oflag, mode_t mode);

extern int ioserver_close(int filedes);

extern off_t ioserver_lseek(int filedes, off_t offset, int whence);

extern ssize_t ioserver_read(int filedes, char *buf, unsigned int size);

extern ssize_t ioserver_write(int filedes, const char *buf, unsigned int size);

int IOServerAccess( int user_fd );
}


OpenFileTable	*FileTab = NULL;
static char				Condor_CWD[ _POSIX_PATH_MAX ];
static int				MaxOpenFiles;
static int				Condor_isremote;

char * shorten( char *path );
extern "C" void Set_CWD( const char *working_dir );
extern "C" sigset_t block_condor_signals(void);
extern "C" void restore_condor_sigmask(sigset_t omask);

extern int errno;
extern volatile int check_sig;

extern "C" {
void printft(void)
{
  FileTab->Display();
}

void filetabdup2(int ofd, int nfd)
{
  FileTab->DoDup2(ofd, nfd);
}

void filetabclose(int fd)
{
  FileTab->DoClose(fd);
}
}
void
File::Init()
{
	open = FALSE;
	firstBuf = NULL;
	pathname = 0;
	ioserversocket = -1;
}

void
OpenFileTable::Init()
{
	int		i;

	SetSyscalls( SYS_UNMAPPED | SYS_LOCAL );

	MaxOpenFiles = sysconf(_SC_OPEN_MAX);
	
	file = new File[ MaxOpenFiles ];
	bufCount = 0;
	for( i=0; i<MaxOpenFiles; i++ ) {
		file[i].Init();
	}

	PreOpen( 0, TRUE, FALSE, FALSE );
	PreOpen( 1, FALSE, TRUE, FALSE );
	PreOpen( 2, FALSE, TRUE, FALSE );

	getcwd( Condor_CWD, sizeof(Condor_CWD) );
}


void
OpenFileTable::Display()
{
	int		i;
	int		scm;


	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

	dprintf( D_ALWAYS, "%4s %3s %3s %3s %3s %3s %3s %8s %6s %6s %s\n",
		"   ", "Dup", "Pre", "Rem", "IO " "Sha", "Rd", "Wr", "Offset", "RealFd", "Sockfd" "DupOf", "Pathname" );
	for( i=0; i<MaxOpenFiles; i++ ) {
		if( !file[i].isOpen() ) {
			continue;
		}
		dprintf( D_ALWAYS, "%4d ", i );
		file[i].Display();
	}
	dprintf( D_ALWAYS, "CWD = \"%s\"\n", cwd );

	SetSyscalls( scm );
}

void
File::Display()
{
	dprintf( D_ALWAYS, "%3c ", isDup() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isPreOpened() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isRemoteAccess() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isIOServerAccess() ? 'T' : 'F' );
 	dprintf( D_ALWAYS, "%3c ", isShadowSock() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isReadable() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%3c ", isWriteable() ? 'T' : 'F' );
	dprintf( D_ALWAYS, "%8d ", offset );
	dprintf( D_ALWAYS, "%6d ", real_fd );
	dprintf( D_ALWAYS, "%6d ", ioserversocket );
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

extern "C" {

void PatchSockets( int oldsockfd, int newsockfd )
{
 	for ( int i = 0 ; i < MaxOpenFiles; i++)
		if ( FileTab->getSockFd(i) == oldsockfd )
		{
			FileTab->setSockFd(i,newsockfd);
			return;
		}
}

}

int
OpenFileTable::DoOpen(
	const char *path, int flags, int real_fd, int is_remote, int filetype = -1 )
{
	int	user_fd;
	int     rval;
	char	buf[ _POSIX_PATH_MAX ];
	off_t   tempSize;

	if (MyImage.GetMode() == STANDALONE) {
		user_fd = real_fd;
	} else {
		// find an unused fd
		if( (user_fd = find_avail(0)) < 0 ) {
			errno = EMFILE;
			return -1;
		}
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
	file[user_fd].remote_access = is_remote;
	file[user_fd].ioserver_access = FALSE;
	file[user_fd].ioserversocket = -1;
	file[user_fd].shadow_sock = FALSE;
	file[user_fd].offset = 0;

#if defined( BufferCondorIO )
	        // set the initial file size
	if( LocalSysCalls() || FileTab->IsLocalAccess( user_fd ) ) {
		tempSize = syscall( SYS_lseek, real_fd, 0, SEEK_END );
		if( tempSize > 0 ) {
			rval = syscall( SYS_lseek, real_fd, -tempSize, SEEK_END );
			if( rval < 0 ) {
				return -1;
			}
			file[user_fd].size = tempSize; 
		} else if( tempSize < 0 ) {
			return -1;
		} else {
			file[user_fd].size = 0;
		}
	} else {
	        tempSize = REMOTE_syscall( CONDOR_lseek, real_fd, 0, SEEK_END );
		if( tempSize > 0 ) {
		        rval = REMOTE_syscall( CONDOR_lseek, real_fd, -tempSize, SEEK_END );
			if( rval < 0 ) {
			        return -1;
			}  
			file[user_fd].size = tempSize;
		} else if( tempSize < 0 ) {
		        return -1;
		} else {
		        file[user_fd].size = 0;
		}		        
	 }
#endif /* defined( BufferCondorIO ) */

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

	if( !file[fd].isOpen() || file[fd].isShadowSock() ) {
		errno = EBADF;
		return -1;
	}


		// See if this file is a dup of another
	if( file[fd].isDup() ) {
		file[fd].open = FALSE;
		return 0;
	}

//	DisplayBuf();                     // for debugging purpose only.
	FlushBuf();		

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
		/* we don't want to be interrupted by a checkpoint between when
		   we close this file and we mark it closed in our local data
		   structure */
		sigset_t omask = block_condor_signals();
		if( file[fd].isRemoteAccess() ) {
			rval = REMOTE_syscall( CONDOR_close, file[fd].real_fd );
		} else {
			rval = syscall( SYS_close, file[fd].real_fd );
		}
		file[fd].open = FALSE;
		delete [] file[fd].pathname;
		file[fd].pathname = NULL;
		restore_condor_sigmask(omask);
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

	// If in standalone mode, close this fd.  The dup is an actual dup.
	if (MyImage.GetMode() == STANDALONE) {
		file[new_base_file].real_fd = new_base_file;
		return syscall( SYS_close, fd );
	}

	return 0;
}


int
OpenFileTable::PreOpen(
	int fd, BOOL readable, BOOL writeable, BOOL shadow_connection )
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
	file[fd].ioserver_access = FALSE;
	file[fd].ioserversocket = -1;
	file[fd].shadow_sock = shadow_connection;
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
	return ( !file[user_fd].isRemoteAccess() && !file[user_fd].isIOServerAccess() );
}

int
OpenFileTable::isIOServerAccess( int fd )
{
  if ( !file[fd].isOpen() ) return 0 ;
  return file[fd].isIOServerAccess();
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
  
  //dprintf(D_ALWAYS,"Just before closing %d\n",new_fd);
  //Display();
  // POSIX.1 says do it this way, AIX does it differently - any
  // AIX programs which depend on that behavior are now hosed...
  if( file[new_fd].isOpen() ) {
    DoClose( new_fd );
  }
  //dprintf(D_ALWAYS,"Just after closing %d\n",new_fd);
  
  //Display();
  // make new fd a duplicate of the original one
  // Note: be careful to handle the case of a dup of a dup... we
  //	need to find the real original (non-dupped) entry here or
  //	we will get messed up after a checkpoint/restart - Todd 2/98
  while ( file[orig_fd].isDup() ) {   // find the real non-dup entry
	orig_fd = file[orig_fd].dup_of;
  }
  file[new_fd] = file[orig_fd];
  file[new_fd].duplicate = TRUE;
  file[new_fd].dup_of = orig_fd;

  if (MyImage.GetMode() == STANDALONE) {
	  file[new_fd].real_fd = new_fd;
  }
  
  //dprintf(D_ALWAYS,"Just after duplicating %d\n",new_fd);
  //Display();
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

	// If STANDALONE mode, make an actual dup of the fd, and make
	// sure to return that fd to the caller.
	if (MyImage.GetMode() == STANDALONE) {
		new_fd = syscall( SYS_dup, user_fd );
	} 
	else {
		if( (new_fd=find_avail(0)) < 0 ) {
			errno = EMFILE;
			return -1;
		}
	}

	return DoDup2( user_fd, new_fd );
}

int
OpenFileTable::DoSocket(int addr_family, int type, int protocol )
{
	int	user_fd;
	int	real_fd;
	char	buf[ _POSIX_PATH_MAX ];

//	dprintf(D_ALWAYS,"Dosocket called ......\n");
	
#if defined(LINUX)
	unsigned long	socket_args[4] = {
		(unsigned long)addr_family,
		(unsigned long)type,
		(unsigned long)protocol
	};
#endif

		// Try to create the socket
	if( LocalSysCalls() ) {
#if defined(LINUX)
		// Linux combines syscalls such as socket, accept, recv, send, ...
		// into a single system call - SYS_socketcall.  Parameters are
		// passed through the third argument to syscall, in this case
		// socket_args.  The type of call (socket, accept,...) is given
		// as the second argument.  The valid types are listed in
		// /usr/include/sys/socketcall.h  The kernel source warns that
		// this interface may change in the future so that this
		// code may need adjustment! - Greger
		real_fd = syscall( SYS_socketcall, SYS_SOCKET, socket_args );
		fprintf(stderr, "Socketcall returned %d, errno=%d\n",
			real_fd, errno);
#elif defined(Solaris)
		real_fd = socket( addr_family, type, protocol );
#else
		real_fd = syscall( SYS_socket, addr_family, type, protocol );
#endif
	} else {
		real_fd = -1;
	}

		// Stop here if there was an error
	if( real_fd < 0 ) {
		return -1;
	}

	if (MyImage.GetMode() == STANDALONE) {
		user_fd = real_fd;
	} else {
		// find an unused fd
		if( (user_fd = find_avail(0)) < 0 ) {
			errno = EMFILE;
			return -1;
		}
	}

//	fprintf(stderr, "JCP: DoSocket returning %d\n", user_fd); fflush(stderr);

	file[user_fd].readable = TRUE;
	file[user_fd].writeable = TRUE;
	file[user_fd].open = TRUE;
	file[user_fd].duplicate = FALSE;
	file[user_fd].pre_opened = TRUE;  /* Make it not be re-opened */
	file[user_fd].remote_access = RemoteSysCalls();
	file[user_fd].ioserver_access = FALSE;
	file[user_fd].ioserversocket = -1;
	file[user_fd].shadow_sock = FALSE;
	file[user_fd].offset = 0;
	file[user_fd].real_fd = real_fd;
	file[user_fd].pathname = (char *) 0;
	return user_fd;
}


#if defined(SYS_accept)
int
OpenFileTable::DoAccept(int s, struct sockaddr *addr, int *addrlen )
{
	int	user_fd;
	int	real_fd;
	char	buf[ _POSIX_PATH_MAX ];

		// Try to create the socket
	if( LocalSysCalls() ) {
		real_fd = syscall( SYS_accept, s, addr, addrlen );
	} else {
		real_fd = -1;
	}


		// Stop here if there was an error
	if( real_fd < 0 ) {
		return -1;
	}

	if (MyImage.GetMode() == STANDALONE) {
		user_fd = real_fd;
	} else {
		// find an unused fd
		if( (user_fd = find_avail(0)) < 0 ) {
			errno = EMFILE;
			return -1;
		}
	}

	file[user_fd].readable = TRUE;
	file[user_fd].writeable = TRUE;
	file[user_fd].open = TRUE;
	file[user_fd].duplicate = FALSE;
	file[user_fd].pre_opened = TRUE;  /* Make it not be re-opened */
	file[user_fd].remote_access = RemoteSysCalls();
	file[user_fd].ioserver_access = FALSE;
	file[user_fd].ioserversocket = -1;
	file[user_fd].shadow_sock = FALSE;
	file[user_fd].offset = 0;
	file[user_fd].real_fd = real_fd;
	file[user_fd].pathname = (char *) 0;
	return user_fd;
}
#else
int
OpenFileTable::DoAccept(int s, struct sockaddr *addr, int *addrlen )
{
	return -1;
}
#endif


void
OpenFileTable::Save()
{
	int		i, scm;
	off_t	pos;
	File	*f;

	getcwd( cwd, sizeof(cwd) );

	for( i=0; i<MaxOpenFiles; i++ ) {
		f = &file[i];
		if( f->isOpen() && !f->isDup() ) {
//			dprintf(D_ALWAYS,"**** Current file entry is %d\n",i);
			if ( f->isIOServerAccess() ) {
				scm = SetSyscalls( SYS_LOCAL | SYS_MAPPED);
				pos = ioserver_lseek( i, (off_t)0, SEEK_CUR);
//				dprintf(D_ALWAYS,">>>>> IO server seek returns %d\n",pos);
				SetSyscalls( scm );
			}
			else if( f->isRemoteAccess() ) {
				pos = REMOTE_syscall( CONDOR_lseek, f->real_fd, (off_t)0,SEEK_CUR);
			} else {
				pos = syscall( SYS_lseek, f->real_fd, (off_t)0, SEEK_CUR);
			}
			if( pos < (off_t)0 && f->isPreOpened() ) {
				pos = (off_t)0;		// probably it's a tty
			}
			if( pos < (off_t)0 ) {
				dprintf(D_ALWAYS, "lseek failed for %d: %s\n", f->real_fd,
						strerror(errno));
				Suicide();
			}
			f->offset = pos;

				// Close shadow end so we don't have a fd leak if
				// we're doing checkpoint/restart repetitively
				// 8/95: modified so that we only do this on a
				// SIGTSTP (i.e. a checkpoint & vacate).  Specifically, we
				// want to _avoid_ doing this during a periodic checkpoint
				// -Todd Tannenbaum
			if( (f->isRemoteAccess()) && (check_sig == SIGTSTP) ) {
				REMOTE_syscall( CONDOR_close, f->real_fd );
			}
	/*
			if ((f->isIOServerAccess()) && (check_sig == SIGTSTP)) {
				scm = SetSyscalls( SYS_LOCAL | SYS_MAPPED );
				char *path = strdup(f->pathname);
//				ioserver_close( i );
//				DoOpen(path,f->flags,-1,0,IS_IOSERVER);
				SetSyscalls( scm );
			}
	*/

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
      // If we're in STANDALONE mode, this must be an actual dup
	    // of the fd.  -Jim B.
      if (MyImage.GetMode() == STANDALONE) {
#if defined(SYS_dup2)
	syscall( SYS_dup2, user_fd, i );
#else
	dup2( user_fd, i );
#endif
      }
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
	int		scm, init_scm;
	
	// restoring state of IOserver files..
	for(i=0;i<MaxOpenFiles;i++)
	  {
	    f = &file[i];
	    if (f->isOpen())
	      {
//		dprintf(D_ALWAYS,"%d : pathname = %s, isopen = %d",i,file[i].pathname, f->isOpen());
	      }
	    
	    
	    if (f->isOpen() && !(strcmp(file[i].pathname, "/IO_Socket")))
	      {
		file[i].open = FALSE;
		delete [] file[i].pathname;
		file[i].pathname = NULL;
	      }
	  }
	// 


	if( RemoteSysCalls() ) {
		init_scm = SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
	} else {
		init_scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	}


	chdir( cwd );
//	Display();
	for( i=0; i<MaxOpenFiles; i++ ) {
		f = &file[i];
		if ( (f->pathname) && (!strcmp(f->pathname,"/IO_Socket") ))
		  continue;
		
		if( f->isOpen() && !f->isDup() && !f->isPreOpened() ) {
//			dprintf(D_ALWAYS,"Current restoring file is %d\n",i);
			if( f->isWriteable() && f->isReadable() ) {
				mode = O_RDWR;
			} else if( f->isWriteable() ) {
				mode = O_WRONLY;
			} else if( f->isReadable() ) {
				mode = O_RDONLY;
			} else {
				mode = (mode_t)0;
			}
			
			
			if (f->ioserver_access)
			  {
			    scm=SetSyscalls(SYS_LOCAL|SYS_MAPPED);
			    int temp  = ioserver_open( f->pathname, f->flags, f->mode);
			    file[temp].offset = file[i].offset;

			    SetSyscalls(scm);
			    
//			    dprintf(D_ALWAYS,"before duping... \n");
//			    printft();
			    
			    file[i] = file[temp];
			    file[i].duplicate = TRUE;
			    file[i].dup_of = temp;

//			    dprintf(D_ALWAYS,"after duping... \n");
//			    printft();

//			    dprintf(D_ALWAYS,"---  table printed done \n");
//			    FileTab->DoClose( temp );
//			    dprintf(D_ALWAYS,"---  close done \n");
//			    printft();
			  }
			
			else
			  f->real_fd = open( f->pathname, mode );
			if( f->real_fd < 0 ) {
			  dprintf(D_ALWAYS, "i=%d, filename=%s\n", i, f->pathname);
			  dprintf(D_ALWAYS, "open: %s\n", strerror(errno) );
			  Suicide();
			}
			
			// the open above saved in a global variable whether or not
			// the file is remote (i.e. accessable only via shadow) or not.
			// we must now save this into the file state table immediately,
			// because we assume it is correct in what follows...
			// Todd Tannenbaum, 3/14/95
			if ( !f->ioserver_access )
			   f->remote_access = Condor_isremote;
			else
			   f->remote_access = FALSE ;
			
			// In STANDALONE mode, we must make sure that the real_fd
			// returned by the OS is the same as the "user_fd" that we've
			// previously given to the App.  JCP 8/96

			if (MyImage.GetMode() == STANDALONE && f->real_fd != i) {
#if defined(SYS_dup2)
			  syscall( SYS_dup2, f->real_fd, i);
#else
			  dup2(f->real_fd, i);
#endif
			  syscall( SYS_close, f->real_fd );
			  f->real_fd = i;
			}
			
			// No need to seek if offset is 0, but more importantly, this
			// fd could be a tty if offset is 0.
			if( f->offset != 0 ) {
			  // for seeking to right position
			  // restoring state of IOserver files..
			  if (f->ioserver_access)
			  {
			    scm = SetSyscalls( SYS_LOCAL | SYS_MAPPED );
			    pos = ioserver_lseek(i,f->offset,SEEK_SET);
			    SetSyscalls( scm );
			  }
			  else
			    if( f->isRemoteAccess() ) {
			      pos = REMOTE_syscall( CONDOR_lseek,
						   f->real_fd, f->offset, SEEK_SET );
			    } else {
			      pos = syscall( SYS_lseek, f->real_fd, f->offset, SEEK_SET );
			    }

			  
			  if( pos != f->offset ) {
			    dprintf( D_ALWAYS,  "ioserver lseek: %s\n", strerror(errno) );
				Suicide();
			  }
			}
			
			fix_dups( i );
		}
	}
	if( RemoteSysCalls() ) {
		SetSyscalls( SYS_REMOTE | SYS_MAPPED );
	} else {
		// SetSyscalls( SYS_LOCAL | SYS_MAPPED );
		SetSyscalls( init_scm);
	}
}

///////////////////////////////////////////////////////////////////////////////
//
//  Methods for setting and accessing various IO server related parameters
//
///////////////////////////////////////////////////////////////////////////////

int 
OpenFileTable::setServerName( int user_fd, char *name )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[user_fd].ioservername = strdup( name ) ;
	return 0 ;
}

int
OpenFileTable::setServerPort( int user_fd, int port )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[user_fd].ioserverport = port ;
	return 0;
}

int
OpenFileTable::setSockFd( int user_fd, int sockfd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[user_fd].ioserversocket = sockfd ;
	return 0;
}

int
OpenFileTable::setRemoteFd( int user_fd, int fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[user_fd].real_fd = fd ;
	return 0;
}


int
OpenFileTable::setOffset( int user_fd, int off )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[user_fd].offset = off ;
	return 0;
}


int
OpenFileTable::setFileName( int user_fd, char *name )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[user_fd].pathname = strdup( name );
	return 0;
} 

//
// This function inserts a new read buffer block to the list. 
//
ssize_t
OpenFileTable::InsertR( int user_fd, void *buf, int tempPos, int tempRead, int tempFetch, BufElem *prevP, BufElem *afterP )
{
        int rval;
        int real_fd;
	BufElem *newBuf;

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
		return -1;
	}                      
	 
	newBuf = new BufElem;
	if( !newBuf ) {
		dprintf(D_ALWAYS, "failed to alloc new BufElem!\n");
		Suicide();
	}
		
	if( !prevP ) {
	         newBuf->next = afterP;
		 file[user_fd].firstBuf = newBuf;
	} else {
	         prevP->next = newBuf;
		 newBuf->next = afterP;
	}		  

	newBuf->buf = new char[tempFetch+1];
	if( !newBuf->buf ) {
		dprintf(D_ALWAYS, "failed to alloc file buffer!\n");
		Suicide();
	}
	rval = REMOTE_syscall( CONDOR_lseekread, real_fd, tempPos, SEEK_SET, (void *)newBuf->buf, tempFetch );
	if( rval != tempFetch ) {
	        return -1;
	}
	newBuf->offset = tempPos;
	newBuf->len = rval;
	bufCount += (sizeof(BufElem)+1) + rval;      
	strncpy( (char *) buf, newBuf->buf, tempRead );
	char * tempB = ( char * ) buf;
	tempB[tempRead] = '\0';

	return tempRead;
}

//
// This function handles a reading (fetching) from a buffer like 
//     Fetch : -----        or -----       or -----
//     Buffer :       -----         -----       -----
// Merge in cases like  
//     Fetch : -----        or -----    
//     Buffer :     -----        -----
//
ssize_t
OpenFileTable::OverlapR1( int user_fd, void *buf, int tempPos, int tempRead, int tempFetch, BufElem *prevP, BufElem *afterP )
{
        int rval;
        int real_fd;
	char * tempB;

	if( afterP == NULL ) {
	        return -1;
	}

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
		return -1;
	}                       
                                                // case I.
	if( tempPos+tempFetch < afterP->offset ) {
	        return InsertR( user_fd, buf, tempPos, tempRead, tempFetch, prevP, afterP );
	} else {                                // case II & III.
	        if( !prevP ) {
		        file[user_fd].firstBuf = afterP;
		} else {
		        prevP->next = afterP;
		}

	        tempFetch = afterP->offset - tempPos;     
		char *fetch = new char[tempFetch+1];
		if( !fetch ) {
			dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
			Suicide();
		}
		rval = REMOTE_syscall( CONDOR_lseekread, real_fd, tempPos, SEEK_SET, fetch, tempFetch );
		if( rval != tempFetch ) {
		        return -1;
		}
		if( tempRead <= tempFetch ) {
		        strncpy( (char *) buf, fetch, tempRead );
			tempB = ( char * ) buf;
			tempB[tempRead] = '\0';
		} else {
		        char *temp = ( char * ) buf;
		        strncpy( (char *) buf, fetch, tempFetch );
			temp += tempFetch;
			strncpy( temp, afterP->buf, tempRead-tempFetch );
			tempB = ( char * ) buf;
			tempB[tempRead] = '\0';
		}
		char *newbuf = new char[tempFetch+afterP->len+1];
		if( !newbuf ) {
			dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
			Suicide();
		}
		char *tempP = newbuf;
		strncpy( newbuf, fetch, tempFetch );
		tempP += tempFetch;
		strncpy( tempP, afterP->buf, afterP->len );
		newbuf[tempFetch+afterP->len] = '\0';
		afterP->len += tempFetch;
		afterP->offset = tempPos;
		delete []fetch;
		delete []afterP->buf;
		afterP->buf = newbuf;
		bufCount += tempFetch;

		return tempRead;
	}
}

//
// This function handles a reading (fetching) from a buffer like 
//     Fetch :        -----    or    -----    or    -----
//     Buffer :  -----             -----        -----   -----
// Merge the buffers
//
ssize_t
OpenFileTable::OverlapR2( int user_fd, void *buf, int tempPos, int tempRead, int tempFetch, BufElem *prevP, BufElem *afterP )
{
        int rval;
        int real_fd;
	char *tempB;

	if( prevP == NULL ) {
	        return -1;
	}

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
		return -1;
	}                       
                                                // case I & case II
	if( !afterP || tempPos+tempFetch < afterP->offset ) {
	    tempFetch -= prevP->offset + prevP->len - tempPos;

	    char *fetch = new char[tempFetch+1];
	    if( !fetch ) {
			dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
			Suicide();
	    }
	    rval = REMOTE_syscall( CONDOR_lseekread, real_fd, prevP->offset + prevP->len, SEEK_SET, fetch, tempFetch );
	    if( rval != tempFetch ) {
	            return -1;
	    }
    	
	    char *temp = prevP->buf + tempPos - prevP->offset;
	    if( tempRead <= prevP->offset+prevP->len - tempPos ) {
	        strncpy( (char *) buf, temp, tempRead );
		tempB = ( char * ) buf;
		tempB[tempRead] = '\0';
	    } else {  
	        char *tempP = (char *) buf;
	        strncpy( (char *) buf, temp, prevP->offset + prevP->len - tempPos );
		tempP += prevP->offset + prevP->len - tempPos;
		strncpy( tempP, fetch, tempRead - prevP->offset - prevP->len + tempPos );
		tempB = ( char * ) buf;
		tempB[tempRead] = '\0';
	    }

	    char *newbuf = new char[tempFetch+prevP->len+1];
	    if( !newbuf ) {
			dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
			Suicide();
	    }
	    char *tempP = newbuf;
	    strncpy( newbuf, prevP->buf, prevP->len );
	    tempP += prevP->len;
	    strncpy( tempP, fetch, tempFetch ); 
	    newbuf[tempFetch+prevP->len] = '\0';
	    prevP->len += tempFetch;
	    delete []fetch;
	    delete []prevP->buf;
	    prevP->buf = newbuf;
	    prevP->next = afterP;
	    bufCount += tempFetch;
        } else {                                // case III
	    tempFetch = afterP->offset - prevP->offset - prevP->len;

	    char *fetch = new char[tempFetch+1];
	    if( !fetch ) {
			dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
			Suicide();
	    }
	    rval = REMOTE_syscall( CONDOR_lseekread, real_fd, prevP->offset + prevP->len, SEEK_SET, fetch, tempFetch );
	    if( rval != tempFetch ) {
	            return -1;
	    }
    	
	    char *temp = prevP->buf + tempPos - prevP->offset;
	    if( tempRead <= prevP->offset+prevP->len - tempPos ) {
	        strncpy( (char *) buf, temp, tempRead );
		tempB = ( char * ) buf;
		tempB[tempRead] = '\0';
	    } else if( tempRead <= afterP->offset - tempPos ) {
	        char *tempP = ( char * ) buf; 
	        strncpy( (char *) buf, temp, prevP->offset + prevP->len - tempPos );
		tempP += prevP->offset + prevP->len - tempPos;
		strncpy( tempP, fetch, tempRead - prevP->offset - prevP->len + tempPos );
		tempB = ( char * ) buf;
		tempB[tempRead] = '\0';
	    } else {
	        char *tempP = ( char * ) buf;
	        strncpy( (char *) buf, temp, prevP->offset + prevP->len - tempPos );
		tempP += prevP->offset + prevP->len - tempPos;
		strncpy( tempP, fetch, tempFetch );
		tempP += tempFetch;
       		strncpy( tempP, afterP->buf, tempPos+tempRead-afterP->offset );
		tempB = ( char * ) buf;
		tempB[tempRead] = '\0';
	    }

	    char *newbuf = new char[afterP->offset+afterP->len-prevP->offset+1];
	    if( !newbuf ) {
			dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
			Suicide();
	    }
	    char *tempP = newbuf;
	    strncpy( newbuf, prevP->buf, prevP->len );
	    tempP += prevP->len;
	    strncpy( tempP, fetch, tempFetch ); 
	    tempP += tempFetch;
	    strncpy( tempP, afterP->buf, afterP->len ); 
	    newbuf[tempFetch+prevP->len+afterP->len] = '\0';
	    prevP->len += tempFetch + afterP->len;
	    delete []fetch;
	    delete []prevP->buf;
	    delete []afterP->buf;
	    prevP->buf = newbuf;
	    prevP->next = afterP->next;
	    delete afterP;
	    bufCount += tempFetch - (sizeof(BufElem)+1);	    
	}
	
	return tempRead;
}

//
// Prefetching for read-only files.
//
ssize_t 
OpenFileTable::PreFetch( int user_fd, void *buf, size_t nbyte )
{
        int tempPos;
	int tempSize;
	int rval; 
	char *tempB;
	int real_fd;
	int tempRead;
	int tempFetch;
	BufElem *prevP;
	BufElem *afterP;

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
		return -1;
	}                      

	tempPos = file[user_fd].getOffset();
	tempSize = file[user_fd].getSize();

	tempRead = tempSize - tempPos;      // # of bytes need to transfer.
	if( tempRead > nbyte ) {
	    tempRead = nbyte;
        }
	if( tempRead > MAXBUF - (sizeof(BufElem)+1) ) { // if too large, read directly.
	    rval = REMOTE_syscall( CONDOR_lseekread, real_fd, tempPos, SEEK_SET, buf, nbyte );
	    return rval;
	}

	if( tempRead > PREFETCH ) {         // # of bytes to prefetch.
	    tempFetch = tempRead;
	} else if( tempSize - tempPos < PREFETCH ) {
	    tempFetch = tempSize - tempPos;
	} else {
	    tempFetch = PREFETCH;
	}
	if( tempFetch + bufCount > MAXBUF - (sizeof(BufElem)+1) ) {
	    FlushBuf();                     // if too full, flush the buffer.
	}          	

	if( !file[user_fd].firstBuf ) {     // bring the 1st buffer block.
	        return InsertR( user_fd, buf, tempPos, tempRead, tempFetch, NULL, NULL );
        }

	prevP = NULL;
	for( afterP=file[user_fd].firstBuf; afterP; afterP=afterP->next ) {
	    if( tempPos < afterP->offset ) {
	        if( tempPos+tempFetch-1 >= afterP->offset+afterP->len ) {
		    BufElem *currP;
		    while( 1 ) {
		        currP = afterP->next;
			afterP->next = NULL;
			delete []afterP->buf;
			bufCount -= (sizeof(BufElem)+1) + afterP->len;
			delete afterP;
			afterP = currP;
			if( !currP ) {
			    break;
			}
			if( tempPos+tempFetch-1 >= afterP->offset+afterP->len ) {
			    ;               // keep erasing old buffers.
			} else {
			    return OverlapR1( user_fd, buf, tempPos, tempRead, tempFetch, prevP, afterP );
			}
		    }
		    return InsertR( user_fd, buf, tempPos, tempRead, tempFetch, prevP, afterP );		    
		} else {		  
		    return OverlapR1( user_fd, buf, tempPos, tempRead, tempFetch, prevP, afterP );
		}                           // cache hits.
            } else if( tempPos+tempRead-1 < afterP->offset+afterP->len ) {
	        char *tempP = afterP->buf + ( tempPos - afterP->offset );
	        strncpy( (char *) buf, tempP, tempRead );
		tempB = ( char * ) buf;
		tempB[tempRead] = '\0';
		return tempRead;
	    } else if( tempPos > afterP->offset+afterP->len ) {
	        prevP = afterP;             // go to next buffer block.
	    } else {
	        BufElem *currP;
	        BufElem *saveP = afterP;
		afterP = afterP->next;
		while( 1 ) {
		    currP = afterP;
		    if( !currP ) {
		        break;
		    }
		    afterP = afterP->next;
		    if( tempPos+tempFetch >= currP->offset+currP->len ) {
		        currP->next = NULL;
			delete []currP->buf;
			bufCount -= (sizeof(BufElem)+1) + currP->len;
			delete currP;
		    } else {
		        return OverlapR2( user_fd, buf, tempPos, tempRead, tempFetch, saveP, currP );
		    }
		}
		return OverlapR2( user_fd, buf, tempPos, tempRead, tempFetch, saveP, afterP );    
	    }
	}
	return InsertR( user_fd, buf, tempPos, tempRead, tempFetch, prevP, afterP );
}

int
OpenFileTable::setFlag( int user_fd, int flags )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[user_fd].flags = flags ;
	return 0;
}

int
OpenFileTable::setMode( int user_fd, mode_t mode )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	file[user_fd].mode = mode ;
	return 0;
}

char *
OpenFileTable::getServerName( int user_fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return NULL;
	}

	return strdup( file[user_fd].ioservername ) ;
}


int
OpenFileTable::getServerPort( int user_fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	return file[user_fd].ioserverport;
}


int
OpenFileTable::getSockFd( int user_fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	return file[user_fd].ioserversocket;
}


int
OpenFileTable::getRemoteFd( int user_fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	return file[user_fd].real_fd;
}


int
OpenFileTable::getOffset( int user_fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	return file[user_fd].offset;
}


char *
OpenFileTable::getFileName( int user_fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return NULL;
	}

	return strdup( file[user_fd].pathname ) ;
}


int
OpenFileTable::getFlag( int user_fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return -1;
	}

	return file[user_fd].flags;
}


mode_t
OpenFileTable::getMode( int user_fd )
{
        if( !file[user_fd].isOpen() ) {
		errno = EBADF;
		return 0;				// can't return -1, mode_t can be unsigned
	}

	return file[user_fd].mode  ;
}

//
// This function inserts a new write buffer block to the list. 
//
ssize_t
OpenFileTable::InsertW( int user_fd, const void *buf, int tempPos, int len, BufElem *prevP, BufElem *afterP )
{
        int rval;
        int real_fd;
	BufElem *newBuf;

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
		return -1;
	}                      
	 
	newBuf = new BufElem;
	if( !newBuf ) {
		dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
		Suicide();
	}
		
	if( !prevP ) {
	         newBuf->next = afterP;
		 file[user_fd].firstBuf = newBuf;
	} else {
	         prevP->next = newBuf;
		 newBuf->next = afterP;
	}		  

	newBuf->buf = new char[len+1];
	if( !newBuf->buf ) {
		dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
		Suicide();
	}
	newBuf->offset = tempPos;
	newBuf->len = len;
	bufCount += (sizeof(BufElem)+1) + len;      
	strncpy( newBuf->buf, ( char * ) buf, len );	    
	newBuf->buf[len] = '\0';

	return len;
}

//
// This function handles a write to a buffer like 
//     Write : -----        or -----       or -----
//     Buffer :       -----         -----       -----
// Merge in cases like  
//     Write : -----        or -----    
//     Buffer :     -----        -----
//
ssize_t
OpenFileTable::OverlapW1( int user_fd, const void *buf, int tempPos, int len, BufElem *prevP, BufElem *afterP )
{
        int rval;
        int real_fd;
	int tempWrite;                          // from the old buffer.

	if( afterP == NULL ) {
	        return -1;
	}

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
		return -1;
	}                      

	if( tempPos+len < afterP->offset ) {    // case I.
	        return InsertW( user_fd, buf, tempPos, len, prevP, afterP );
	} else {                                // case II & III.
	        if( !prevP ) {
		        file[user_fd].firstBuf = afterP;
		} else {
		        prevP->next = afterP;
		}

	        tempWrite = afterP->offset + afterP->len - tempPos - len;     
	     
		char *newbuf = new char[tempWrite+len+1];
		if( !newbuf ) {
			dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
			Suicide();
		}
		char *tempP = newbuf;
		strncpy( newbuf, ( char * )buf, len );
		tempP += len;
		strncpy( tempP, afterP->buf+afterP->len-tempWrite, tempWrite );
		newbuf[tempWrite+len] = '\0';
		bufCount += afterP->offset - tempPos;
		afterP->len = len + tempWrite;
		afterP->offset = tempPos;
		delete []afterP->buf;
		afterP->buf = newbuf;

		return len;
	}
}

//
// This function handles a write to a buffer like 
//     Write :        -----    or    -----    or    -----
//     Buffer :  -----             -----        -----   -----
// Merge the buffers
//
ssize_t
OpenFileTable::OverlapW2( int user_fd, const void *buf, int tempPos, int len, BufElem *prevP, BufElem *afterP )
{
        int rval;
        int real_fd;
	int tempWrite;                          // from old buffer.

	if( prevP == NULL ) {
	        return -1;
	}

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
		return -1;
	}                       
                                                // case I & case II
	if( !afterP || tempPos+len < afterP->offset ) {
	    tempWrite = tempPos - prevP->offset;

	    char *newbuf = new char[tempWrite+len+1];
	    if( !newbuf ) {
			dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
			Suicide();
	    }
	    char *tempP = newbuf;
	    strncpy( newbuf, prevP->buf, tempWrite );
	    tempP += tempWrite;
	    strncpy( tempP, ( char * ) buf, len ); 
	    newbuf[tempWrite+len] = '\0';
	    bufCount += tempWrite + len  - prevP->len;
	    prevP->len = tempWrite + len;
	    delete []prevP->buf;
	    prevP->buf = newbuf;
	    prevP->next = afterP;
        } else {                                // case III
	    char *newbuf = new char[afterP->offset+afterP->len-prevP->offset+1];
	    if( !newbuf ) {
		dprintf( D_ALWAYS, "failed to alloc new file buffer\n");
		Suicide();
	    }
	    char *tempP = newbuf;
	    strncpy( newbuf, prevP->buf, tempPos-prevP->offset );
	    tempP += tempPos-prevP->offset;
	    strncpy( tempP, ( char * ) buf, len ); 
	    tempP += len;
	    tempWrite = afterP->offset + afterP->len - tempPos - len; 
	    strncpy( tempP, afterP->buf+afterP->len-tempWrite, tempWrite ); 
	    newbuf[afterP->offset+afterP->len-prevP->offset] = '\0';
	    bufCount += afterP->offset - prevP->offset - prevP->len - (sizeof(BufElem)+1);      
    	    prevP->len = afterP->offset+afterP->len-prevP->offset;
	    delete []prevP->buf;
	    delete []afterP->buf;
	    prevP->buf = newbuf;
	    prevP->next = afterP->next;
	    delete afterP;
	}
	
	return len;
}

//
// Buffering for write-only files.
//
ssize_t
OpenFileTable::Buffer( int user_fd, const void *buf, size_t len )
{
        int tempPos;
	int rval;
	int real_fd;
	BufElem *prevP;
	BufElem *afterP;

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
                return -1;
        }
                                            
        tempPos = file[user_fd].getOffset();
        
	if( len > MAXBUF - (sizeof(BufElem)+1) ) {           // if too large, write directly
	    rval = REMOTE_syscall( CONDOR_lseekwrite, real_fd, tempPos, SEEK_SET, buf, len );
	    return rval;
	}

	if( len + bufCount > MAXBUF - (sizeof(BufElem)+1) ) {
	     FlushBuf();                    // if too full, flush the buffer.
	}

	if( !file[user_fd].firstBuf ) {     // write the 1st buffer block.
	    return InsertW( user_fd, buf, tempPos, len, NULL, NULL ); 
	}

	prevP = NULL;
	for( afterP=file[user_fd].firstBuf; afterP; afterP=afterP->next ) {
	    if( tempPos < afterP->offset ) {
	        if( tempPos+len-1 >= afterP->offset+afterP->len ) {
		    BufElem *currP;
		    while( 1 ) {
		        currP = afterP->next;
			afterP->next = NULL;
			delete []afterP->buf;
			bufCount -= (sizeof(BufElem)+1) + afterP->len;
			delete afterP;
			afterP = currP;
			if( !currP ) {
			    break;
			}
			if( tempPos+len-1 >= afterP->offset+afterP->len ) {
			    ;               // keep erasing old buffers.
			} else {
			    return OverlapW1( user_fd, buf, tempPos, len, prevP, afterP );
			}
		    }
		    return InsertW( user_fd, buf, tempPos, len, prevP, afterP );
		} else {
		    return OverlapW1( user_fd, buf, tempPos, len, prevP, afterP ); 
	        }                           // cache hits.
	    } else if( tempPos+len-1 < afterP->offset+afterP->len ) {	      
	        char *tempP = afterP->buf + ( tempPos - afterP->offset );
		strncpy( tempP, (char *) buf, len );
		return len;
	    } else if( tempPos > afterP->offset+afterP->len ) {
	        prevP = afterP;             // go to the next buffer block.
	    } else {
	        BufElem *currP;
		BufElem *saveP = afterP;
		afterP = afterP->next;
		while( 1 ) {
		    currP = afterP;
		    if( !currP ) {
		        break;
		    }
		    afterP = afterP->next;
		    if( tempPos+len >= currP->offset+currP->len ) {
		        currP->next = NULL;
			delete []currP->buf;
			bufCount -= (sizeof(BufElem)+1) + currP->len;
			delete currP;
		    } else {
		        return OverlapW2( user_fd, buf, tempPos, len, saveP, currP );
		    }
		}
		return OverlapW2( user_fd, buf, tempPos, len, saveP, afterP );
	    }
	}
	return InsertW( user_fd, buf, tempPos, len, prevP, afterP );       
}

//
// Buffers for both RDONLY and WRONLY (flushed first) files are released.
//
void 
OpenFileTable::FlushBuf()
{
        int real_fd;
	int rval;
        BufElem *tempBuf;

        for( int i=0; i<MaxOpenFiles; i++ ) {
	    if( file[i].firstBuf ) {
		if( file[i].isReadable() && !file[i].isWriteable() ) {
		    while( file[i].firstBuf ) {
			tempBuf = file[i].firstBuf;
			file[i].firstBuf = tempBuf->next;
			tempBuf->next = NULL;
			delete tempBuf;
		    }
		} else if( file[i].isWriteable() && ! file[i].isReadable() ) {
		    while( file[i].firstBuf ) {
		        tempBuf = file[i].firstBuf;
			file[i].firstBuf = tempBuf->next;
			tempBuf->next = NULL;
			if( ( real_fd=MapFd( i ) ) < 0 ) {
				dprintf(D_ALWAYS,
						"failed to MapFd in OpenFileTable::FlushBuf()!\n");
			}
			rval = REMOTE_syscall( CONDOR_lseekwrite, real_fd, tempBuf->offset, SEEK_SET, tempBuf->buf, tempBuf->len );
			if( rval != tempBuf->len ) {
				dprintf(D_ALWAYS,
						"lseekwrite failed in OpenFileTable::FlushBuf()!\n");
			}
			delete tempBuf;
		    }
		} else {
			dprintf(D_ALWAYS, "invalid case in OpenFileTable::FlushBuf()!\n");
		}
	    }
	}
	bufCount = 0;                       // buffer is empty now.
}

//
// Display the buffers for both RDONLY and WRONLY files.
// For debugging purposes only.
//
void 
OpenFileTable::DisplayBuf()
{
        int real_fd;
	int rval;
        BufElem *tempBuf;

	printf( "%s%d\n", "Total occupied buffer space is : ", bufCount );

        for( int i=0; i<MaxOpenFiles; i++ ) {
	    if( tempBuf = file[i].firstBuf ) {
	        if( file[i].isReadable() && !file[i].isWriteable() ) {
		    printf( "%s%d%s\n", "\nRDONLY file ", i, " : " );
		} else if( file[i].isWriteable() && ! file[i].isReadable() ) {
		    printf( "%s%d%s\n", "\nWRONLY file ", i, " : " );
		} else {
		    dprintf(D_ALWAYS, "invalid case in OpenFileTable::DisplayBuf()\n");
		}
		
		int j = 1;
		for( ; tempBuf; tempBuf=tempBuf->next ) {
		    printf( "%s%d\n", "    Buffer ", j );
		    printf( "%s%d\n", "        offset : ", tempBuf->offset );
		    printf( "%s%d\n", "        length : ", tempBuf->len );
		    printf( "%s%s\n", "        contents : ", tempBuf->buf );
		    j++;
		}
	    }
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

	if( ptr = (char *)strrchr((const char *)path,'/') ) {
		return ptr;
	} else {
		return path;
	}
}

extern "C" {

#if defined( BufferCondorIO )

#if defined( SYS_lseek )

off_t
lseek( int user_fd, off_t offset, int whence )
{
  
        int real_fd;
	off_t temp;

	if ( FileTab == NULL )
		InitFileState();

	errno = 0;

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
	        errno = EBADF;
                return (off_t)-1;
        }

	if( whence == SEEK_SET ) {
             	if( offset < 0 ) {
		        errno = EINVAL;
			return (off_t)-1;
		} else {
		        FileTab->SetOffset( user_fd, offset );
			return offset;
		}
	} else if( whence == SEEK_CUR ) {
	        temp = FileTab->GetOffset( user_fd )+offset;
		if( temp < 0 )  {
		        errno = EINVAL;
		        return (off_t)-1;
		} else {
		        FileTab->SetOffset( user_fd, temp );
			return temp;
		}
	} else if( whence == SEEK_END ) {
	        temp = FileTab->GetSize( user_fd )+offset;
		if( temp < 0 ) {
		        errno = EINVAL;
		        return (off_t)-1;
		} else {
		        FileTab->SetOffset( user_fd, temp );
			return temp;		       
		}
	} else {
		errno = EINVAL;
		return (off_t)-1;
	}       	
}

#if defined( SYS_read )

ssize_t
read( int user_fd, void *buf, size_t nbyte )
{
        int rval;
        int real_fd;
	off_t tempPos, tempSize;
	int use_local_access = FALSE;

	if ( FileTab == NULL )
		InitFileState();

	errno = 0;

	if( !FileTab->IsReadable( user_fd ) ) {
	        errno = EBADF;
		return (ssize_t)-1;
	}

        if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
                errno = EBADF;
                return (ssize_t)-1;
        } else if( ( int ) nbyte < 0 ) {
	        errno = EINVAL;
		return (ssize_t)-1;
	} else if ( nbyte == 0 ) {
	        return (ssize_t) 0;
	}

	if( FileTab->IsLocalAccess( user_fd ) ) {
		use_local_access = TRUE;
	}

	tempPos = FileTab->GetOffset( user_fd );
	tempSize = FileTab->GetSize( user_fd );

	if( LocalSysCalls() || use_local_access ) {
	        if( tempSize == 0 ) {
		        rval = syscall( SYS_lseek, real_fd, tempPos, SEEK_SET );
			if( rval < 0 && user_fd != 14 ) {
			        return (ssize_t)-1;
			}
			rval = syscall( SYS_read, real_fd, buf, nbyte );
			return rval;
		}

		if( tempPos >= tempSize ) {	     
		        return (ssize_t)0;
		} else if( tempPos+nbyte < tempSize ) {
		        rval = syscall( SYS_lseek, real_fd, tempPos, SEEK_SET );
			if( rval < 0 ) {
			        return (ssize_t)-1;
			}
			FileTab->SetOffset( user_fd, tempPos+nbyte );
			rval = syscall( SYS_read, real_fd, buf, nbyte );
			if( rval != nbyte ) {
			        return -1;
			}
		} else {
		        rval = syscall( SYS_lseek, real_fd, tempPos, SEEK_SET );
			if( rval < 0 ) {
			        return (ssize_t)-1;
			}
			FileTab->SetOffset( user_fd, tempSize );
			rval = syscall( SYS_read, real_fd, buf, nbyte );
			if( rval != tempSize - tempPos ) {
			        return -1;
			}
		}
	} else {
	        if( tempSize == 0 ) {
		        rval = REMOTE_syscall( CONDOR_lseekread, real_fd, tempPos, SEEK_SET, buf, nbyte );
			return rval;
		}

		if( tempPos >= tempSize ) {	     
		        return (ssize_t)0;
		} else if( tempPos+nbyte < tempSize ) {
		        if( !FileTab->IsWriteable( user_fd ) ) {
			 	rval = FileTab->PreFetch( user_fd, buf, nbyte );
			} else {
			        rval = REMOTE_syscall( CONDOR_lseekread, real_fd, tempPos, SEEK_SET, buf, nbyte );
		        }
			if( rval > 0 ) {
			        FileTab->SetOffset( user_fd, tempPos+nbyte );
			}
			if( rval != nbyte ) {
			        return -1;
			}
		} else {
		        if( !FileTab->IsWriteable( user_fd ) ) {
				rval = FileTab->PreFetch( user_fd, buf, nbyte );
			} else { 
			        rval = REMOTE_syscall( CONDOR_lseekread, real_fd, tempPos, SEEK_SET, buf, nbyte );
		        }
			if( rval > 0 ) {
			        FileTab->SetOffset( user_fd, tempSize );
			}
			if( rval != tempSize - tempPos ) {
			        return -1;
			}
		}
	}
	
	return rval;
}

#endif // #if defined( SYS_read )


#if defined( SYS_write )

ssize_t
write( int user_fd, const void *buf, size_t len )
{
        int rval;
        int real_fd;
	off_t tempPos, tempSize;
	int use_local_access = FALSE;

	if ( FileTab == NULL )
		InitFileState();

	errno = 0;
        
	if( !FileTab->IsWriteable( user_fd ) ) {
	        errno = EBADF;
		return (ssize_t)-1;
	}

	if( ( real_fd=MapFd( user_fd ) ) < 0 ) {
                errno = EBADF;
                return (ssize_t)-1;
        } else if( ( int ) len < 0 ) {
	        errno = EINVAL;
		return (ssize_t)-1;
	} else if ( len == 0 ) {
	        return (ssize_t) 0;
	}

	if( FileTab->IsLocalAccess( user_fd ) ) {
		use_local_access = TRUE;
	}

	tempPos = FileTab->GetOffset( user_fd );
	tempSize = FileTab->GetSize( user_fd );

	if( LocalSysCalls() || use_local_access ) {
	        rval = syscall( SYS_lseek, real_fd, tempPos, SEEK_SET );
		if( rval < 0 ) {
		        return (ssize_t)-1;
		}
		rval = syscall( SYS_write, real_fd, buf, len );
		if( rval != len ) {
		        return -1;
		}
		if( rval > 0 ) {
			FileTab->SetOffset( user_fd, tempPos+len );
			if( tempPos+len < tempSize ) {
                                ;
		        } else {
				FileTab->SetSize( user_fd, tempPos+len );
	        	}
                }
	} else {
	        if( !FileTab->IsReadable( user_fd ) ) {
		        rval = FileTab->Buffer( user_fd, buf, len );
		} else {
		        rval = REMOTE_syscall( CONDOR_lseekwrite, real_fd, tempPos, SEEK_SET, buf, len );
		}
                if( rval != len ) {
		        return -1;
		}
		if( rval > 0 ) {
			FileTab->SetOffset( user_fd, tempPos+len );
			if( tempPos+len < tempSize ) {
                                ;
		        } else {
				FileTab->SetSize( user_fd, tempPos+len );
	        	}
                }
        }

	return rval;
}

#endif // #if defined( SYS_write )

#endif // #if defined( SYS_lseek )

#endif // #if defined( BufferCondorIO )

#if defined( SYS_open )

int AvoidNFS;

#if 1
	void debug_msg( const char *msg );
#endif

int open_stream( const char *local_path, int flags, int *_FileStreamLen );
int _FileStreamWanted;
int _FileStreamLen;

int	_condor_open( const char *path, int flags, va_list ap )
{
	int		creat_mode = 0;
	char	local_path[ _POSIX_PATH_MAX ];	// pathname on this machine
	int		pipe_fd;	// fd number if this file is already open as a pipe
	int		fd, scm;
	int		status = IS_LOCAL;
	int		is_remote = FALSE;
	sigset_t omask;

	if ( FileTab == NULL )
		InitFileState();

	if( flags & O_CREAT ) {
		creat_mode = va_arg( ap, int );
	}

	/* we don't want to be interrupted by a checkpoint between when
	   we open this file and we mark it opened in our local data
	   structure */
	omask = block_condor_signals();
	if( LocalSysCalls() ) {
		fd = syscall( SYS_open, path, flags, creat_mode );
		strcpy( local_path, path );
	} else {
		status = REMOTE_syscall( CONDOR_file_info, path, &pipe_fd, local_path );
		if( status < 0 ) {
			dprintf( D_ALWAYS, "CONDOR_file_info failed\n" );
			Suicide();
		}
		switch( status ) {
		  case IS_PRE_OPEN:
			fd = pipe_fd;
			break;
		  case IS_IOSERVER:
//			dprintf(D_ALWAYS," Calling ioserver_open.. %s, %d, %d \n", local_path, flags, creat_mode);
			
			scm = SetSyscalls( SYS_LOCAL | SYS_MAPPED );
			fd = ioserver_open( local_path, flags, creat_mode ) ;
			
//			char str[10];
//			sprintf(str,"fd = %d\n",fd);
			
//			dprintf(D_ALWAYS,str);
			SetSyscalls( scm );
			
			break ;
		  case IS_NFS:
		  case IS_AFS:
		  case IS_LOCAL:
			fd = syscall( SYS_open, local_path, flags, creat_mode );
			if( fd >= 0 ) {
				break;
			} // else fall through, and try by remote syscalls anyway
		  case IS_RSC:
		  default:
			if( _FileStreamWanted ) {
				fd = open_stream( local_path, flags, &_FileStreamLen );
			} else {
				fd = REMOTE_syscall(CONDOR_open, local_path, flags, creat_mode);
			}
			is_remote = TRUE;
			break;
		}
	}

	Condor_isremote = is_remote;

	if( fd < 0 ) {
		restore_condor_sigmask(omask);
		return -1;
	}

	if( MappingFileDescriptors() && status != IS_IOSERVER) {
		fd = FileTab->DoOpen( local_path, flags, fd, is_remote );
	}

	restore_condor_sigmask(omask);
	return fd;
}
#endif

int
open_stream( const char *local_path, int flags, int *_FileStreamLen )
{
	EXCEPT( "Should never get here" );
}

int
open( const char *path, int flags, ... )
{
	va_list ap;
	int rval;
	va_start( ap, flags );
	rval = _condor_open( path, flags, ap );
	va_end( ap );
	return rval;
}


int
_open( const char *path, int flags, ... )
{
	va_list ap;
	int rval;
	va_start( ap, flags );
	rval = _condor_open( path, flags, ap );
	va_end( ap );
	return rval;
}

__open( const char *path, int flags, ... )
{
	va_list ap;
	int rval;
	va_start( ap, flags );
	rval = _condor_open( path, flags, ap );
	va_end( ap );
	return rval;
}

#if defined(OSF1)
/* Force isatty() to be undefined so programs that use it get it from
   the condor library rather than libc.a.
*/
int
not_used()
{
	return isatty(0);
}
#endif

#if defined(AIX32)
int
openx( const char *path, int flags, mode_t creat_mode, int ext )
{

	if ( FileTab == NULL )
		InitFileState();

	if( ext != 0 ) {
		errno = ENOSYS;
		return -1;
	}

	if( MappingFileDescriptors() ) {
		return FileTab->DoOpen( path, flags, creat_mode, ext );
	} else {
		if( LocalSysCalls() ) {
			return syscall( SYS_openx, path, flags, creat_mode, ext );
		} else {
			return REMOTE_syscall( CONDOR_openx, path, flags, creat_mode, ext );
		}
	}
}
#endif

#if defined( SYS_close )
int
close( int fd )
{
	if ( FileTab == NULL )
		InitFileState();

  if ( !LocalSysCalls() && MappingFileDescriptors() && IOServerAccess(fd)) 
    {
      int scm = SetSyscalls(SYS_LOCAL | SYS_MAPPED);
      int rval=ioserver_close(fd);
      SetSyscalls(scm);
      return rval;
    }
  
        
	if( MappingFileDescriptors() ) {
		int rval = FileTab->DoClose( fd );
		// In standalone mode, this fd might be a socket or pipe which
		// we didn't catch.  Allow the application to close it
		// successfully, to allow sockets and pipes to be used
		// successfully between checkpoints.
		if (rval < 0 && MyImage.GetMode() == STANDALONE) {
			return syscall( SYS_close, fd );
		}
		return rval;
	} else {
		if( LocalSysCalls() ) {
			return syscall( SYS_close, fd );
		} else {
			return REMOTE_syscall( CONDOR_close, fd );
		}
	}
}

/* these definitions of _close and __close match definitions of
   _open and __open above */

int
_close( int fd )
{
	return close( fd );
}
int
__close( int fd )
{
	return close( fd );
}

#endif

#if defined(SYS_dup)
int
dup( int old )
{
	sigset_t omask;
	int rval;

	if ( FileTab == NULL )
		InitFileState();

	/* we don't want to be interrupted by a checkpoint between when
	   modify our local file table and actually make the change */
	omask = block_condor_signals();
	if( MappingFileDescriptors() ) {
		rval = FileTab->DoDup( old );
		restore_condor_sigmask(omask);
		// In standalone mode, this fd might be a socket or pipe which
		// we didn't catch.  Allow the application to dup it
		// successfully, to allow sockets and pipes to be used
		// successfully between checkpoints.
		if (rval < 0 && MyImage.GetMode() == STANDALONE) {
			return syscall( SYS_dup, old );
		}
		return rval;
	}

	if( LocalSysCalls() ) {
		rval = syscall( SYS_dup, old );
		restore_condor_sigmask(omask);
		return rval;
	} else {
		rval = REMOTE_syscall( CONDOR_dup, old );
		restore_condor_sigmask(omask);
		return rval;
	}
}
#endif

#if defined(SYS_dup2)
int
dup2( int old, int new_fd )
{
	int		rval;

	if ( FileTab == NULL )
		InitFileState();

	if( MappingFileDescriptors() ) {
		rval =  FileTab->DoDup2( old, new_fd );
		// In STANDALONE mode, this must make an actual dup of the fd.
		// We do this even if DoDup2 failed above, to allow the application
		// to use fds which we're not tracking between checkpoints.
		if (MyImage.GetMode() == STANDALONE)
			rval = syscall( SYS_dup2, old, new_fd );
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

#if defined(SYS_socket) || (defined(LINUX) && defined(SYS_socketcall))
int
socket( int addr_family, int type, int protocol )
{
	int		rval;
#if defined(LINUX)
	unsigned long	socket_args[4] = {
		(unsigned long)addr_family,
		(unsigned long)type,
		(unsigned long)protocol
	};
#endif

	if ( FileTab == NULL )
		InitFileState();

	if( MappingFileDescriptors() ) {
		rval =  FileTab->DoSocket( addr_family, type, protocol );
		return rval;
	}

	if( LocalSysCalls() ) {
#if defined(LINUX)
		// Linux combines syscalls such as socket, accept, recv, send, ...
		// into a single system call - SYS_socketcall.  Parameters are
		// passed through the third argument to syscall, in this case
		// socket_args.  The type of call (socket, accept,...) is given
		// as the second argument.  The valid types are listed in
		// /usr/include/sys/socketcall.h  The kernel source warns that
		// this interface may change in the future so that this
		// code may need adjustment! - Greger
		rval =  syscall( SYS_socketcall, SYS_SOCKET, socket_args );
#else
		rval =  syscall( SYS_socket, addr_family, type, protocol );
#endif
	} else {
		rval =  REMOTE_syscall( CONDOR_socket, addr_family, type, protocol );
	}

	return rval;
}
#endif /* SYS_socket */

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
InitFileState()
{
	FileTab = new OpenFileTable();
	FileTab->Init();
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
IOServerAccess( int user_fd )
{
  return FileTab->isIOServerAccess( user_fd );
}

int
pre_open( int fd, BOOL readable, BOOL writeable, BOOL shadow_connection )
{
	return FileTab->PreOpen( fd, readable, writeable, shadow_connection );
}

int
MarkOpen( const char *path, int flags, int real_fd, int is_remote )
{
	return FileTab->DoOpen( path, flags, real_fd, is_remote );
}

int creat(const char *path, mode_t mode)
{
	return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

#if defined (SYS_fcntl)
int
_condor_fcntl( int fd, int cmd, va_list ap )
{
	int arg = 0;
	int user_fd;
	int use_local_access = FALSE;
	int rval;
	struct flock *lockarg;
#if HAS_64BIT_SYSCALLS
	struct flock64 *lock64arg;
#endif
	
	if ( FileTab == NULL )
		InitFileState();
	
	if ((user_fd = MapFd(fd)) < 0) {
		// allow fds we don't have in our table in STANDALONE mode
		if (MyImage.GetMode() == STANDALONE) {
			user_fd = fd;
		} else {
			return -1;
		}
	}
	
	if ( LocalAccess(fd) || MyImage.GetMode() == STANDALONE ) {
		use_local_access = TRUE;
	}
	
	switch (cmd) {
	case F_DUPFD:
#if HAS_F_DUP2FD
	case F_DUP2FD:
#endif

// Linux uses a long as the type for the third argument.  All other
// platforms use an int.  For Linux, we just cast the long to an int
// for use with our remote syscall.  Derek Wright 4/11/97
#if defined(LINUX)
		arg = (int) va_arg( ap, long );
#else
		arg = va_arg( ap, int );
#endif

		if( MappingFileDescriptors() ) {
			rval =  FileTab->DoDup2( fd, arg );
				// In STANDALONE mode, this must make an actual dup of the
				// fd.  We do this even if DoDup2 failed above, to allow
				// the application to use fds which we're not tracking
				// between checkpoints.
			if (MyImage.GetMode() == STANDALONE) {
				rval = syscall( SYS_fcntl, user_fd, cmd, arg );
			}
			return rval;
		}

		if ( LocalSysCalls() || use_local_access ) {
			return syscall( SYS_fcntl, user_fd, cmd, arg );
		} else {
				// In remote mode, we want to send a CONDOR_dup2 on
				// the wire, not an fcntl(), so we have a prayer of
				// heterogeneous syscalls working.  -Derek W. and Jim
				// B. 8/18/98
			return REMOTE_syscall( CONDOR_dup2, user_fd, arg );
		}
	case F_GETFD:
	case F_GETFL:
		if ( LocalSysCalls() || use_local_access ) {
			return syscall( SYS_fcntl, user_fd, cmd, arg );
		} else {
			return REMOTE_syscall( CONDOR_fcntl,
								   user_fd, cmd, arg );
		}
	case F_SETFD:
	case F_SETFL:
#if defined(LINUX)
		arg = (int) va_arg( ap, long );
#else
		arg = va_arg( ap, int );
#endif
		if ( LocalSysCalls() || use_local_access ) {
			return syscall( SYS_fcntl, user_fd, cmd, arg );
		} else {
			return REMOTE_syscall( CONDOR_fcntl, user_fd, cmd, arg );
		}	

	// These fcntl commands use a struct flock pointer for their
	// 3rd arg.  Supporting these as remote calls would require a
	// pseudo syscall for the seperate argument signature.  
	// Currently, this is not supported.  Derek Wright 4/11/97
	case F_GETLK:
	case F_SETLK: 
	case F_SETLKW: 
		lockarg = va_arg ( ap, struct flock* );
		if ( LocalSysCalls() || use_local_access ) {
			return syscall( SYS_fcntl, user_fd, cmd, lockarg );
		} else {
			dprintf( D_ALWAYS, "Unsupported fcntl() command %d\n", cmd );
			return -1;
		}
	// If we have 64 bit syscalls/structs, we'll have some flock64
	// versions of these, as well.  -Derek W. 8/18/98
#if HAS_64BIT_SYSCALLS
	case F_GETLK64:
	case F_SETLK64: 
	case F_SETLKW64: 
		lock64arg = va_arg ( ap, struct flock64* );
		if ( LocalSysCalls() || use_local_access ) {
			return syscall( SYS_fcntl, user_fd, cmd, lock64arg );
		} else {
			dprintf( D_ALWAYS, "Unsupported fcntl() command %d\n", cmd );
			return -1;
		}
#endif /* HAS_64BIT_SYSCALLS */
#if defined( F_FREESP ) 
	case F_FREESP:
#if defined( F_FREESP64 )
	case F_FREESP64:
#endif
		lockarg = va_arg ( ap, struct flock* );
		if ( LocalSysCalls() || use_local_access ) {
			return syscall( SYS_fcntl, user_fd, cmd, lockarg );
		} else {
			if( lockarg->l_whence == 0 && 
				lockarg->l_start == 0 &&
				lockarg->l_len == 0 ) {
					// This is the same as ftruncate(), and we'll trap
					// ftruncate() and send that on the wire,
					// instead.  -Derek Wright 10/14/98
				return ftruncate( user_fd, 0 );
			} else {
				dprintf( D_ALWAYS, "Unsupported fcntl() command %d\n", cmd );
				return -1;
			}
		}
#endif /* defined( F_FREESP ) */

	default:
		dprintf( D_ALWAYS, "Unsupported fcntl() command %d\n", cmd );
	}
	return -1;
}


/* _fcntl and __fcntl must have the same cases as fcntl so the correct
   third argument gets passed.  -Jim B. */

/* To avoid totally nasty code duplication, we can just use a va_list
   to pass the variable args for _fcntl() and __fcntl() to our real
   fcntl() function above.  -Derek W. 8/18/98 */

/* Actually, you can't just setup a va_list and pass it as an arg to a
   function expecting "...", since it doesn't work like you think.
   Inside the function expecting "...", when you do a "va_arg(ap,
   int)", for example, you get garbage, since there's no int in the
   arg list, there's just a va_list.

   Instead, you need to define a function that takes a va_list
   explicitly as it's last argument, and have stub functions that take
   "..." and setup the va_list themselves.  So, we define
   _condor_fcntl() to take a va_list, and define stubs for (_*)fcntl()
   which are all identical: they just put a va_list on their stack,
   call va_start() to set it up, call _condor_fcntl passing this
   va_list, when that returns, they clean up the va_list by calling
   va_end(), and finally return what _condor_fcntl() returned.

   Inside _condor_fcntl, we never call va_start (since the va_list has
   already been setup), we just call va_arg() whenever we think we need
   to.  _condor_open() works the same way, with (_*)open() and
   (_*)open64() all looking almost identical to fcntl() below.

   -Derek W. 8/20/98 */

int
fcntl(int fd, int cmd, ...)
{
	int rval;
	va_list ap;
	va_start( ap, cmd );
	rval = _condor_fcntl( fd, cmd, ap );
	va_end( ap );
	return rval;
}


int
_fcntl(int fd, int cmd, ...)
{
	int rval;
	va_list ap;
	va_start( ap, cmd );
	rval = _condor_fcntl( fd, cmd, ap );
	va_end( ap );
	return rval;
}


int
__fcntl(int fd, int cmd, ...)
{
	int rval;
	va_list ap;
	va_start( ap, cmd );
	rval = _condor_fcntl( fd, cmd, ap );
	va_end( ap );
	return rval;
}

#endif /* SYS_fcntl */

} // end of extern "C"

