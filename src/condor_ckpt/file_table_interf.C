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

#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "condor_debug.h"

#include "file_table_interf.h"
#include "file_state.h"

#include <errno.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>

// XXX What are the header files for these?

extern "C" sigset_t block_condor_signals(void);
extern "C" void restore_condor_sigmask(sigset_t omask);
extern "C" int syscall( int kind, ... );

extern "C" {

/*
These are the switches for syscalls dealing with the open file table.
They should be moved back into syscall.tmpl, but there are
several obstacles:
	1 - adjustments to stub gen needed (easy)
	2 - switches.c needs to become switches.C (harder than it sounds)
	3 - These functions are required by libckpt when 
	    switches.o is not linked in.
*/

#define FILE_TABLE_SWITCH( rtype, funcname , callname , params , args ) \
rtype funcname ( params )\
{\
	int rval;\
\
	errno = 0;\
\
	if( MappingFileDescriptors() ) {\
		sigset_t sigs = block_condor_signals();\
		InitFileState();\
		rval =  FileTab-> callname ( args );\
		restore_condor_sigmask(sigs);\
	} else {\
		if( LocalSysCalls() ) {\
			rval =  syscall( SYS_##callname , args );\
		} else {\
			rval =  REMOTE_syscall( CONDOR_##callname , args );\
		}\
	}\
\
	return rval;\
}

#define X ,

FILE_TABLE_SWITCH( int, _condor_open, open, const char *path X int flags X int mode, path X flags X mode )

FILE_TABLE_SWITCH( int, _condor_close, close, int fd, fd )

FILE_TABLE_SWITCH( ssize_t, read, read, int user_fd X void *buf X size_t nbyte, user_fd X buf X nbyte )

FILE_TABLE_SWITCH( ssize_t, write, write, int user_fd X const void *buf X size_t nbyte, user_fd X buf X nbyte )

#undef lseek

FILE_TABLE_SWITCH( off_t, lseek, lseek, int fd X off_t offset X int whence, fd X offset X whence )

FILE_TABLE_SWITCH( offset_t, llseek, lseek, int fd X offset_t offset X int whence, fd X offset X whence )

FILE_TABLE_SWITCH( long long , lseek64, lseek, int fd X long long offset X int whence, fd X offset X whence )

#ifdef SYS_dup
FILE_TABLE_SWITCH( int, dup, dup, int fd, fd )
#endif

#ifdef SYS_dup2
FILE_TABLE_SWITCH( int, dup2, dup2, int fd X int nfd, fd X nfd)
#else
int dup2( int fd, int new_fd )
{
	int rval;
	errno = 0;
	if( MappingFileDescriptors() ) {
		sigset_t sigs = block_condor_signals();
		InitFileState();
		rval =  FileTab->dup2(fd,new_fd);
		restore_condor_sigmask(sigs);
	} else {
		if( LocalSysCalls() ) {
			rval = syscall(SYS_fcntl,fd,F_DUPFD,new_fd);
		} else {
			rval = REMOTE_syscall(CONDOR_fcntl,fd,F_DUPFD,new_fd);
		}
	}

	return rval;
}
#endif

FILE_TABLE_SWITCH( int, fchdir, fchdir, int fd, fd )

FILE_TABLE_SWITCH( int, fstat, fstat, int fd X struct stat *buf, fd X buf)

FILE_TABLE_SWITCH( int, _condor_fcntl, fcntl, int fd X int cmd X int arg, fd X cmd X arg)

FILE_TABLE_SWITCH( int, _condor_ioctl, ioctl, int fd X int cmd X int arg, fd X cmd X arg)

#ifdef SYS_flock
FILE_TABLE_SWITCH( int, flock, flock, int fd X int op, fd X op)
#endif

#ifdef SYS_fstatfs
FILE_TABLE_SWITCH( int, fstatfs, fstatfs, int fd X struct statfs *buf, fd X buf )
#endif

FILE_TABLE_SWITCH( int, fchown, fchown, int fd X uid_t owner X gid_t group, fd X owner X group)

FILE_TABLE_SWITCH( int, fchmod, fchmod, int fd X mode_t mode, fd  X mode)

#ifdef SYS_ftruncate
FILE_TABLE_SWITCH( int, ftruncate, ftruncate, int fd X size_t length, fd X length )
#endif

#ifdef SYS_fsync
FILE_TABLE_SWITCH( int, fsync, fsync, int fd, fd )
#endif

/*
This function is going the way of the dinosaur.  Operations on fds
need to pass through the file table so's we can redirect them to the
appropriate agent on a per-file basis.  An fd will not always map
to another fd in the same medium.

However, for system calls that are not routed through the open
file table, we'll provide this for now...
*/

int MapFd( int user_fd )
{
	if( MappingFileDescriptors() ) {
		return FileTab->map_fd_hack(user_fd);
	} else {
		return user_fd;
	}
}

int LocalAccess( int user_fd )
{
	if( MappingFileDescriptors() ) {
		return FileTab->local_access_hack(user_fd);
	} else {
		return LocalSysCalls();
	}
}

void DumpOpenFds()
{
	InitFileState();
	FileTab->dump();
}

void CheckpointFileState()
{
	InitFileState();
	FileTab->checkpoint();
}

void SuspendFileState()
{
	InitFileState();
	FileTab->suspend();
}

void ResumeFileState()
{
	InitFileState();
	FileTab->resume();
}

void InitFileState()
{
	if(!FileTab) {
		FileTab = new OpenFileTable();
		FileTab->init();
	}
}

int pre_open( int fd, int readable, int writable, int is_remote )
{
	InitFileState();
	sigset_t sigs = block_condor_signals();
	int result = FileTab->pre_open(fd,readable,writable,is_remote);
	restore_condor_sigmask(sigs);
}

int creat(const char *path, mode_t mode)
{
	return _condor_open((char*)path, O_WRONLY | O_CREAT | O_TRUNC, mode);
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


int
open( const char *path, int flags, ... )
{
	va_list ap;
	int rval,mode;
	va_start( ap, flags );
	mode = va_arg( ap, int );
	rval = _condor_open( path, flags, mode );
	va_end( ap );
	return rval;
}


int
_open( const char *path, int flags, ... )
{
	va_list ap;
	int rval,mode;
	va_start( ap, flags );
	mode = va_arg( ap, int );
	rval = _condor_open( path, flags, mode );
	va_end( ap );
	return rval;
}

__open( const char *path, int flags, ... )
{
	va_list ap;
	int rval,mode;
	va_start( ap, flags );
	mode = va_arg( ap, int );
	rval = _condor_open( path, flags, mode );
	va_end( ap );
	return rval;
}

int close( int fd )
{
	return _condor_close(fd);
}

int _close( int fd )
{
	return _condor_close(fd);
}

int __close( int fd )
{
	return _condor_close(fd);
}

int ioctl( int fd, int cmd, ... )
{
	va_list ap;
	int rval, arg;
	va_start(ap,cmd);
	arg = va_arg(ap, int );
	rval = _condor_ioctl( fd, cmd, arg );
	va_end(ap);
	return rval;
}

int fcntl( int fd, int cmd, ... )
{
	va_list ap;
	int rval, arg;
	va_start(ap,cmd);
	arg = va_arg(ap, int );
	rval = _condor_fcntl( fd, cmd, arg );
	va_end(ap);
	return rval;
}


} // extern "C"
