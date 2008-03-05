/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "image.h"

#include "condor_common.h"
#include "condor_syscalls.h"
#include "condor_sys.h"
#include "condor_syscall_mode.h"

#include "condor_debug.h"

#include "file_state.h"
#include "file_table_interf.h"
#include "condor_file.h"
#include "condor_file_local.h"
#include "condor_file_fd.h"
#include "condor_file_remote.h"
#include "condor_file_special.h"
#include "condor_file_agent.h"
#include "condor_file_buffer.h"
#include "condor_file_compress.h"
#include "condor_file_append.h"
#include "condor_file_nest.h"
#include "condor_error.h"

#include <stdarg.h>
#include <sys/errno.h>

/* These are the remote system calls we use in this file */
extern "C" int REMOTE_CONDOR_report_file_info_new(char *name, 
	long long open_count, long long read_count, long long write_count,
	long long seek_count, long long read_bytes, long long write_bytes);
extern "C" int REMOTE_CONDOR_get_file_info_new(char *logical_name,
	char *&actual_url);
extern "C" int REMOTE_CONDOR_get_buffer_info(int *bytes, int *block_size, 
	int *prefetch_size);
extern "C" int REMOTE_CONDOR_chdir(const char *path);

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
		info_name = strdup(n);
		open_count = 0;
		read_count = read_bytes = 0;
		write_count = write_bytes = 0;
		seek_count = 0;
		already_warned = 0;
		next = 0;
	}
	~CondorFileInfo() {
		free( info_name );
	}

	void	report() {
		if( RemoteSysCalls() ) {
			REMOTE_CONDOR_report_file_info_new( info_name, open_count, 
				read_count, write_count, seek_count, read_bytes, write_bytes );
		}
	}

	// IMPORTANT NOTE:
	// This name is not necessarily the same as the logical
	// name of the file with which it is associated.  After
	// a certain number of files are opened, one block
	// named "everything else" is shared between all open files.

	char	*info_name;

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
	CondorFilePointer( CondorFile *f, CondorFileInfo *i, int fl, char *n ) {
		file = f;
		info = i;
		flags = fl;
		offset = 0;
		logical_name = strdup( n );
	}
	~CondorFilePointer() {
		free( logical_name );
	}

	CondorFile *file;
	CondorFileInfo *info;
	int flags;
	off_t offset;
	char *logical_name;
};

/*
We register this function to be the last thing executed before a job exits.
Here, we call fflush() to force all the stio buffers into the Condor buffers,
and then close all the files.  We are obliged to close all of our files manually,
because several file types have commit semantics that only force data when a file
Anything output after this point will be lost, so users relying on output from
global destructors will be disappointed.
*/

static void _condor_file_table_shutdown()
{
	fflush(0);
	FileTab->close_all();
	FileTab->report_all();
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
	buffer_size = 0;
	buffer_block_size = 0;
	info_count = 0;
	aggravate_mode = 0;

	int scm = SetSyscalls( SYS_UNMAPPED | SYS_LOCAL );
	length = sysconf(_SC_OPEN_MAX);
	SetSyscalls(scm);

	pointers = new CondorFilePointer *[length];
	if(!pointers) {
		_condor_error_retry("Out of memory!\n");
	}

	for( int i=0; i<length; i++ ) {
		pointers[i]=0;
	}

	info_head = 0;

	pointers[0] = new CondorFilePointer(0,make_info("default stdin"),O_RDONLY,"default stdin");
	pointers[1] = new CondorFilePointer(0,make_info("default stdout"),O_WRONLY,"default stdout");
	pointers[2] = new CondorFilePointer(0,make_info("default stderr"),O_WRONLY,"default stderr");

	working_dir = strdup(".");

	atexit( _condor_file_table_shutdown );
}

void CondorFileTable::flush()
{
	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			if( pointers[i]->file ) {
				pointers[i]->file->flush();
			}
		}
	}
}

void CondorFileTable::report_all()
{
	CondorFileInfo *i;

	for( i=info_head; i; i=i->next ) {
		i->report();
	}
}

