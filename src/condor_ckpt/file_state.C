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
#include "condor_file_fd.h"
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
	CondorFileInfo( char *n ) {
		strcpy(logical_name,n);
		open_count = 0;
		read_count = read_bytes = 0;
		write_count = write_bytes = 0;
		seek_count = 0;
		already_warned = 0;
		next = 0;
	}

	void	report() {
		if( RemoteSysCalls() ) {
			REMOTE_syscall( CONDOR_report_file_info_new, logical_name, open_count, read_count, write_count, seek_count, read_bytes, write_bytes );
		}
	}

	char	logical_name[_POSIX_PATH_MAX];

	long long open_count;
	long long read_count, read_bytes;
	long long write_count, write_bytes;
	long long seek_count;
	int already_warned;

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
	CondorFilePointer( CondorFile *f, CondorFileInfo *i, int fl ) {
		file = f;
		info = i;
		flags = fl;
		offset = 0;
	}

	CondorFile *file;
	CondorFileInfo *info;
	int flags;
	off_t offset;
};

/*
This is a little tricky.

When a program exits, the last thing the startup code does is flush
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
	got_buffer_info = 0;
	flush_mode = 0;
	buffer_size = 0;
	buffer_block_size = 0;

	int scm = SetSyscalls( SYS_UNMAPPED | SYS_LOCAL );
	length = sysconf(_SC_OPEN_MAX);
	SetSyscalls(scm);

	pointers = new (CondorFilePointer *)[length];
	if(!pointers) {
		_condor_error_retry("Out of memory!\n");
	}

	for( int i=0; i<length; i++ ) {
		pointers[i]=0;
	}

	info_head = 0;

	pointers[0] = new CondorFilePointer(0,make_info("default stdin"),O_RDONLY);
	pointers[1] = new CondorFilePointer(0,make_info("default stdout"),O_WRONLY);
	pointers[2] = new CondorFilePointer(0,make_info("default stderr"),O_WRONLY);

	atexit( _condor_disable_buffering );
}

void CondorFileTable::flush()
{
	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			if( pointers[i]->file ) {
				pointers[i]->file->fsync();
			}
			if( pointers[i]->info ) {
				pointers[i]->info->report();
			}
		}
	}
}

void CondorFileTable::set_flush_mode( int on_off )
{
	flush_mode = on_off;
}

void CondorFileTable::set_aggravate_mode( int on_off )
{
	aggravate_mode = on_off;
}

void CondorFileTable::dump()
{
	CondorFilePointer *p;
	CondorFileInfo *i;
	CondorFile *f;

	dprintf(D_ALWAYS,"\nOPEN FILE TABLE:\n");

	for( int j=0; j<length; j++ ) {

		p = pointers[j];
		if( p ) {
			dprintf(D_ALWAYS,"fd %d\n",j);
			dprintf(D_ALWAYS,"\toffset:       %d\n",p->offset);
			dprintf(D_ALWAYS,"\tdups:         %d\n",count_pointer_uses(p));
			dprintf(D_ALWAYS,"\topen flags:   0x%x\n",p->flags);
			i = p->info;
			f = p->file;
			if( i ) {
				dprintf(D_ALWAYS,"\tlogical name: %s\n",i->logical_name);
			}
			if( f ) {
				dprintf(D_ALWAYS,"\turl:          %s\n",f->get_url());
				dprintf(D_ALWAYS,"\tsize:         %d\n",f->get_size());
				dprintf(D_ALWAYS,"\topens:        %d\n",count_file_uses(f));
			} else {
				dprintf(D_ALWAYS,"\tnot currently bound to a url.\n");
			}
		}
	}
}

/*
If this url is already open, return its file descriptor.
*/

int CondorFileTable::find_url( char *url )
{
	for( int i=0; i<length; i++ )
		if( pointers[i] )
			if( pointers[i]->file )
				if( !strcmp(pointers[i]->file->get_url(),url) )
					return i;

	return -1;
}


/*
If this logical name is already open, return its file descriptor.
*/

int CondorFileTable::find_logical_name( char *logical_name )
{
	for( int i=0; i<length; i++ )
		if( pointers[i] )
			if( pointers[i]->info )
				if( !strcmp(pointers[i]->info->logical_name,logical_name) )
					return i;

	return -1;
}

/*
Make a new info block, or re-use an existing one.
*/

CondorFileInfo * CondorFileTable::make_info( char *logical_name )
{
	CondorFileInfo *i;

	for( i=info_head; i; i=i->next )
		if( !strcmp(logical_name,i->logical_name) )
			return i;

	i = new CondorFileInfo( logical_name );
	i->next = info_head;
	info_head = i;

	return i;
}

