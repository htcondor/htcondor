#ifndef CONDOR_FILE_H
#define CONDOR_FILE_H

#include "condor_common.h"

/**
CondorFile is a completely virtual class which defines that operations
that a storage medium must implement.  Actual methods for accessing
a file (local, remote, ioserver, etc.) are built by extending
CondorFile.
<p>
<pre>
                     CondorFile
                      |      | 
                      V      V
        CondorFileBasic     CondorFileBuffer
         |         |
         V         V
CondorFileRemote  CondorFileLocal
                   |        |
                   V        V
          CondorFileAgent CondorFileSpecial
</pre>
<p>
The basic file operations - open, close, read, and write - are
expected to be implemented for any sort of file, whether it be local,
remote, or some exotic storage method such as SRB.  These are
implemented in a descendant of CondorFile.  This allows FileTab->read()
to translate into whatever code is appropriate for the file.
<p>
Exotic operations such as fstatfs() are only supported by the old
method -- find the fd with _condor_file_table_map(), and attempt
the syscall.  If an exotic operation needs to do more than simply
get the fd of the file in question, then the operation needs to
be routed through CondorFileTable.
<p>
*/

class CondorFile {
public:
	CondorFile();
	virtual ~CondorFile();

	virtual int open( const char *path, int flags, int mode )=0;
	virtual int close()=0;
	virtual int read( int offset, char *data, int length )=0;
	virtual int write( int offset, char *data, int length )=0;

	virtual int fcntl( int cmd, int arg )=0;
	virtual int ioctl( int cmd, int arg )=0;
	virtual int ftruncate( size_t length )=0; 
	virtual int fsync()=0;

	virtual void checkpoint()=0;
	virtual void suspend()=0;
	virtual void resume( int resume_count )=0;

	virtual int	is_readable()=0;
	virtual int	is_writeable()=0;
	virtual void	set_size(size_t size)=0;
	virtual int	get_size()=0;
	virtual char	*get_kind()=0;
	virtual char	*get_name()=0;

	/**
	Without performing an actual open, associate this
	object with an existing fd, and mark it readable or writable
	as indicated.
	*/

	virtual int attach( int fd, char *name, int readable, int writable )=0;

	/**
	Return the real fd associated with this file.
	Returns -1 if the mapping is not trivial.
	*/

	virtual int map_fd_hack()=0;

	/**
	Returns true if this file can be accessed by
	referring to the fd locally.  Returns false
	otherwise.
	*/

	virtual int local_access_hack()=0;
};

#endif
