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
#include "condor_file_info.h"

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

#include "file_state.h"
#include "file_types.h"
#include "file_table_interf.h"

#include <stdarg.h>
#include <sys/errno.h>

// XXX Where is the header for this?
extern "C" int syscall( int kind, ... );

// XXX Where is the header for this?
extern int errno;

OpenFileTable *FileTab=0;

class FilePointer {
public:
	FilePointer( File *f ) {
		file = f;
		offset = 0;
		use_count = 0;
	}

	void	add_user()		{ use_count++; }
	void	remove_user()		{ use_count--; }
	int	get_use_count()		{ return use_count; }
	off_t	get_offset()		{ return offset; }
	void	set_offset( size_t s )	{ offset=s; }
	File	*get_file()		{ return file; }

private:
	File	*file;		// What file do I refer to?
	off_t	offset;		// The current seek pointer for this fd
	int	use_count;	// How many fds share this fp?
};

/*
This init function could be called from just about anywhere, perhaps
even when there is no shadow to talk to.  We will delay initializing
a buffer until we do the first remote open.
*/

void OpenFileTable::init()
{
	buffer = 0;
	prefetch_size = 0;

	int scm = SetSyscalls( SYS_UNMAPPED | SYS_LOCAL );
	length = sysconf(_SC_OPEN_MAX);
	SetSyscalls(scm);

	pointers = new (FilePointer *)[length];

	for( int i=0; i<length; i++ ) pointers[i]=0;

	/* Until we know better, identity map std files */

	pre_open( 0, 1, 0, 0 );
	pre_open( 1, 0, 1, 0 );
	pre_open( 2, 0, 1, 0 );

}

static void flush_and_disable_buffer()
{
	InitFileState();
	FileTab->flush();
	FileTab->disable_buffer();
}

void OpenFileTable::init_buffer()
{
	int blocks=0, blocksize=0;

	if(buffer) return;

	if(REMOTE_syscall(CONDOR_get_buffer_info,&blocks,&blocksize,&prefetch_size)<0) {
		dprintf(D_ALWAYS,"get_buffer_info failed!");
		buffer = 0;
		return;
	}

	buffer = new BufferCache( blocks, blocksize );

	dprintf(D_ALWAYS,"Buffer cache is %d blocks of %d bytes.\n",blocks,blocksize);
	dprintf(D_ALWAYS,"Prefetch size is %d\n",prefetch_size);

	if( !blocks || !blocksize ) {
		dprintf(D_ALWAYS,"Buffer is disabled\n.");
		buffer = 0;
		return;
	}

	// If the program ends via exit() or return from main,
	// then flush the buffers.  Furthermore, disable any further buffering,
	// because iostream or stdio will flush _after_ us.

	if(atexit(flush_and_disable_buffer)<0) {
		file_warning("atexit() failed.  Buffering is disabled.\n");
		delete buffer;
		buffer = 0;
	}
}

void OpenFileTable::disable_buffer()
{
	if(buffer) {
		delete buffer;
		buffer = 0;
	}
}

void OpenFileTable::flush()
{
	if(buffer) buffer->flush();
}

void OpenFileTable::dump()
{
	dprintf(D_ALWAYS,"\nOPEN FILE TABLE:\n");

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			dprintf(D_ALWAYS,"fd: %d offset: %d dups: %d ",
				i,pointers[i]->get_offset(),
				pointers[i]->get_use_count());

			pointers[i]->get_file()->dump();
		}
		dprintf(D_ALWAYS,"\n");
	}
}

int OpenFileTable::pre_open( int fd, int readable, int writeable, int is_remote )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]) ) {
		errno = EBADF;
		return -1;
	}

	File *f;

	if( is_remote ) f = new RemoteFile;
	else f = new LocalFile;

	f->force_open(fd,readable,writeable);
	f->add_user();

	pointers[fd] = new FilePointer(f);
	pointers[fd]->add_user();

	return fd;	
}

int OpenFileTable::find_name( const char *path )
{
	for( int fd=0; fd<length; fd++ ) {
		if( pointers[fd] && !strcmp(pointers[fd]->get_file()->get_name(),path) ) {
			return fd;
		}
	}
	return -1;
}

int OpenFileTable::find_empty()
{
	for( int fd=0; fd<length; fd++ ) {
		if( !pointers[fd] ) return fd;
	}
	return -1;
}

