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

// FIXME: Things labeled with FIXME were changed to graft the buffering
// portion of the new-syscalls branch into the trunk.

#include "condor_common.h"
#include "buffer_cache.h"
#include "condor_debug.h"
#include "condor_macros.h"

// FIXME
#include "condor_syscall_mode.h"
// FIXME
#include "syscall_numbers.h"

/*
Policy:
	read on demand
	write back when more space is needed
	writes to blocks not currently in the cache will fall through

Nomenclature:
	offset   The number of bytes into a file
	order    The number of a block, as it is ordered in a file
	position The number of a block, as it is positioned in the cache
*/

class BlockInfo {
public:
	File	*owner;		// The virtual file this belongs to
	int	order;		// The order of this block in the file
	int	dirty;		// Has this been changed?
	int	last_used;	// Time of last use
};

/*
If we can't allocate a buffer this big, leave the pointer at zero,
and divert all reads and writes directly to access methods.
*/

BufferCache::BufferCache(int b, int bs)
{
	blocks = b;
	block_size = bs;
	buffer = 0;
	info = 0;

	if( (blocks<=0) || (block_size<=0) ) {
		dprintf(D_ALWAYS,"Buffering is disabled\n");
		return;
	}

	buffer = new char[blocks*block_size];
	info = new BlockInfo[blocks];

	if( !buffer || !info ) {
		_condor_file_warning("Condor Warning: Unable to allocate the requested buffer of %d blocks of %d bytes.  Buffering is disabled.\n",blocks,block_size);

		if(info) delete [] info;
		if(buffer) delete [] buffer;

		info = 0;
		buffer = 0;
		return;
	}

	bzero(buffer,blocks*block_size);
	for( int i=0; i<blocks; i++ ) info[i].owner = 0;

	time = 0;
}

BufferCache::~BufferCache()
{
	if(info) delete [] info;
	if(buffer) delete [] buffer;
}

/*
An incomplete or failed write can be a real problem -- it's too late
to return an error code, so we will print a warning here, and see what
happens.

Consider common examples -- what is the best thing to do?
	 File system full
	 Bad permissions
	 Lost connection on socket
*/

int BufferCache::write_block( int position )
{
	int result,fragment;

	if( (position<0) || (position>=blocks) ) return -1;

	// If this is the last block of the file,
	// only write the necessary portion of the file

	// FIXME int file_size = info[position].owner->get_size();
	int file_size = info[position].owner->getSize();
	
	int last_block = file_size / block_size;

	if( info[position].order==last_block ) {
		fragment = file_size % block_size;
	} else {
		fragment = block_size;
	}

	// FIXME result = info[position].owner->write(
	result = REMOTE_syscall( CONDOR_lseekwrite,
		info[position].owner->real_fd,
		info[position].order*block_size,
		SEEK_SET,
		&buffer[position*block_size],
		fragment );

	if(result!=fragment) {
		_condor_file_warning("Unable to write buffered data to file %s! (%s).\n",			
			// FIXME info[position].owner->get_name(),
			info[position].owner->pathname,
			strerror(errno));
	}

	info[position].dirty = 0;

	return fragment;
}

/*
An error committed in a read isn't so bad.
We can return the error to the user.
*/

int BufferCache::read_block( File *owner, int position, int order )
{
	int result;

	if( (position<0) || (position>=blocks) ) return -1;

	// FIXME result = owner->read(order*block_size,&buffer[position*block_size],block_size);
	result = REMOTE_syscall( CONDOR_lseekread, owner->real_fd, order*block_size, SEEK_SET, &buffer[position*block_size],block_size );

	if(result<0) {
		// Error, but buffer not changed
		return result;
	}

	// Clear the remaining part of the block left unread
	bzero(&buffer[position*block_size+result],block_size-result);

	// Update the info block

	info[position].owner = owner;
	info[position].order = order;
	info[position].dirty = 0;
	info[position].last_used = time++;

	return result;
}

/*
Return an unused, or otherwise the least recently used block
position in the cache.  Never fails.
*/

int BufferCache::find_lru_position()
{
	int	best_time=info[0].last_used;
	int	best_block=0;

	for( int i=0; i<blocks; i++ ) {
		if( info[i].owner==0 ) {
			return i;
		}
		if( info[i].last_used < best_time ) {
			best_time = info[i].last_used;
			best_block = i;
		}
	}

	return best_block;
}

/*
Find a block in a particular order in a file.
If it is not in the cache, return -1.  Otherwise, return
the position of the block.
*/

int BufferCache::find_block(File *owner, int order)
{
	for( int i=0; i<blocks; i++ ) {
		if( (info[i].owner==owner) && (info[i].order==order) ) {
			return i;
		}
	}

	return -1;
}

/*
Find an empty place in the cache for a new block.
Return the position of the new block.
*/

int BufferCache::make_room()
{
	int position;

	while(1) {
		position = find_lru_position();
		if(info[position].owner && info[position].dirty) {
			if(write_block(position)!=block_size) {
				// If the block could not be written,
				// put it back in the lru queue
				info[position].last_used = time++;
				continue;
			}
		}
		break;
	}

	return position;
}

/*
Attempt to bring a block into the cache, reading if necessary.
*/

