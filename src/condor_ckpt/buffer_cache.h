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

#ifndef BUFFER_CACHE_H
#define BUFFER_CACHE_H

#include "condor_common.h"
// FIXME #include "file_types.h"
#include "file_state.h"

class BlockInfo;

/**
This class buffers fixed-sized blocks of data for all virtual
files in a process.  When a read or writeback is necessary,
the appropriate I/O method is called using a pointer to
a File object.
*/

class BufferCache {
public:

	BufferCache( int blocks, int block_size );
	~BufferCache();

	/** read data from a file at a particular offset.  Take
	advantage of the buffer cache if possible.  Reading past
	the length of a file returns data of all zeroes. */

	ssize_t read( File *f, off_t offset, char *data, ssize_t length );

	/** write data to a file at a particular offset.  If the appropriate
	blocks are in the buffer, write to the buffer and write back when
	convenient.  If the appropriate blocks are not in the buffer,
	then write through. */

	ssize_t write( File *f, off_t offset, char *data, ssize_t length );

	/** Force the buffer to preload data from a particular range.
	    We would like to do all prefetching as one gigantic read
	    which fills the buffer -- this method may cause some
	    writebacks in order to create one unbroken region
	    in the buffer.  */

	void prefetch( File *f, off_t offset, size_t length );

	/** Invalidate all blocks owned by this file, after flushing
	any dirty blocks. */

	void flush( File *f );

	/** Flush all dirty blocks in the cache. Do not invalidate
	clean blocks. */

	void flush();

private:

	void	invalidate( File *owner, off_t offset, size_t length );
	int	find_block( File *owner, int order );
	int	find_or_load_block( File *owner, int order );
	int	find_lru_position();
	int	make_room();

	int	write_block( int position );
	int	read_block( File *owner, int position, int order );

	int		blocks;		// Number of blocks in this object
	int		block_size;	// Size of a block, in bytes
	char		*buffer;	// The buffer data
	BlockInfo	*info;		// Info about each block
	int		time;		// Integer time for lru
};

#endif
