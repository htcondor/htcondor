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

#include "image.h"

#include "condor_common.h"
#include "condor_syscalls.h"
#include "condor_sys.h"
#include "condor_syscall_mode.h"

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

#include "file_state.h"
#include "file_table_interf.h"
#include "condor_file.h"
#include "condor_file_local.h"
#include "condor_file_remote.h"
#include "condor_file_special.h"
#include "condor_file_agent.h"
#include "condor_file_buffer.h"
#include "condor_error.h"

#include <stdarg.h>
#include <sys/errno.h>

// XXX Where is the header for this?
extern int errno;

CondorFileTable *FileTab=0;

/**
This class records statistics about how a file is used.
It is kept around even after a file is closed so that it may be
re-used and updated for multiple opens.  The class definition
is placed here because the implementation is entirely private
to this module.
*/

class CondorFileInfo {
public:
	CondorFileInfo( char *k, char *n ) {
		strcpy(kind,k);
		strcpy(name,n);
		open_count = 0;
		read_count = read_bytes = 0;
		write_count = write_bytes = 0;
		seek_count = 0;
		already_warned = 0;
		next = 0;
	}

	void	report() {
		if( RemoteSysCalls() ) {
			REMOTE_syscall( CONDOR_report_file_info, kind, name, open_count, read_count, write_count, seek_count, read_bytes, write_bytes );
		}
	}

	char	kind[_POSIX_PATH_MAX];
	char	name[_POSIX_PATH_MAX];
	int	open_count;
	int	read_count, read_bytes;
	int	write_count, write_bytes;
	int	seek_count;
	int	already_warned;

	CondorFileInfo	*next;
};

/**
This class maintains a seek pointer which sits between
a file descriptor and a file implementation.  Several file
descriptors may share one file pointer.  It also associates
a File object with a FileInfo object.  This class is
a helper for CondorFileTable.  The class definition is placed
here because the implementation is entirely private to this module.
*/

class CondorFilePointer {
public:
	CondorFilePointer( CondorFile *f, CondorFileInfo *i ) {
		file = f;
		info = i;
		offset = 0;
	}

	CondorFile *file;
	CondorFileInfo *info;
	off_t offset;
};

/*
This is a little tricky.

When a progam exists, the last thing the startup code does is flush
any (stdio or iostream) buffered files and close them. If any files are
(Condor) buffered, this data will sit in the (Condor) buffer until file
is closed, whereupon it will be flushed to the shadow.

However, files don't always get closed nicely, so we don't know exactly
when to flush them.  There is no mechanism that ensures Condor gets the
last laugh at exit time, so we will have atexit() warn us when an
exit is imminent.  This will cause a flag to be set in CondorFileBuffer
that instructs any further writes to be flushed immediately to disk.
*/

static void _condor_disable_buffering()
{
	FileTab->flush();
	FileTab->set_flush_mode(1);	
}

/*
This init function could be called from just about anywhere, perhaps
even when there is no shadow to talk to.  We will delay initializing
a buffer until we do the first remote open.  Each of the standard file 
descriptors will be connected to their local analogues until further
notice.
*/

void CondorFileTable::init()
{
	resume_count = 0;
	got_buffer_info = 0;
	buffer_size = 0;
	buffer_block_size = 0;
	flush_mode = 0;

	int scm = SetSyscalls( SYS_UNMAPPED | SYS_LOCAL );
	length = sysconf(_SC_OPEN_MAX);
	SetSyscalls(scm);

	pointers = new (CondorFilePointer *)[length];
	if(!pointers) {
		EXCEPT("Condor Error: CondorFileTable: Out of memory!\n");
		Suicide();
	}

	for( int i=0; i<length; i++ ) {
		pointers[i]=0;
	}

	info_head = 0;

	CondorFileInfo *info;

	info = find_info("stream","stdin");
	pointers[0] = new CondorFilePointer(new CondorFileLocal,info);
	pointers[0]->file->force_open( 0, "stdin", 1, 0 );

	info = find_info("stream","stdout");
	pointers[1] = new CondorFilePointer(new CondorFileLocal,info);
	pointers[1]->file->force_open( 1, "stdout", 0, 1 );

	info = find_info("stream","stderr");
	pointers[2] = new CondorFilePointer(new CondorFileLocal,info);
	pointers[2]->file->force_open( 2, "stderr", 0, 1 );

	atexit( _condor_disable_buffering );
}