int BufferCache::find_or_load_block(File *owner, int order)
{
	int position = find_block(owner,order);

	if(position!=-1) return position;

	position = make_room();

	// If the read fails, the data and info in the
	// existing block have not been touched.

	if(read_block(owner,position,order)<=0) return -1;

	info[position].owner = owner;
	info[position].order = order;
	info[position].dirty = 0;
	info[position].last_used = time++;

	return position;
}

/*
Read from the buffer cache.
Returns an error or the number of bytes read.
*/

ssize_t BufferCache::read( File *owner, off_t offset, char *data, ssize_t length )
{
	int order,position,chunksize;
	int bytes_read=0;

	// FIXME
	if(!buffer) return REMOTE_syscall( CONDOR_lseekread, owner->real_fd, offset, SEEK_SET, data, length );

	do {
		// Decide what portion of the block to read
		int offset_in_block = offset%block_size;
		if( offset_in_block ) {
			chunksize = MIN(block_size-offset_in_block,length);
		} else {
			chunksize = MIN(length,block_size);
		}

		// Find the block # to begin reading from
		order = offset/block_size;
		position = find_or_load_block(owner,order);
		
		// If the block cannot be found, we are done
		if(position<0) break;

		// Copy the data from the buffer to the user area
		memcpy(data,&buffer[block_size*position+offset_in_block],chunksize );

		// Update the LRU time
		info[position].last_used = time++;

		// Push all the counters forward
		length -= chunksize;
		data += chunksize;
		offset += chunksize;
		bytes_read += chunksize;

	} while(length>0);

	if(bytes_read==0) {
		return -1;
	} else {
		return bytes_read;
	}
}

/*
Write to the buffer cache.
Return the number of bytes written, or an error.
*/

ssize_t BufferCache::write( File *owner, off_t offset, char *data, ssize_t length )
{
	int order,position,chunksize,readmode;
	int bytes_written=0;

	// FIXME
	if(!buffer) return REMOTE_syscall( CONDOR_lseekwrite, owner->real_fd, SEEK_SET, offset, data, length );

	do {
		// Decide what portion of the block to write
		int offset_in_block = offset%block_size;
		if( offset_in_block ) {
			chunksize = MIN( block_size-offset_in_block, length );
		} else {
			chunksize = MIN( length, block_size );
		}

		// Is the needed block in the cache?
		order = offset/block_size;
		position = find_block(owner,order);
		
		if( position!=-1 ) {
			// The block was found
		} else if( (chunksize==block_size) ) {
			// Or, we are writing a whole block
			position = make_room();
		// FIXME } else if((offset_in_block==0)&&(offset>=owner->get_size())) {
		} else if((offset_in_block==0)&&(offset>=owner->getSize())) {
		
			// Or, we are appending in a new block
			position = make_room();
		} else {
			// Or, we must modify a block not in the cache
			position = find_or_load_block(owner,order);
		}

		if( position==-1 ) {

			// This particular chunk is not in the cache,
			// so we are forced to do a write.  We are
			// performing a system call anyway, so we will
			// cut our losses and write the rest of the
			// buffer, excepting any partial last block.

			chunksize = length - (length-chunksize)%block_size;
			// FIXME owner->write(offset,data,chunksize);
			REMOTE_syscall( CONDOR_lseekwrite, owner->real_fd, offset, SEEK_SET, data, chunksize );
			invalidate(owner,offset,chunksize);

		} else {

			memcpy(&buffer[block_size*position+offset_in_block],data,chunksize);

			// Update the block info
			info[position].last_used = time++;
			info[position].dirty = 1;
			info[position].owner = owner;
			info[position].order = order;
		}

		// Push all the counters forward
		length -= chunksize;
		data += chunksize;
		offset += chunksize;
		bytes_written += chunksize;

		// FIXME if( offset>owner->get_size() ) owner->set_size(offset);
		if( offset>owner->getSize() ) owner->setSize(offset);

	} while(length>0);

	if(bytes_written==0) {
		return -1;
	} else {
		return bytes_written;
	}
}

void BufferCache::prefetch( File *f, off_t offset, size_t length )
{
	if( (!f) || (offset<0) || (length<=0) ) return;

	int order = offset/block_size;

	while(length>0) {
		if(find_or_load_block(f,order)<0) break;
		length -= block_size;
		order++;
	}
}

void BufferCache::invalidate( File *owner, off_t offset, size_t length )
{
	int start_block = offset/block_size;
	int end_block = (offset+length)/block_size;

	for( int i=0; i<blocks; i++ ) {

		if( (info[i].owner==owner) &&
		    (info[i].order>=start_block) &&
		    (info[i].order<=end_block) ) {

			info[i].owner = 0;
			info[i].dirty = 0;
		}
	}
		 
}

void BufferCache::flush()
{
	if(!buffer) return;

	dprintf(D_ALWAYS,"Full buffer flush.\n");

	for( int i=0; i<blocks; i++ )
		if( info[i].dirty && info[i].owner )
			write_block(i);
}

void BufferCache::flush( File *owner )
{
	if(!buffer) return;

	for( int i=0; i<blocks; i++ ) {
		if( info[i].owner==owner ) {
			if( info[i].dirty ) {
				write_block(i);
			}
			info[i].owner = 0;
		}
	}

	// When a file is flushed, we need to force
	// a seek because seek pointer bufferring is not
	// going to take place any longer...

	REMOTE_syscall( CONDOR_lseek, owner->real_fd, owner->offset, SEEK_SET );
}