void CondorFileTable::close_all()
{
	for( int i=0; i<length; i++ ) {
		close(i);
	}
}

void CondorFileTable::set_aggravate_mode( int on_off )
{
	aggravate_mode = on_off;
}

void CondorFileTable::dump()
{
	CondorFilePointer *p;

	dprintf(D_ALWAYS,"\nOPEN FILE TABLE:\n");

	for( int j=0; j<length; j++ ) {

		p = pointers[j];
		if( p ) {
			dprintf(D_ALWAYS,"fd %d\n",j);
			dprintf(D_ALWAYS,"\tlogical name: %s\n",p->logical_name);
			dprintf(D_ALWAYS,"\toffset:       %d\n",p->offset);
			dprintf(D_ALWAYS,"\tdups:         %d\n",count_pointer_uses(p));
			dprintf(D_ALWAYS,"\topen flags:   0x%x\n",p->flags);
			if( p->file ) {
				dprintf(D_ALWAYS,"\turl:          %s\n",p->file->get_url());
				dprintf(D_ALWAYS,"\tsize:         %d\n",p->file->get_size());
				dprintf(D_ALWAYS,"\topens:        %d\n",count_file_uses(p->file));
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

int CondorFileTable::find_logical_name( const char *logical_name )
{
	for( int i=0; i<length; i++ )
		if( pointers[i] )
			if( !strcmp(pointers[i]->logical_name,logical_name) )
				return i;

	return -1;
}

/*
If there already exists an info block for this name, re-use it.
Otherwise, make and return a new one.  If more than 100 files
have been opened, stop making info blocks (i.e. leaking memory,)
and dump all of the rest into "everything else".
*/

CondorFileInfo * CondorFileTable::make_info( char *logical_name )
{
	CondorFileInfo *i;

	for( i=info_head; i; i=i->next )
		if( !strcmp(logical_name,i->info_name) )
			return i;

	if( info_count<100 ) {
		i = new CondorFileInfo( logical_name );
		i->next = info_head;
		info_head = i;
	} else if( info_count==100 ) {
		i = new CondorFileInfo( "everything else" );
		i->next = info_head;
		info_head = i;
	} else {
		/* fall through */
	}

	info_count++;

	return info_head;
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
Convert a logical file name into a physical url by any and all methods available.  This function will not fail -- it will always fall back on something reasonable.  This is a private function which expects an already compelted path name.
*/

void CondorFileTable::lookup_url( char *logical_name, char **url )
{
	static int never_ask_shadow = FALSE;

	// Special case: Requests to look up default standard files
	// should be mapped to constant file descriptors.
 
	if(!strcmp(logical_name,"default stdin")) {
		*url = strdup("fd:0");
		return;
	} else if(!strcmp(logical_name,"default stdout")) {
		*url = strdup("fd:1");
		return;
	} else if(!strcmp(logical_name,"default stderr")) {
		*url = strdup("fd:2");
		return;
	}

	// If in local mode, just use local:logical_name.
	// Otherwise, ask shadow.  If that fails, try buffer:remote:logical_name.

	if( LocalSysCalls() || (never_ask_shadow == TRUE)) {
		*url = (char *)malloc(strlen(logical_name)+7);
		sprintf(*url,"local:%s",logical_name);
	} else {
		if (never_ask_shadow == FALSE ) {
			int result;

			// If <0 is returned, then an error happened.

			// If 0 is returned then the shadow wanted the FileTable to work
			// just like normal in comming back to it for each file the
			// FileTable wants to know what to do with.

			// if 1 is returned, then the shadow automatically give up 
			// control of how the files are opened, permanently I assume,
			// since the static variable in here is preserved across
			// checkpoints in the memory image. 
			result = REMOTE_CONDOR_get_file_info_new( logical_name, *url );

			if (result == 1) {
				never_ask_shadow = TRUE;
			} else if( result < 0 ) {
				free( *url );
				*url = (char *)malloc(strlen(logical_name)+8);
				sprintf(*url,"buffer:remote:%s",logical_name);
			} 

			if(!got_buffer_info) {
				int temp;
				REMOTE_CONDOR_get_buffer_info( &buffer_size, 
												&buffer_block_size, &temp );
				got_buffer_info = 1;
			}
		}
	}
}

/*
Convert a nested url into a url chain.  For example, compress:fetch:remote:/tmp/file
becomes CondorFileCompress( CondorFileFetch( CondorFileRemote ) )
*/

CondorFile * CondorFileTable::create_url_chain( char const *url )
{
	char *method = (char *)malloc(strlen(url)+1);
	char *rest = (char *)malloc(strlen(url)+1);

	CondorFile * result = create_url_chain( url, method, rest );

	free( method );
	free( rest );

	return result;
}

CondorFile * CondorFileTable::create_url_chain( char const *url, char *method, char *rest )
{
	char *next;
	CondorFile *f;


	/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't
		work with 8bit numbers */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	int fields = sscanf( url, "%[^:]:%[\x1-\x7F]", method, rest );
	#else
	int fields = sscanf( url, "%[^:]:%[\x1-\xFF]", method, rest );
	#endif

	if( fields!=2 ) return 0;

	/* Options interpreted by each layer go in () following the : */
	/* If we encounter that here, skip it. */

	next = rest;
	if( *next=='(' ) {
		while(*next) {
			next++;
			if(*next==')') {
				next++;
				break;
			}
		}
	}

	/* Now examine the method. */

	if( !strcmp( method, "local" ) ) {
		return new CondorFileLocal;
	} else if( !strcmp( method, "fd" ) )  {
		return new CondorFileFD;
	} else if( !strcmp( method, "special" ) ) {
		return new CondorFileSpecial;
	} else if( !strcmp( method, "remote" ) ) {
		return new CondorFileRemote;

#ifdef ENABLE_NEST
	} else if( !strcmp( method, "nest" ) ) {
		return new CondorFileNest;
#endif

	} else if( !strcmp( method, "buffer" ) ) {
		f = create_url_chain( next );
		if(f) return new CondorFileBuffer( f, buffer_size, buffer_block_size );
		else return 0;
	} else if( !strcmp( method, "fetch" ) ) {
		f = create_url_chain( next );
		if(f) return new CondorFileAgent( f );
		else return 0;
	} else if( !strcmp( method, "compress" ) ) {
		f = create_url_chain( next );
		if(f) return new CondorFileCompress( f );
		else return 0;
	} else if( !strcmp( method, "append" ) ) {
		f = create_url_chain( next );
		if(f) return new CondorFileAppend( f );
		else return 0;
	} else {
		_condor_warning(CONDOR_WARNING_KIND_BADURL,"I don't understand url (%s).",url);
		errno = ENOENT;
		return 0;
	}
}

/* Given a url, create and open a url chain. */

CondorFile * CondorFileTable::open_url( char const *url, int flags, int mode )
{
	CondorFile *f = create_url_chain( url );

	if( !f ) {
		_condor_warning(CONDOR_WARNING_KIND_BADURL,"I can't parse url type (%s).",url);
		errno = ENOENT;
		return 0;
	} else {
		if( f->open(url,flags,mode)>=0 ) {
			return f;
		} else {
			delete f;
			return 0;
		}
	}
}

/* Open a url.  If the open fails, fall back on buffered remote system calls. */

CondorFile * CondorFileTable::open_url_retry( char const *url, int flags, int mode )
{
	CondorFile *f;

	f = open_url( url, flags, mode );
	if(f) return f;

	if( RemoteSysCalls() ) {
		char *path;

		if( strstr(url,"remote:") ) return 0;

		path = strrchr(url,':');
		if(!path) return 0;

		path++;
		if(!*path) return 0;

		char *new_url = (char *)malloc(strlen(path)+15);
		sprintf(new_url,"buffer:remote:%s",path);
		CondorFile *result = open_url( new_url, flags, mode );
		free( new_url );
		return result;
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
Find any pointers to one file and replace it with another.
*/

void CondorFileTable::replace_file( CondorFile *old_file, CondorFile *new_file )
{
	int i;

	for( i=0;i<length; i++ ) {
		if(pointers[i] && pointers[i]->file==old_file) {
			pointers[i]->file=new_file;
		}
	}
}


/*
Create and open a file object from a logical name.  If either the logical
name or url are already in the file table AND the file was previously
opened with compatible flags, then share the object.  If not, elevate
the flags to a common denominator and open a new object suitable for
both purposes.
*/

CondorFile * CondorFileTable::open_file_unique( char *logical_name, int flags, int mode )
{

	int match = find_logical_name( logical_name );
	if( match==-1 || !pointers[match]->file ) {
		char *url = NULL;
		lookup_url( (char*)logical_name, &url );
		if( url ) {
			match = find_url( url );
			if( match==-1 ) {
				CondorFile *f = open_url_retry( url, flags, mode );
				free( url );
				return f;
			}
			free( url );
		}
	}

	CondorFilePointer *p = pointers[match];
	CondorFile *old_file;

	if( needs_reopen( p->flags, flags ) ) {
		flags &= ~(O_RDONLY);
		flags &= ~(O_WRONLY);
		flags |= O_RDWR;

		p->file->flush();

		CondorFile *f = open_url_retry( p->file->get_url(), flags, mode );
		if(!f) return 0;

		old_file = p->file;
		if( old_file->close()!=0 ) return 0;

		replace_file( old_file, f );

		/* Even though old_file is aliasing p->file here, and
			then we're deleting old_file, There is no
			returning a freed pointer error at the end of the
			function because replace_file() will *always*
			be able to replace the entry in the pointers[]
			table (aliased by p) indexed with old_file (being
			actually p->file) with the updated value of f.
		*/

		delete old_file;
	}

	return p->file;
}

/*
If short_path is an absolute path, copy it to full path.
Otherwise, tack the current directory on to the front
of short_path, and copy it to full_path.
*/
 
void CondorFileTable::complete_path( const char *short_path, char **full_path )
{
        if(short_path[0]=='/') {
                *full_path = strdup(short_path);
        } else {
                *full_path = (char *)malloc(strlen(working_dir)+strlen(short_path)+2);
				strcpy(*full_path,working_dir);
                strcat(*full_path,"/");
                strcat(*full_path,short_path);
        }
}

int CondorFileTable::open( const char *logical_name, int flags, int mode )
{
	char *full_logical_name = NULL;

	CondorFile *file=0;
	CondorFileInfo *info=0;

	// Convert a relative path into a complete path

	complete_path( logical_name, &full_logical_name );

	// Find a fresh file descriptor

	int fd = find_empty();
	if(fd<0) {
		errno = EMFILE;
		free( full_logical_name );
		return -1;
	}

	file = open_file_unique( full_logical_name, flags, mode );
	if(!file) {
		free( full_logical_name );
		return -1;
	}

	info = make_info( full_logical_name );
	info->open_count++;

	// Flags that should be applied once only are now removed

	flags &= ~(O_TRUNC);
	flags &= ~(O_CREAT);
	
	// Install the pointer and return!

	pointers[fd] = new CondorFilePointer(file,info,flags,full_logical_name);
	free( full_logical_name );
	return fd;
}

int CondorFileTable::install_special( int real_fd, char *kind )
{
	int fd = find_empty();
	if(fd<0) return -1;

	CondorFileInfo *i= make_info(kind);
	i->already_warned = 1;
	i->open_count++;

	CondorFileSpecial *s = new CondorFileSpecial();
	s->attach( real_fd );

	pointers[fd] = new CondorFilePointer( s, i, O_RDWR, kind );

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
	int result = ::pipe(real_fds);
	SetSyscalls(scm);

	if(result<0) return -1;

	fds[0] = install_special(real_fds[0],"pipes");
	fds[1] = install_special(real_fds[1],"pipes");

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

	return install_special(real_fd,"network connections");
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
	//CondorFileInfo *i = p->info;
	CondorFile *f = p->file;

	// If this is the last use of the file, flush, close, and delete.
	// If the close fails (yes, possible!) then return the error
	// but do not adjust the table.

	if(f && count_file_uses(f)==1) {

		/* Commented out this next line to prevent eager updates
			to the shadow of file statistics on a file
			close. This is to make WantRemoteIO function
			better when you say False. At checkpoint
			boundaries and the end of the job will this
			information get sent (by other parts of the
			codebase), so it isn't lost. AFAICT, this
			commenting out doesn't hurt anything, but I'll
			leave the line of code in in case we ever want
			to restore eager reporting and or make it do
			something better when remote io is turned off
			for a job inside of Condor. */
/*		i->report(); */

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

	// XXX Don't fix the warning on this line. There is a semantic breakdown
	// between char* and const void* in the file table API itself, and
	// between that and the UNIX system API. It will take a pretty serious
	// rewrite of the function signatures to make everything conform, and
	// there is a chance there is no such conforming API.
	int actual = f->write( fp->offset, (char*)data, nbyte );
	
	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Special case: always flush data on stderr
	if(fd==2) f->flush();

	// Update the offset by the amount written
	fp->offset += actual;

	// Record that a write was done
	fp->info->write_count++;
	fp->info->write_bytes+=actual;

	// Perhaps send a warning message
	check_safety(fp);

	// Return the number of bytes written
	return actual;
}

void CondorFileTable::check_safety( CondorFilePointer *fp )
{
	if( fp->info->read_bytes && fp->info->write_bytes && !fp->info->already_warned ) {
		fp->info->already_warned = 1;
		_condor_warning(CONDOR_WARNING_KIND_READWRITE,"File '%s' used for both reading and writing.  This is not checkpoint-safe.\n",fp->logical_name);
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

	// This error could conceivably be simply a warning and return with errno=ESPIPE.
	// Despite this, most programs assume that a seek performed on a regular file
	// will succeed and do not check the error code.  So, this must be a fatal error.

	if( !f->is_seekable() ) {
		_condor_error_fatal("Seek operation attempted on non-seekable file %s.",f->get_url());
	}

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
		fd, pointers[fd]->logical_name );

	return chdir( pointers[fd]->logical_name );
}

/*
chdir() works by attempting the chdir remotely, and then
storing the working directory text.  If this is a relative
path, we just tack it on to the given path.  We have
to be careful never to do a getcwd(), because this is not
safe on auto-mounted NFS systems.
*/

int CondorFileTable::chdir( const char *path )
{
	int result;
	char *nmem = NULL;

	if( MyImage.GetMode() != STANDALONE ) {
		result = REMOTE_CONDOR_chdir( path );
	} else {
		result = syscall( SYS_chdir, path );
	}

	if( result==-1 ) return result;

	if( path[0]=='/' ) {
		/* since path might in fact be identical to working_dir, let's be
			careful and copy before freeing. */
		nmem = strdup( path );
		free( working_dir );
		working_dir = nmem;
	} else {
		nmem = (char *)realloc(working_dir,strlen(path)+strlen(working_dir)+2);
		if (nmem == NULL) {
			/* I wonder what happens if the actual chidir succeeded and I fail
				to get memory here and return. If the user program ignores
				the errno, I'm sure problems will arise since working_dir is
				now unsynchronized with the actual chdir.
			*/
			errno = ENOMEM;
			return -1;
		}
		/* if working_dir's pointer stayed the same, this is a nop, if it
			changed, then realloc() already freed() the memory and returned
			the new pointer, which I update here. */
		working_dir = nmem;
		strcat( working_dir, "/" );
		strcat( working_dir, path );
	}

	return result;
}

/*
ioctls don't affect the open file table, so we will pass them
along to the individual access method, which will decide
if it can be supported.
*/

int CondorFileTable::ioctl( int fd, int cmd, long arg )
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

	if( resume(fd)<0 ) return -1;

	switch(cmd) {
		#ifdef F_DUPFD
		case F_DUPFD:
			return search_dup2(fd,arg);
			break;
		#endif

		#ifdef F_DUP2FD
		case F_DUP2FD:
			return dup2(fd,arg);
			break;
		#endif

		/*
			A length of zero to FREESP indicates the file
			should be truncated at the start value.

			Truncation must be handled at this level, and
			not at an individual object, because the entire
			chain of objects, e.g.
			CFBuffer->CFCompress->CFRemote, must all be
			informed about a truncate in turn. 
		*/
		#ifdef F_FREESP
		case F_FREESP:
			{
				struct flock *f = (struct flock *)arg;

				if( (f->l_whence==0) && (f->l_len==0) ) {
					return FileTab->ftruncate(fd,f->l_start);
				}
				return pointers[fd]->file->fcntl(cmd,arg);
			}
			break;
		#endif

		#ifdef F_FREESP64
		case F_FREESP64:
			{
				struct flock64 *f64 = (struct flock64 *)arg;

				if( (f64->l_whence==0) && (f64->l_len==0) ) {
					return FileTab->ftruncate(fd,f64->l_start);
				}

				return pointers[fd]->file->fcntl(cmd,arg);
			}
			break;
		#endif

		default:
			return pointers[fd]->file->fcntl(cmd,arg);
			break;
	}
}

int CondorFileTable::fsync( int fd )
{
	if( resume(fd)<0 ) return -1;
	return pointers[fd]->file->fsync();
}

int CondorFileTable::fstat( int fd, struct stat *buf)
{
	if( resume(fd)<0 ) return -1;
	return pointers[fd]->file->fstat(buf);
}

int CondorFileTable::stat( const char *name, struct stat *buf)
{
	int fd;
	int scm, oscm;
	int ret;
	int do_local;

	/* See if the file is open, if so, then just do an fstat. If it isn't open
		then call the normal stat in unmapped mode with whatever the shadow
		said the location of the file was. */
	fd = find_logical_name( name );
	if (fd == -1)
	{
		oscm = scm = GetSyscallMode();

		/* determine where the shadow says to deal with this file. */
		char *newname = NULL;
		do_local = _condor_is_file_name_local( name, &newname );
		if (LocalSysCalls() || do_local) {
			scm |= SYS_LOCAL;		/* turn on SYS_LOCAL */
			scm &= ~(SYS_REMOTE);	/* turn off SYS_REMOTE */
		}
		else {
			scm |= SYS_REMOTE;		/* Turn on SYS_REMOTE */
			scm &= ~(SYS_LOCAL);	/* Turn off SYS_LOCAL */
		}

		/* either way, it needs to be unmapped so we don't recurse into here */
		scm |= SYS_UNMAPPED; /* turn on SYS_UNMAPPED */
		scm &= ~(SYS_MAPPED); /* turn off SYS_MAPPED */

		/* invoke it with where the shadow said the file was */
		SetSyscalls(scm);
		ret = ::stat(newname, buf);
		SetSyscalls(oscm); /* set it back to what it was */

		free( newname );

		return ret;
	}
	return fstat(fd, buf);
}

int CondorFileTable::lstat( const char *name, struct stat *buf)
{
	int fd;
	int scm, oscm;
	int ret;
	int do_local;

	/* See if the file is open, if so, then just do an fstat. If it isn't open
		then call the normal lstat in unmapped mode with whatever the shadow
		said the location of the file was. */
	fd = find_logical_name( name );
	if (fd == -1)
	{
		oscm = scm = GetSyscallMode();

		/* determine where the shadow says to deal with this file. */
		char *newname = NULL;
		do_local = _condor_is_file_name_local( name, &newname );
		if (LocalSysCalls() || do_local) {
			scm |= SYS_LOCAL;		/* turn on SYS_LOCAL */
			scm &= ~(SYS_REMOTE);	/* turn off SYS_REMOTE */
		}
		else {
			scm |= SYS_REMOTE;		/* Turn on SYS_REMOTE */
			scm &= ~(SYS_LOCAL);	/* Turn off SYS_LOCAL */
		}

		/* either way, it needs to be unmapped so we don't recurse into here */
		scm |= SYS_UNMAPPED; /* turn on SYS_UNMAPPED */
		scm &= ~(SYS_MAPPED); /* turn off SYS_MAPPED */

		SetSyscalls(scm);
		ret = ::lstat(newname, buf);
		SetSyscalls(oscm); /* set it back to what it was */

		free( newname );

		return ret;
	}
	return fstat(fd, buf);
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
		if(_condor_is_fd_local(fds[i].fd)) {
			realfds[i].fd = _condor_get_unmapped_fd(fds[i].fd);
			realfds[i].events = fds[i].events;
		} else {
			_condor_warning(CONDOR_WARNING_KIND_UNSUP,"poll() is not supported for remote files");
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
		fd_real = _condor_get_unmapped_fd(fd);
		if( fd_real>=0 ) {
			if( _condor_is_fd_local(fd) ) {
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
					_condor_warning(CONDOR_WARNING_KIND_UNSUP,"select() is not supported for remote files.");
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
		fd_real = _condor_get_unmapped_fd(fd);
		if( fd_real>=0 && _condor_is_fd_local(fd) ) {
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

/*
Checkpoint the state of the file table.  This involves reporting the disposition of all files, and flushing and closing them.  The closed files will be re-bound and re-opened lazily after a resume.  Notice that these need to be closed even for a periodic checkpoint, because logical names must be re-bound to physical urls.  If a close fails at this point, we must roll-back, otherwise the data on disk would not be consistent with the checkpoint image.
*/

void CondorFileTable::checkpoint()
{
	int temp;

	dprintf(D_ALWAYS,"CondorFileTable::checkpoint\n");

	dump();

	if( MyImage.GetMode() != STANDALONE ) {
		REMOTE_CONDOR_get_buffer_info( &buffer_size, &buffer_block_size, 
			&temp );
	}
	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
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

	report_all();
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
		REMOTE_CONDOR_get_buffer_info( &buffer_size, &buffer_block_size, 
			&temp );
		result = REMOTE_CONDOR_chdir(working_dir);
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
	CondorFilePointer *p;

	if( fd<0 || fd>=length || !pointers[fd] ) {
		errno = EBADF;
		return -1;
	}

	p = pointers[fd];

	if( !p->file ) {
		p->file = open_file_unique( p->logical_name, p->flags, 0 );
		if( !p->file ) {
			_condor_error_retry("Couldn't re-open '%s' after a restart.  Please fix it.\n",p->logical_name);
		}
	}

	return 0;
}

int CondorFileTable::get_unmapped_fd( int fd )
{
	if( resume(fd)<0 ) return -1;
	return pointers[fd]->file->get_unmapped_fd();
}

int CondorFileTable::is_fd_local( int fd )
{
	if( resume(fd)<0 ) return -1;
	return pointers[fd]->file->is_file_local();
}

int CondorFileTable::is_file_name_local( const char *incomplete_name, char **local_name )
{
	char *url = NULL;
	char *logical_name = NULL;

	assert( local_name != NULL && *local_name == NULL );

	// Convert the incomplete name into a complete path
	complete_path( incomplete_name, &logical_name );

	// Now look to see if the file is already open.
	// If it is, ask the file if it is local.

	int match = find_logical_name( logical_name );
	if(match!=-1) {
		CondorFile *file;
		resume(match);
		file = pointers[match]->file;
		*local_name = strdup(strrchr(file->get_url(),':')+1); 
		free( logical_name );
		return file->is_file_local();
	}

	// Otherwise, resolve the url by normal methods.
	lookup_url( logical_name, &url );
	free( logical_name );

	// Copy the local path name out
	*local_name = strdup(strrchr(url,':')+1);

	// Create a URL chain and ask it if it is local.
	CondorFile *f = create_url_chain(url);
	free( url );
	if(f) {
		if( f->is_file_local() ) {
			delete f;
			return 1;
		} else {
			delete f;
			return 0;
		}
	}
	return 0;
}