void CondorFileTable::flush()
{
	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			pointers[i]->file->fsync();
			pointers[i]->info->report();
		}
	}
}

void CondorFileTable::set_flush_mode( int on_off )
{
	flush_mode = on_off;
}

void CondorFileTable::dump()
{
	dprintf(D_ALWAYS,"\nOPEN FILE TABLE:\n");

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			dprintf(D_ALWAYS,"fd: %d offset: %d dups: %d ", i, pointers[i]->offset, count_pointer_uses(pointers[i]));
			pointers[i]->file->dump();
		}
		dprintf(D_ALWAYS,"\n");
	}
}

void CondorFileTable::close_all()
{
	for( int i=0; i<length; i++ )
		close(i);
}

int CondorFileTable::find_name( const char *name )
{
	for( int fd=0; fd<length; fd++ ) {
		if( pointers[fd] && !strcmp(pointers[fd]->file->get_name(),name) ) {
			return fd;
		}
	}
	return -1;
}

CondorFileInfo * CondorFileTable::find_info( char *kind, char *name )
{
	CondorFileInfo *i;

	for( i=info_head; i; i=i->next ) {
		if(!strcmp(name,i->name) && !strcmp(kind,i->kind) ) {
			return i;
		}
	}
	
	i = new CondorFileInfo(kind,name);
	i->next = info_head;
	info_head = i;

	return i;
}

int CondorFileTable::find_empty()
{
	for( int fd=0; fd<length; fd++ )
		if( !pointers[fd] )
			return fd;

	return -1;
}

void CondorFileTable::replace_file( CondorFile *oldfile, CondorFile *newfile )
{
	for( int fd=0; fd<length; fd++ )
		if(pointers[fd] && (pointers[fd]->file==oldfile))
			pointers[fd]->file==newfile;
}

int CondorFileTable::count_pointer_uses( CondorFilePointer *p )
{
	int count=0;

	for( int fd=0; fd<length; fd++ )
		if(pointers[fd]==p)
			count++;	

	return count;
}

int CondorFileTable::count_file_uses( CondorFile *f )
{
	int count=0;

	for( int fd=0; fd<length; fd++ )
		if(pointers[fd] && (pointers[fd]->file==f))
			count++;	

	return count;
}

int CondorFileTable::open( const char *logical_name, int flags, int mode )
{
	int	fd, real_fd;
	char	full_path[_POSIX_PATH_MAX];
	char	url[_POSIX_PATH_MAX];
	char	method[_POSIX_PATH_MAX];

	CondorFile	*f;

	// Decide how to access the file.
	// In local syscalls, always use local.
	// In remote, ask the shadow for a complete url.

	if( LocalSysCalls() ) {
		strcpy(method,"local");
		strcpy(full_path,logical_name);
	} else {
		REMOTE_syscall( CONDOR_get_file_info, logical_name, url );
		sscanf(url,"%[^:]:%s",method,full_path);
		if(!got_buffer_info) {
			REMOTE_syscall( CONDOR_get_buffer_info, &buffer_size, &buffer_block_size );
			got_buffer_info = 1;
		}

	}

	// If a file by this name is already open, share the file object.
	// Otherwise, create a new one according to the given method.

	int match = find_name(full_path);
	if(match>=0) {
		f = pointers[match]->file;
	} else {
		if(!strcmp(method,"local")) {
			f = new CondorFileLocal;
		} else if(!strcmp(method,"remote")) {
			if( fd==2 || !buffer_size ) {
				f = new CondorFileRemote();
			} else {
				f = new CondorFileBuffer(new CondorFileRemote);
			}
		} else if(!strcmp(method,"remotefetch")) {
		 	f = new CondorFileAgent(new CondorFileRemote);
		} else {
			_condor_warning("I don't know how to access file '%s'",url);
			errno = ENOENT;
			return -1;
		}

		real_fd = f->open(full_path,flags,mode);
		if( real_fd<0 ) {
			delete f;
			return -1;
		}
	}

	// Standalone checkpointing requires that the virtual
	// fd match the real fd.  Full remote syscalls can
	// have any arbitrary mapping.

	if( MyImage.GetMode()==STANDALONE ) {
		fd = real_fd;
	} else {
		fd = find_empty();
		if(fd<0) {
			errno = EMFILE;
			f->close();
			delete f;
			return -1;
		}
	}

	// Look up the info block for this file.
	// Allocate a new one if needed.

	CondorFileInfo *info = find_info( f->get_kind(), f->get_name() );
	info->open_count++;

	// A file may not be opened RDWR, not may it be opened once for
	// reading and later for writing, or vice-versa.

	if(	!info->already_warned
		&&
		(
			(flags&O_RDWR) ||
			( (flags&O_WRONLY) && (info->read_bytes) ) ||
			( (flags&O_RDONLY) && (info->write_bytes) )
		)
	) {
		_condor_warning("File '%s' for both reading and writing.  This is not safe in a program that may be checkpointed\n",logical_name);
		info->already_warned = 1;
	}

	// Finally, install the pointer and return!

	pointers[fd] = new CondorFilePointer(f,info);
	return fd;
}