int OpenFileTable::open( const char *path, int flags, int mode )
{
	int	kind;
	int	fd, new_fd;
	char	new_path[_POSIX_PATH_MAX];
	File	*f;

	// Opening files O_RDWR is not safe across checkpoints
	// However, there is no problem as far as the rest of the
	// file code is concerned.  We will catch it here,
	// but the rest of the file table and buffer has no problem.

	if( flags & O_RDWR )
		file_warning("Opening file '%s' for read and write is not safe across all checkpoints!  You should use separate files for reading and writing.\n",path);

	// Find an open slot in the table

	fd = find_empty();
	if( fd<0 ) {
		errno = EMFILE;
		return -1;
	}

	if( LocalSysCalls() ) {

		f = new LocalFile;
		if( f->open(path,flags,mode)<0 ) {
			delete f;
			return -1;
		}

	} else {

		init_buffer();

		kind = REMOTE_syscall( CONDOR_file_info, path, &new_fd, new_path );

		int match = find_name(new_path);
		if( match>=0 ) {

			// A file with this remapped name is already open,
			// so share the file object with the existing pointer.

			f = pointers[match]->get_file();

		} else {

			// The file is not already open, so create a new file object

			switch( kind ) {
				case IS_PRE_OPEN:
				f = new LocalFile;
				f->force_open( new_fd, 1, 1 );
				break;

				case IS_NFS:
				case IS_AFS:
				case IS_LOCAL:
				f = new LocalFile;
				break;

				case IS_RSC:
				default:
				f = new RemoteFile;
				f->enable_buffer();
				break;
			}

			// Do not buffer stderr
			if(fd==2) f->disable_buffer();

			// Open according to the access method.
			// Forced files are not re-opened.

			if( (kind!=IS_PRE_OPEN) && (f->open(new_path,flags,mode)<0) ) {
				delete f;
				return -1;
			}
		}
	}

	// Install a new fp and update the use counts 

	pointers[fd] = new FilePointer(f);
	pointers[fd]->add_user();
	pointers[fd]->get_file()->add_user();

	// Prefetch as allowed

	if( f->ok_to_buffer() && buffer && (prefetch_size>0) )
		buffer->prefetch(f,0,prefetch_size);

	return fd;
}

/* 
Close is a little tricky.
Find the fp corresponding to the fd.
If I am the last user of this fp:
	Find the file corresponding to this fp.
	If I am the last user of this file:
		Flush the buffer
		Delete the file
	Delete the fp
In any case, zero the appropriate entry of the table.
*/

int OpenFileTable::close( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}
	
	pointers[fd]->remove_user();
	if( pointers[fd]->get_use_count()<=0 ) {
		pointers[fd]->get_file()->remove_user();
		if( pointers[fd]->get_file()->get_use_count()<=0 ) {
			if(buffer) buffer->flush(pointers[fd]->get_file());
			pointers[fd]->get_file()->close();
			delete pointers[fd]->get_file();
		}
		delete pointers[fd];
	}

	pointers[fd] = 0;

	return 0;
}


ssize_t OpenFileTable::read( int fd, void *data, size_t nbyte )
{
	if( (fd>=length) || (fd<0) || (pointers[fd]==0) ||
	    (!pointers[fd]->get_file()->is_readable()) ) {
		errno = EBADF;
		return -1;
	}
	
	if( (!data) || (nbyte<0) ) {
		errno = EINVAL;
		return -1;
	}

	FilePointer *fp = pointers[fd];
	File *f = fp->get_file();

	// First, figure out the appropriate place to read from.

	int offset = fp->get_offset();
	int size = f->get_size();
	int actual;

	if( (offset>=size) || (nbyte==0) ) return 0;

	// If buffering is allowed, read from the buffer,
	// otherwise, go straight to the access method

	if( f->ok_to_buffer() && buffer ) {

		// The buffer doesn't know about file sizes,
		// so cut off extra long reads here

		if( (offset+nbyte)>size ) nbyte = size-offset;

		// Now fetch from the buffer

		actual = buffer->read( f, offset, (char*)data, nbyte );


	} else {
		actual = f->read( offset, (char*)data, nbyte );
	}

	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Update the offset
	fp->set_offset(offset+actual);

	// Return the number of bytes read
	return actual;
}

ssize_t OpenFileTable::write( int fd, const void *data, size_t nbyte )
{
	if( (fd>=length) || (fd<0) || (pointers[fd]==0) ||
	    (!pointers[fd]->get_file()->is_writeable()) ) {
		errno = EBADF;
		return -1;
	}

	if( (!data) || (nbyte<0) ) {
		errno = EINVAL;
		return -1;
	}

	if( nbyte==0 ) return 0;

	FilePointer *fp = pointers[fd];
	File *f = fp->get_file();

	// First, figure out the appropriate place to write to.

	int offset = fp->get_offset();
	int actual;

	// If buffering is allowed, write to the buffer,
	// otherwise, go straight to the access method

	if( f->ok_to_buffer() && buffer ) {
      		actual = buffer->write( f, offset, (char*)data, nbyte );
	} else {
		actual = f->write( offset, (char*)data, nbyte );
	}

	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Update the offset, and mark the file bigger if necessary
	if( (offset+actual)>f->get_size() )
		f->set_size(offset+actual);

	fp->set_offset(offset+actual);

	// Return the number of bytes written
	return actual;
}