/*
Usually, chose the lowest numbered file descriptor that is available.
However, if aggravation is enabled, never choose an fd between
3 and 31.  This helps to uncover bugs due to untrapped system calls.
*/

int CondorFileTable::find_empty()
{
	int fd;

	for( fd=0; fd<length; fd++ ) {
		if( aggravate_mode ) {
			if( fd>2 && fd<32 ) {
				continue;
			}
		}
		if( !pointers[fd] ) {
			return fd;
		}
	}

	return -1;
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
		if(pointers[fd])
			if(pointers[fd]->file==f)
				count++;

	return count;
}

/*
Convert a logical file name into a physical url by any and all methods available.  This function will not fail -- it will always fall back on something reasonable.
*/

void CondorFileTable::lookup_url( char *logical_name, char *url )
{
	// Special case: Requests to look up default standard files
	// should be mapped to constant file descriptors.
 
	if(!strcmp(logical_name,"default stdin")) {
		strcpy(url,"fd:0");
		return;
	} else if(!strcmp(logical_name,"default stdout")) {
		strcpy(url,"fd:1");
		return;
	} else if(!strcmp(logical_name,"default stderr")) {
		strcpy(url,"fd:2");
		return;
	}

	// If in local mode, just use local:logical_name.
	// Otherwise, ask shadow.  If that fails, try remote:logical_name.

	if( LocalSysCalls() ) {
		sprintf(url,"local:%s",logical_name);
	} else {
		int result = REMOTE_syscall( CONDOR_get_file_info, logical_name, url );
		if( result<0 ) {
			sprintf(url,"remote:%s",logical_name);
		}

		if(!got_buffer_info) {
			int temp;
			REMOTE_syscall( CONDOR_get_buffer_info, &buffer_size, &buffer_block_size, &temp );
			got_buffer_info = 1;
		}
	}

}

/*
Given a url, create and open an instance of CondorFile.
*/

CondorFile * CondorFileTable::open_url( char *url, int flags, int mode, int allow_buffer )
{
	CondorFile *f;

	if(!strncmp(url,"local",5)) {
		f = new CondorFileLocal;
	} else if(!strncmp(url,"fd",2)) {
		f = new CondorFileFD;
	} else if(!strncmp(url,"special",7)) {
		f = new CondorFileSpecial;
	} else if(!strncmp(url,"remote",6)) {
		if( allow_buffer && !buffer_size && !buffer_block_size ) {
			f = new CondorFileRemote;
		} else {
			f = new CondorFileBuffer(new CondorFileRemote);
		}
	} else {
		_condor_warning("I don't understand url (%s).",url);
		errno = ENOENT;
		return 0;
	}

	if( f->open(url,flags,mode)>=0 ) {
		return f;
	} else {
		delete f;
		return 0;
	}
}

/*
Open a url.  If the open fails, retry using remote system calls when possible.
*/

CondorFile * CondorFileTable::open_url_retry( char *url, int flags, int mode, int allow_buffer )
{
	CondorFile *f;

	f = open_url( url, flags, mode, allow_buffer );
	if(f) return f;

	char method[_POSIX_PATH_MAX];
	char path[_POSIX_PATH_MAX];
	char new_url[_POSIX_PATH_MAX];

	if( RemoteSysCalls() ) {
		if( strncmp(url,"remote",6) ) {
			sscanf(url,"%[^:]:%s",method,path);
			sprintf(new_url,"remote:%s",path);
			return open_url( new_url, flags, mode, allow_buffer );
		}
	}

	return 0;
}

/*
Compare the flag of a file already opened and a file about to be opened.  Return true if the old flags will not allow the access requested by the new flags. 
*/

static int needs_reopen( int old_flags, int new_flags )
{
	int mask = (O_RDONLY|O_WRONLY|O_RDWR);

	if( (old_flags&mask) == (new_flags&mask) ) return 0;
	if( (old_flags&mask) == O_RDWR ) return 0;

	return 1;
}

/*
Create and open a file object from a logical name.  If either the logical name or url are already in the file table AND the file was previously opened with compatible flags, then share the object.  If not, elevate the flags to a common denominator and open a new object suitable for both purposes.
*/

CondorFile * CondorFileTable::open_file_unique( char *logical_name, int flags, int mode, int allow_buffer )
{
	char url[_POSIX_PATH_MAX];

	int match = find_logical_name( logical_name );
	if( match==-1 || !pointers[match]->file ) {
		lookup_url( (char*)logical_name, url );
		match = find_url( url );
		if( match==-1 ) {
			return open_url_retry( url, flags, mode, allow_buffer );
		}
	}

	CondorFilePointer *p = pointers[match];

	if( needs_reopen( p->flags, flags ) ) {
		flags &= ~(O_RDONLY);
		flags &= ~(O_WRONLY);
		flags |= O_RDWR;

		CondorFile *f = open_url_retry( p->file->get_url(), flags, mode, allow_buffer );
		if(!f) return 0;

		if( p->file->close()!=0 ) return 0;
		delete p->file;

		p->file = f;
	}

	return p->file;
}