/* Install a CondorFileSpecial.  Use the real fd if possible. */

int CondorFileTable::install_special( char * kind, int real_fd )
{
	int fd;

	CondorFileSpecial *f = new CondorFileSpecial(kind);
	if(!f) {
		errno = ENOMEM;
		return -1;
	}

	CondorFileInfo *info = find_info( f->get_kind(), f->get_name() );
	info->open_count++;

	CondorFilePointer *fp = new CondorFilePointer(f,info);
	if(!fp) {
		delete f;
		errno = ENOMEM;
		return -1;
	}

	if( pointers[real_fd] ) {
		fd = find_empty();
	} else {
		fd = real_fd;
	}

	pointers[fd] = fp;
	pointers[fd]->file->force_open(real_fd,"",1,1);

	return fd;
}
/*
pipe() works by invoking a local pipe, and then installing a
CondorFileSpecial on those two fds.  A CondorFileSpecial is just like 
a local file, but checkpointing is prohibited while it exists.
*/

int CondorFileTable::pipe(int fds[])
{
	int real_fds[2];

	int scm = SetSyscalls( SYS_LOCAL|SYS_UNMAPPED );
	int result = pipe(real_fds);
	SetSyscalls(scm);

	if(result<0) return -1;

	fds[0] = install_special("pipe zero",real_fds[0]);
	fds[1] = install_special("pipe one",real_fds[1]);

	if( fds[0]<0 || fds[1]<0 ) return -1;

	return 0;
}

/*
Socket is similar to pipe.  We will perform a local socket(), and then
install a CondorFileSpecial on that fd to access it locally and inhibit
checkpointing in the meantime.
*/

int CondorFileTable::socket( int domain, int type, int protocol )
{
	int scm = SetSyscalls( SYS_LOCAL|SYS_UNMAPPED ); 
	int real_fd = ::socket(domain,type,protocol);
	SetSyscalls(scm);

	if(real_fd<0) return -1;

	return install_special("network connection", real_fd);
}

/*
Close is a little tricky.
The file pointer might be in use by several dups,
or the file itself might be in use by several opens.

So, count all uses of the file.  If there is only one,
close and delete.  Same goes for the file pointer.
*/

int CondorFileTable::close( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	CondorFilePointer *pointer = pointers[fd];
	CondorFile *file = pointers[fd]->file;

	// If this is the last use of the file, flush, close, and delete
	if(count_file_uses(file)==1) {
		pointers[fd]->info->report();
		file->close();
		delete file;
	}

	// If this is the last use of the pointer, delete it
	if(count_pointer_uses(pointer)==1) {
		delete pointer;
	}

	// In any case, mark the fd as unused.
	pointers[fd]=0;

	return 0;
}