off_t OpenFileTable::lseek( int fd, off_t offset, int whence )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	FilePointer *fp = pointers[fd];
	File *f = fp->get_file();
	int temp;

	// Compute the new offset first.
	// If the new offset is illegal, don't change it.

	if( whence == SEEK_SET ) {
		temp = offset;
	} else if( whence == SEEK_CUR ) {
	        temp = fp->get_offset()+offset;
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
		fp->set_offset(temp);
		return temp;
	}
}

int OpenFileTable::dup( int fd )
{
	return search_dup2( fd, 0 );
}

int OpenFileTable::dup2( int fd, int nfd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ||
	    (nfd<0) || (nfd>=length) ) {
		errno = EBADF;
		return -1;
	}

	if( pointers[nfd]!=0 ) close(nfd);

	pointers[fd]->add_user();
	pointers[nfd] = pointers[fd];

	return nfd;
}

int OpenFileTable::search_dup2( int fd, int search )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ||
	    (search<0) || (search>=length) ) {
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

/*
fchdir is a little sneaky.  A particular fd might
be remapped to any old file name or access method, which
may or may not support changing directories.

(furthermore, what does it mean to fchdir to a file that
is mapped locally to, say, /tmp/foo?)

So, we will just avoid the problem by extracting the name
of the file, and calling chdir.  (Could be local or remote.)
*/

int OpenFileTable::fchdir( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return chdir( pointers[fd]->get_file()->get_name() );
}


int OpenFileTable::fstat( int fd, struct stat *buf )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->fstat(buf);
}


int OpenFileTable::ioctl( int fd, int cmd, int arg )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->ioctl(cmd,arg);
}

int OpenFileTable::flock( int fd, int op )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->flock(op);
}

int OpenFileTable::fstatfs( int fd, struct statfs *buf )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->fstatfs(buf);
}

int OpenFileTable::fchown( int fd, uid_t owner, gid_t group )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->fchown(owner,group);
}

int OpenFileTable::fchmod( int fd, mode_t mode )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->fchmod( mode );
}

int OpenFileTable::ftruncate( int fd, size_t length )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	/* The below length check is a hack to patch an f77 problem on
	   OSF1.  - Jim B. */

	if( length<0 ) return 0;

	buffer->flush(pointers[fd]->get_file());

	return pointers[fd]->get_file()->ftruncate(length);
}

/*
fcntl does all sorts of wild stuff.
We can classify these into three types:
	1 - Calls that manipulate the fd table. (dup)
	    Perform these here.
	2 - Calls that manipulate a particular file.
	    Pass those along to the right file object.
	3 - Calls that require variable sized args
	    We don't support those.
*/

int OpenFileTable::fcntl( int fd, int cmd, int arg )
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

		#ifdef F_GETLK64
		case F_GETLK64:
		#endif

		#ifdef F_SETLK64
		case F_SETLK64:
		#endif

		#ifdef F_SETLKW64
		case F_SETLKW64:
		#endif

		case F_GETFD:
		case F_GETFL:
		case F_SETFD:
		case F_SETFL:

			return pointers[fd]->get_file()->fcntl(cmd,arg);
			break;

		default:
			file_warning("fcntl(%d,%d,...) is not supported.\n",fd,cmd);
			errno = ENOTSUP;
			return -1;
	}

}

int OpenFileTable::fsync( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	if(buffer) buffer->flush(pointers[fd]->get_file());

	pointers[fd]->get_file()->fsync();
}

void OpenFileTable::checkpoint()
{
	dump();

	if( MyImage.GetMode() == STANDALONE ) {
		getwd( working_dir );
	} else {
		REMOTE_syscall( CONDOR_getwd, working_dir );
	}

	for( int i=0; i<length; i++ )
	     if( pointers[i] ) pointers[i]->get_file()->checkpoint();
}

void OpenFileTable::suspend()
{
	dump();

	if( MyImage.GetMode() == STANDALONE ) {
		getwd( working_dir );
	} else {
		REMOTE_syscall( CONDOR_getwd, working_dir );
	}

	for( int i=0; i<length; i++ )
	     if( pointers[i] ) pointers[i]->get_file()->suspend();
}

void OpenFileTable::resume()
{
	int result;

	if( MyImage.GetMode() == STANDALONE ) {
		result = syscall( SYS_chdir, working_dir );
	} else {
		result = REMOTE_syscall( CONDOR_chdir, working_dir );
	}

	if( result<0 ) {
		EXCEPT("Unable to restore working directory after checkpoint!");
	}

	for( int i=0; i<length; i++ )
	     if( pointers[i] ) pointers[i]->get_file()->resume();

	dump();
}

int OpenFileTable::map_fd_hack( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->map_fd_hack();
}

int OpenFileTable::local_access_hack( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->local_access_hack();
}