int CondorFileTable::open( const char *logical_name, int flags, int mode )
{
	CondorFile *file=0;
	CondorFileInfo *info=0;

	// Find a fresh file descriptor
	// This has to come first so we know if this is stderr

	int fd = find_empty();
	if(fd<0) {
		errno = EMFILE;
		return -1;
	}

	file = open_file_unique( (char*) logical_name, flags, mode, fd!=2 );
	if(!file) return -1;

	info = make_info( (char*) logical_name );
	info->open_count++;

	// Flags that should be applied once only are now removed

	flags &= ~(O_TRUNC);
	flags &= ~(O_CREAT);
	
	// Install the pointer and return!

	pointers[fd] = new CondorFilePointer(file,info,flags);
	return fd;
}

int CondorFileTable::install_special( int real_fd )
{
	int fd = find_empty();
	if(fd<0) return -1;

	CondorFileInfo *i= new CondorFileInfo("special");
	i->already_warned = 1;

	CondorFileSpecial *s = new CondorFileSpecial();
	s->attach( real_fd );

	pointers[fd] = new CondorFilePointer( s, i, O_RDWR );

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

	fds[0] = install_special(real_fds[0]);
	fds[1] = install_special(real_fds[1]);

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

	return install_special(real_fd);
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

	CondorFilePointer *p = pointers[fd];
	CondorFileInfo *i = p->info;
	CondorFile *f = p->file;

	// If this is the last use of the file, flush, close, and delete.
	// If the close fails (yes, possible!) then return the error
	// but do not adjust the table.

	if(f && count_file_uses(f)==1) {
		i->report();
		int result = f->close();
		if( result!=0 ) {
			return result;
		}
		delete f;
	}

	// If this is the last use of the pointer, delete it
	// but leave the info block -- it is in the linked
	// list beginning with info_head.

	if(count_pointer_uses(p)==1) {
		delete p;
	}

	// In any case, mark the fd as unused
	// and return.

	pointers[fd]=0;
	return 0;
}