ssize_t CondorFileTable::read( int fd, void *data, size_t nbyte )
{
	if( (fd>=length) || (fd<0) || (pointers[fd]==0) || (!pointers[fd]->file->is_readable()) ) {
		errno = EBADF;
		return -1;
	}
	
	if( (!data) || (nbyte<0) ) {
		errno = EINVAL;
		return -1;
	}

	CondorFilePointer *fp = pointers[fd];
	CondorFile *f = fp->file;

	if( nbyte==0 ) return 0;

	// get the data from the object
	int actual = f->read( fp->offset, (char*) data, nbyte );

	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Update the offset
	fp->offset += actual;

	// Record that a read was done
	fp->info->read_count++;
	fp->info->read_bytes+=actual;

	// Return the number of bytes read
	return actual;
}

ssize_t CondorFileTable::write( int fd, const void *data, size_t nbyte )
{
	if( (fd>=length) || (fd<0) || (pointers[fd]==0) || (!pointers[fd]->file->is_writeable()) ) {
		errno = EBADF;
		return -1;
	}

	if( (!data) || (nbyte<0) ) {
		errno = EINVAL;
		return -1;
	}

	if( nbyte==0 ) return 0;

	CondorFilePointer *fp = pointers[fd];
	CondorFile *f = fp->file;

	// Write to the object at the current offset
	int actual = f->write( fp->offset, (char *) data, nbyte );

	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Update the offset by the amount written
	fp->offset += actual;

	// Record that a write was done
	fp->info->write_count++;
	fp->info->write_bytes+=actual;

	// If flush_mode is enabled, flush it to disk
	if(flush_mode) f->fsync();

	// Return the number of bytes written
	return actual;
}

off_t CondorFileTable::lseek( int fd, off_t offset, int whence )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	CondorFilePointer *fp = pointers[fd];
	CondorFile *f = fp->file;
	int temp;

	// Compute the new offset first.
	// If the new offset is illegal, don't change it.

	if( whence == SEEK_SET ) {
		temp = offset;
	} else if( whence == SEEK_CUR ) {
	        temp = fp->offset+offset;
	} else if( whence == SEEK_END ) {
		temp = f->get_size()+offset;
	} else {
		errno = EINVAL;
		return -1;
	}

	if(temp<0) {
		errno = EINVAL;
		return -1;
	} else {
		fp->offset = temp;
		fp->info->seek_count++;
		return temp;
	}
}

/* This function does a local dup2 one way or another. */
static int _condor_internal_dup2( int oldfd, int newfd )
{
	#if defined(SYS_dup2)
		return syscall( SYS_dup2, oldfd, newfd );
	#elif defined(SYS_fcntl) && defined(F_DUP2FD)
		return syscall( SYS_fcntl, oldfd, F_DUP2FD, newfd );
	#elif defined(SYS_fcntl) && defined(F_DUPFD)
		syscall( SYS_close, newfd );
		return syscall( SYS_fcntl, oldfd, F_DUPFD, newfd );
	#else
		#error "_condor_internal_dup2: I need SYS_dup2 or F_DUP2FD or F_DUPFD!"
	#endif
}

int CondorFileTable::dup( int fd )
{
	return search_dup2( fd, 0 );
}

int CondorFileTable::search_dup2( int fd, int search )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) || (search<0) || (search>=length) ) {
		errno = EBADF;
		return -1;
	}

	int i;

	for( i=search; i<length; i++ )
		if(!pointers[i]) break;

	if( i==length ) {
		errno = EMFILE;
		return -1;
	} else {
		return dup2(fd,i);
	}
}

int CondorFileTable::dup2( int fd, int nfd )
{
	int result;

	if( (fd<0) || (fd>=length) || (pointers[fd]==0) || (nfd<0) || (nfd>=length) ) {
		errno = EBADF;
		return -1;
	}

	if( fd==nfd ) return fd;

	if( pointers[nfd]!=0 ) close(nfd);

	pointers[nfd] = pointers[fd];

	/* If we are in standalone checkpointing mode,
	   we need to perform a real dup.  Because we
	   are constructing this file table identically
	   to the real system table, the result of
	   the syscall _ought_ to be the same as the
	   result of this virtual dup, but we will
	   check just to be sure. */

	if( MyImage.GetMode()==STANDALONE ) {
		result = _condor_internal_dup2(fd,nfd);
		if(result!=nfd) _condor_error_fatal("internal_dup2(%d,%d) returned %d but I wanted %d!\n",fd,nfd,result,nfd);
	}
	    
	return nfd;
}

