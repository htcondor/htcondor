#ifndef CONDOR_FILE_BUFFER_H
#define CONDOR_FILE_BUFFER_H

#include "condor_file.h"

class CondorChunk;

/**
This class buffers reads and writes by keeping variable-sized
buffers of data.  The minumum chunk size to read is determined
by OpenFileTable::get_buffer_block_size, and the maximum amount
of data to store is determined by OpenFileTable::get_buffer_size.
<p>
This object is simply wrapped around an existing file:
<pre>
CondorFile *original = new CondorFileRemote;
CondorFile *faster = new CondorFileBuffer(original);

faster->open(...);
faster->read(...);
...
</pre>
*/

class CondorFileBuffer : public CondorFile {
public:
	CondorFileBuffer( CondorFile *original );
	virtual ~CondorFileBuffer();

	virtual void dump();

	virtual int open(const char *path, int flags, int mode);
	virtual int close();
	virtual int read(int offset, char *data, int length);
	virtual int write(int offset, char *data, int length);

	virtual int fcntl( int cmd, int arg );
	virtual int ioctl( int cmd, int arg );
	virtual int ftruncate( size_t length ); 
	virtual int fsync();

	virtual void checkpoint();
	virtual void suspend();
	virtual void resume(int count);

	virtual int map_fd_hack();
	virtual int local_access_hack();

private:
	void trim();
	void flush( int deallocate );
	void evict( CondorChunk *c );
	void clean( CondorChunk *c );
	double benefit_cost( CondorChunk *c );

	// The raw file being buffered
	CondorFile *original;

	// A linked list of data chunks
	CondorChunk *head;

	// The integer time used for LRU.
	int time;
};

#endif