ssize_t CondorFileTable::read( int fd, void *data, size_t nbyte )
{
	if( resume(fd)<0 ) return -1;

	if( !pointers[fd]->file->is_readable() ) {
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

	// Perhaps send a warning message
	check_safety(fp);

	// Return the number of bytes read
	return actual;
}

ssize_t CondorFileTable::write( int fd, const void *data, size_t nbyte )
{
	if( resume(fd)<0 ) return -1;

	if( !pointers[fd]->file->is_writeable() ) {
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

	// Perhaps send a warning message
	check_safety(fp);

	// If flush_mode is enabled, flush it to disk
	if(flush_mode) {
		f->fsync();
		fp->info->report();
	}

	// Return the number of bytes written
	return actual;
}

void CondorFileTable::check_safety( CondorFilePointer *fp )
{
	if( fp->info->read_bytes && fp->info->write_bytes && !fp->info->already_warned ) {
		fp->info->already_warned = 1;
		_condor_warning("File '%s' used for both reading and writing.  This is not checkpoint-safe.\n",fp->info->logical_name);
	}
}

off_t CondorFileTable::lseek( int fd, off_t offset, int whence )
{
	if( resume(fd)<0 ) return -1;

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

	if( resume(fd)<0 ) return -1;

	if( (nfd<0) || (nfd>=length) ) {
		errno = EBADF;
		return -1;
	}

	if( fd==nfd ) return fd;

	// If this fd is already in use, close it.
	// But, close _can_ fail!  If that happens,
	// abort the dup with the errno from the close.

	if( pointers[nfd]!=0 ) {
		result = close(nfd);
		if( result==-1 ) {
			return result;
		}
	}

	pointers[nfd] = pointers[fd];
	    
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
	if( resume(fd)<0 ) return -1;

	dprintf(D_ALWAYS,"CondorFileTable::fchdir(%d) will try chdir(%s)\n",
		fd, pointers[fd]->info->logical_name );

	return ::chdir( pointers[fd]->info->logical_name );
}

/*
ioctls don't affect the open file table, so we will pass them
along to the individual access method, which will decide
if it can be supported.
*/

int CondorFileTable::ioctl( int fd, int cmd, int arg )
{
	if( resume(fd)<0 ) return -1;
	return pointers[fd]->file->ioctl(cmd,arg);
}

int CondorFileTable::ftruncate( int fd, size_t size )
{
	if( resume(fd)<0 ) return -1;

	/* The below length check is a hack to patch an f77 problem on
	   OSF1.  - Jim B. */

	if( size<0 ) return 0;

	return pointers[fd]->file->ftruncate(size);
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
	struct flock *f;

	if( resume(fd)<0 ) return -1;

	switch(cmd) {
		#ifdef F_DUPFD
		case F_DUPFD:
		#endif
			return search_dup2(fd,arg);

		#ifdef F_DUP2FD
		case F_DUP2FD:
		#endif
			return dup2(fd,arg);


		#ifdef F_FREESP
		case F_FREESP:
		#endif

		#ifdef F_FREESP64
		case F_FREESP64:
		#endif

			/*
			A length of zero to FREESP indicates the file should
			be truncated at the start value.

			Truncation must be handled at this level, and not at an individual object,
			because the entire chain of objects, e.g. CFBuffer->CFCompress->CFRemote,
			must all be informed about a truncate in turn.
			*/

			f = (struct flock *)arg;
			if( (f->l_whence==0) && (f->l_len==0) ) {
				return FileTab->ftruncate(fd,f->l_start);
			}

			/* Otherwise, fall through here. */

		default:
			return pointers[fd]->file->fcntl(cmd,arg);
			break;
	}
}

int CondorFileTable::fsync( int fd )
{
	if( resume(fd)<0 ) return -1;
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
	result = ::select( n_real, &r_real, &w_real, &e_real, timeout );
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

/*
Checkpoint the state of the file table.  This involves storing the current working directory, reporting the disposition of all files, and flushing and closing them.  The closed files will be re-bound and re-opened lazily after a resume.  Notice that these need to be closed even for a periodic checkpoint, because logical names must be re-bound to physical urls.  If a close fails at this point, we must roll-back, otherwise the data on disk would not be consistent with the checkpoint image.
*/

void CondorFileTable::checkpoint()
{
	int temp;

	dprintf(D_ALWAYS,"CondorFileTable::checkpoint\n");

	dump();

	if( MyImage.GetMode() != STANDALONE ) {
		REMOTE_syscall( CONDOR_get_buffer_info, &buffer_size, &buffer_block_size, &temp );
		REMOTE_syscall( CONDOR_getwd, working_dir );
	} else {
		::getcwd( working_dir, _POSIX_PATH_MAX );
	}

	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			if( pointers[i]->info ) {
				pointers[i]->info->report();
			}
			if( pointers[i]->file ) {
				if( count_file_uses(pointers[i]->file)==1 ) {
					temp = pointers[i]->file->close();
					if( temp==-1 ) {
						_condor_error_retry("Unable to commit data to file %s!\n",pointers[i]->file->get_url());
					}
					delete pointers[i]->file;
				}
				pointers[i]->file=0;						
			}
		}
	}
}

/*
Resume the state of the file table after a checkpoint.  There isn't much to do here except move back to the current working directory.  All the files should be resumed lazily.
*/

void CondorFileTable::resume()
{
	int result;
	int temp;

	dprintf(D_ALWAYS,"CondorFileTable::resume\n");
	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	if(MyImage.GetMode()!=STANDALONE) {
		REMOTE_syscall( CONDOR_get_buffer_info, &buffer_size, &buffer_block_size, &temp );
		result = REMOTE_syscall(CONDOR_chdir,working_dir);
	} else {
		result = ::chdir( working_dir );
	}

	if(result<0) _condor_error_retry("Couldn't move to '%s' (%s).  Please fix it.\n",working_dir,strerror(errno));

	dump();
}

/*
Examine a particular fd.  If there is a valid file object on this fd, return 0.  If this is not a valid fd, return -1 and set errno.  If this is a valid fd, but no valid file object, attempt a lazy resume of the file.  If that fails, die with _condor_error_retry.
*/

int CondorFileTable::resume( int fd )
{
	CondorFileInfo *i;
	CondorFilePointer *p;

	if( fd<0 || fd>=length || !pointers[fd] ) {
		errno = EBADF;
		return -1;
	}

	p = pointers[fd];
	i = p->info;

	if( !p->file ) {
		p->file = open_file_unique( i->logical_name, p->flags, 0, fd!=2 );
		if( !p->file ) {
			_condor_error_retry("Couldn't re-open '%s' after a restart.  Please fix it.\n",i->logical_name);
		}
	}

	return 0;
}

int CondorFileTable::map_fd_hack( int fd )
{
	if( resume(fd)<0 ) return -1;
	return pointers[fd]->file->map_fd_hack();
}

int CondorFileTable::local_access_hack( int fd )
{
	if( resume(fd)<0 ) return -1;
	return pointers[fd]->file->local_access_hack();
}