/*
fchdir is a little sneaky.  A particular fd might
be remapped to any old file name or access method, which
may or may not support changing directories.

(furthermore, what does it mean to fchdir to a file that
is mapped locally to, say, /tmp/foo?)

So, we will just avoid the problem by extracting the name
of the file, and calling chdir.
*/

int CondorFileTable::fchdir( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	dprintf(D_ALWAYS,"CondorFileTable::fchdir(%d) will try chdir(%s)\n",
		fd, pointers[fd]->file->get_name() );

	return ::chdir( pointers[fd]->file->get_name() );
}

/*
ioctls don't affect the open file table, so we will pass them
along to the individual access method, which will decide
if it can be supported.
*/

int CondorFileTable::ioctl( int fd, int cmd, int arg )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->file->ioctl(cmd,arg);
}

int CondorFileTable::ftruncate( int fd, size_t length )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	/* The below length check is a hack to patch an f77 problem on
	   OSF1.  - Jim B. */

	if( length<0 ) return 0;

	return pointers[fd]->file->ftruncate(length);
}

/*
fcntl does all sorts of wild stuff.
Some operations affect the fd table.
Perform those here.  Others merely modify
an individual file.  Pass these along to
the access method, which may support the operation,
or fail with its own error.
*/

int CondorFileTable::fcntl( int fd, int cmd, int arg )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	switch(cmd) {
		#ifdef F_DUPFD
		case F_DUPFD:
		#endif
			return search_dup2(fd,arg);

		#ifdef F_DUP2FD
		case F_DUP2FD:
		#endif
			return dup2(fd,arg);

		default:
			return pointers[fd]->file->fcntl(cmd,arg);
			break;
	}
}

int CondorFileTable::fsync( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	pointers[fd]->file->fsync();
}

/*
poll converts each fd into its mapped equivalent and then does a local poll.  A poll() on a non local file causes a warning and failure with EINVAL
*/

int CondorFileTable::poll( struct pollfd *fds, int nfds, int timeout )
{
	struct pollfd *realfds;

	realfds = (struct pollfd *) malloc( sizeof(struct pollfd)*nfds );
	if(!realfds) {
		errno = ENOMEM;
		return -1;
	}

	for( int i=0; i<nfds; i++ ) {
		if(_condor_file_is_local(fds[i].fd)) {
			realfds[i].fd = _condor_file_table_map(fds[i].fd);
			realfds[i].events = fds[i].events;
		} else {
			_condor_warning("poll() is not supported for remote files");
			free(realfds);
			errno = EINVAL;
			return -1;
		}
	}

	int scm = SetSyscalls( SYS_LOCAL|SYS_UNMAPPED );
	int result = ::poll( realfds, nfds, timeout );
	SetSyscalls(scm);

	for( int i=0; i<nfds; i++ ) {
		fds[i].revents = realfds[i].revents;
	}

	free(realfds);

	return result;
}

/*
select transforms its arguments into local fds and then does a local select.  A select on a non-local fd causes a return with EINVAL and a warning message.
*/

int CondorFileTable::select( int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *timeout )
{
	fd_set	r_real, w_real, e_real;
	int	n_real, fd, fd_real;
	int	result, scm;

	FD_ZERO( &r_real );
	FD_ZERO( &w_real );
	FD_ZERO( &e_real );

	n_real = 0;

	/* For each fd, put its mapped equivalent in the appropriate set */

	for( fd=0; fd<n; fd++ ) {
		fd_real = _condor_file_table_map(fd);
		if( fd_real>=0 ) {
			if( _condor_file_is_local(fd) ) {
				if( r && FD_ISSET(fd,r) )
					FD_SET(fd_real,&r_real);
				if( w && FD_ISSET(fd,w) )
					FD_SET(fd_real,&w_real);
				if( e && FD_ISSET(fd,e) )
					FD_SET(fd_real,&e_real);
				n_real = MAX( n_real, fd_real+1 );
			} else {
				if( (r && FD_ISSET(fd,r)) ||
				    (w && FD_ISSET(fd,w)) ||
				    (e && FD_ISSET(fd,e)) ) {
					_condor_warning("select() is not supported for remote files.");
					errno = EINVAL;
					return -1;
				}
			}
		}
	}

	/* Do a local select */

	scm = SetSyscalls( SYS_LOCAL|SYS_UNMAPPED );
	result = select( n_real, &r_real, &w_real, &e_real, timeout );
	SetSyscalls(scm);

	if( r ) FD_ZERO( r );
	if( w ) FD_ZERO( w );
	if( e ) FD_ZERO( e );

	/* Set the bit for each fd whose real equivalent is set. */

	for( fd=0; fd<n; fd++ ) {
		fd_real = _condor_file_table_map(fd);
		if( fd_real>=0 && _condor_file_is_local(fd) ) {
			if( r && FD_ISSET(fd_real,&r_real) )
				FD_SET(fd,r);
			if( w && FD_ISSET(fd_real,&w_real) )
				FD_SET(fd,w);
			if( e && FD_ISSET(fd_real,&e_real) )
				FD_SET(fd,e);
		}
	}

	return result;
}

int CondorFileTable::get_buffer_size()
{
	return buffer_size;
}

int CondorFileTable::get_buffer_block_size()
{
	return buffer_block_size;
}

void CondorFileTable::checkpoint()
{
	dprintf(D_ALWAYS,"CondorFileTable::checkpoint\n");

	dump();

	if( MyImage.GetMode() != STANDALONE ) {
		REMOTE_syscall( CONDOR_get_buffer_info, &buffer_size, &buffer_block_size );
		REMOTE_syscall( CONDOR_getwd, working_dir );
	} else {
		::getcwd( working_dir, _POSIX_PATH_MAX );
	}

	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			pointers[i]->file->checkpoint();
			pointers[i]->info->report();
		}
	}

	REMOTE_syscall( CONDOR_get_buffer_info, &buffer_size, &buffer_block_size );
}

void CondorFileTable::suspend()
{
	dprintf(D_ALWAYS,"CondorFileTable::suspend\n");

	dump();

	if( MyImage.GetMode() != STANDALONE ) {
		REMOTE_syscall( CONDOR_get_buffer_info, &buffer_size, &buffer_block_size );
		REMOTE_syscall( CONDOR_getwd, working_dir );
	} else {
		::getcwd( working_dir, _POSIX_PATH_MAX );
	}

	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			pointers[i]->file->suspend();
			pointers[i]->info->report();
		}
	}
}

void CondorFileTable::resume()
{
	int result;
	int temp;

	resume_count++;

	dprintf(D_ALWAYS,"CondorFileTable::resume_count=%d\n");
	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	if(MyImage.GetMode()!=STANDALONE) {
		REMOTE_syscall( CONDOR_get_buffer_info, &buffer_size, &buffer_block_size );
		result = REMOTE_syscall(CONDOR_chdir,working_dir);
	} else {
		result = ::chdir( working_dir );
	}

	if(result<0) _condor_error_retry("After checkpointing, I couldn't find %s again!",working_dir);

	/* Resume works a little differently, depending on the image mode.
	   In the standard condor universe, we just go through each file
	   object and call its resume method to reopen the file. */

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {

			/* No matter what the mode, we tell the file to
			   resume itself. */

			pointers[i]->file->resume(resume_count);

			/* In standalone mode, we check to see if fd i shares
			   an fp with a lower numbered fd.  If it does, then
			   we need to dup the lower number into the upper number.
			   Just like dup2, we need to check that the result of
			   the syscall is what we expected. */

			if( MyImage.GetMode()==STANDALONE ) {
				for( int j=0; j<i; j++ ) {
					if( pointers[j]==pointers[i] ) {
						int result = _condor_internal_dup2(j,i);
						if(result!=i) _condor_error_fatal("internal_dup2(%d,%d) returned %d but I wanted %d!\n",j,i,result,i);
						break;
					}
				}
			}	
		}
	}

	dump();
}

int CondorFileTable::map_fd_hack( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->file->map_fd_hack();
}

int CondorFileTable::local_access_hack( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->file->local_access_hack();
}



