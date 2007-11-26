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


#include "condor_file_buffer.h"
#include "condor_error.h"
#include "file_state.h"

/*
A CondorChunk records a variable-sized piece of data in a buffer.
A linked list of CondorChunks is kept as the recently-used data.
*/

class CondorChunk {
public:
	CondorChunk( int b, int s, int m) {
		begin = b;
		size = s;
		last_used = 0;
		dirty = 0;
		next = 0;
		data = new char[size];
		max_size = m;
	}

	~CondorChunk() {
		delete [] data;
	}

	int begin;
	int size;
	int max_size;
	int last_used;
	char *data;
	int dirty;
	CondorChunk *next;
};

/*
Take two CondorChunks and combine them into one.
*/

static CondorChunk * combine( CondorChunk *a, CondorChunk *b )
{
	int begin, end;
	CondorChunk *r;

	begin = MIN(a->begin,b->begin);
	end = MAX(a->begin+a->size,b->begin+b->size);

	r = new CondorChunk(begin,end-begin,a->max_size);

	r->dirty = a->dirty || b->dirty;
	r->last_used = MAX( a->last_used, b->last_used );

	if( a->dirty && !b->dirty ) {
		memcpy( &r->data[b->begin-begin], b->data, b->size );
		memcpy( &r->data[a->begin-begin], a->data, a->size );
	} else {
		memcpy( &r->data[a->begin-begin], a->data, a->size );
		memcpy( &r->data[b->begin-begin], b->data, b->size );
	}

	delete a;
	delete b;

	return r;
}

/*
Return true if two chunks overlap and _must_ be combined
*/

static int overlaps( CondorChunk *a, CondorChunk *b ) 
{
	return
		(
			(a->begin>=b->begin) &&
			(a->begin<(b->begin+b->size))
		) || (
			(b->begin>=a->begin) &&
			(b->begin<(a->begin+a->size))
		);
}

/*
Return true if two chunks are adjacent and _could_ be combined
*/

static int adjacent( CondorChunk *a, CondorChunk *b )
{
	return
		( (a->begin+a->size)==b->begin )
		||
		( (b->begin+b->size)==a->begin )
		;
}

/*
Return true if this chunk contains this integer offset
*/

static int contains( CondorChunk *c, int position )
{
	return
		(c->begin<=position)
		&&
		((c->begin+c->size)>position)
		;
}

/*
Return true if these two chunks _ought_ to be combined.
Overlapping chunks _must_ be combined, and combining is
only a win for adjacent dirty blocks if the resulting block
is smaller than the desired block size.
*/

static int should_combine( CondorChunk *a, CondorChunk *b )
{
	return
		overlaps(a,b)
		||
		(	a->dirty
			&&
			b->dirty
			&&
			( (a->size+b->size) <= a->max_size )
			&&
			adjacent(a,b)
		)
		;
}

/*
Take a list of chunks and add an additional chunk, either by absorbing
and combining, or by simply appending the chunk to the list.
*/

static CondorChunk * absorb( CondorChunk *head, CondorChunk *c )
{
	CondorChunk *next, *combined;

	if(!head) return c;

	if( should_combine( head, c ) ) {
		next = head->next;
		combined = combine( head, c );
		return absorb( next, combined );
	} else {
		head->next = absorb( head->next, c );
		return head;
	}
}

/*
Most of the operations to be done on a buffer simply involve
passing the operation on to the original file.
*/

CondorFileBuffer::CondorFileBuffer( CondorFile *o, int bs, int bbs )
{
	original = o;
	head = 0;
	time = 0;
	size = 0;
	buffer_size = bs;
	buffer_block_size = bbs;
	url = NULL;
}

CondorFileBuffer::~CondorFileBuffer()
{
	delete original;
	free( url );
}

int CondorFileBuffer::open( const char *url_in, int flags, int mode )
{
	char *junk = (char *)malloc(strlen(url_in)+1);
	char *sub_url = (char *)malloc(strlen(url_in)+1);

	int result;

	free( url );
	url = strdup(url_in);
	
	/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't
		work with 8bit numbers */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	result = sscanf( url, "%[^:]:%[\x1-\x7F]", junk, sub_url );
	#else
	result = sscanf( url, "%[^:]:%[\x1-\xFF]", junk, sub_url );
	#endif
	free( junk );
	junk = NULL;

	if(result!=2) {
		_condor_warning(CONDOR_WARNING_KIND_BADURL, "Couldn't understand url '%s'",url_in);
		free(sub_url);
		errno = EINVAL;
		return -1;
	}

	if(sub_url[0]=='(') {
		char *path = (char *)malloc(strlen(sub_url)+1);

		/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't
			work with 8bit numbers */
		#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
		result = sscanf(sub_url,"(%d,%d)%[\x1-\x7F]",&buffer_size,&buffer_block_size,path);
		#else
		result = sscanf(sub_url,"(%d,%d)%[\x1-\xFF]",&buffer_size,&buffer_block_size,path);
		#endif

		if(result!=3) {
			_condor_warning(CONDOR_WARNING_KIND_BADURL, "Couldn't understand url '%s'",sub_url);
			errno = EINVAL;
			free( path );
			free( sub_url );
			return -1;
		}
		if( buffer_size<0 || buffer_block_size<0 || buffer_size<buffer_block_size ) {
			_condor_warning(CONDOR_WARNING_KIND_NOTICE, "Invalid buffer configuration: (%d,%d)",buffer_size,buffer_block_size);
			errno = EINVAL;
			free( path );
			free( sub_url );
			return -1;
		}
		free( sub_url );
		sub_url = strdup(path);
		free( path );
	}

	result = original->open( sub_url, flags, mode );
	free( sub_url );
 	size = original->get_size();
	return result;
}

int CondorFileBuffer::close()
{
	flush(1);
	return original->close();
}

/*
Read data from this buffer.  Attempt to satisfy from the data in memory,
but resort to the original file object if necessary.
*/

int CondorFileBuffer::read(int offset, char *data, int length)
{
	CondorChunk *c=0;
	int piece=0;
	int bytes_read=0;
	int hole_top;

	// If the user is attempting to read past the end
	// of the file, chop off that access here.

	if((offset+length)>size) {
		length = size-offset;
	}

	while(length>0) {

		// hole_top keeps track of the lowest starting data point
		// in case we have created a virtual hole

		hole_top = MIN( size, offset+length );

		// Scan through all the data chunks.
		// If one overlaps with the beginning of the
		// request, then copy that data.

		for( c=head; c; c=c->next ) {
			if( contains(c,offset) ) {
				piece = MIN(c->begin+c->size-offset,length);
				memcpy(data,&c->data[offset-c->begin],piece);
				offset += piece;
				data += piece;
				length -= piece;
				bytes_read += piece;
				c->last_used = time++;
				break;
			} else {
				if((c->begin<hole_top)&&(c->begin>offset)) {
					hole_top = c->begin;
				}
			}
		}

		// If that worked, try it again.

		if(c) continue;

		// Now, consider the logical size of the buffer file
		// and the size of the actual file.  If we are less
		// than the former, but greater than the latter, simply
		// fill the hole with zeroes and continue above.

		piece = hole_top-offset;

		if( offset<size && offset>=original->get_size() ) {
			memset(data,0,piece);
			offset += piece;
			data += piece;
			length -= piece;
			bytes_read += piece;
			continue;
		}

		// Otherwise, make a new chunk.  Try to read a whole block

		c = new CondorChunk(offset,buffer_block_size,buffer_block_size);
		piece = original->read(offset,c->data,c->size);
		if(piece<0) {
			delete c;
			if(bytes_read==0) bytes_read=-1;
			break;
		} else if(piece==0) {
			delete c;
			break;
		} else {
			c->size = piece;
			head = absorb( head, c );
		}
	}

	trim();

	return bytes_read;
}

/*
When writing, simply create a new chunk and add it to the list.
If the resulting list is larger than the required buffer size,
select a block to write to disk.
*/

int CondorFileBuffer::write(int offset, char *data, int length)
{
	CondorChunk *c=0;

	c = new CondorChunk(offset,length,buffer_block_size);
	memcpy(c->data,data,length);
	c->dirty = 1;
	c->last_used = time++;

	head = absorb( head, c );

	trim();

	if((offset+length)>get_size()) {
		size = offset+length;
	}

	return length;
}

int CondorFileBuffer::fcntl( int cmd, int arg )
{
	return original->fcntl(cmd,arg);
}

int CondorFileBuffer::ioctl( int cmd, long arg )
{
	return original->ioctl(cmd,arg);
}

int CondorFileBuffer::ftruncate( size_t length )
{
	flush(1);
	size = length;
	return original->ftruncate(length);
}

int CondorFileBuffer::fsync()
{
	flush();
	return original->fsync();
}

int CondorFileBuffer::flush()
{
	flush(0);
	return 0;
}

int CondorFileBuffer::fstat(struct stat *buf)
{
	int ret;

	ret = original->fstat(buf);
	if (ret != 0) {
		return ret;
	}
	buf->st_size = get_size();

	return ret;
}

int CondorFileBuffer::is_readable()
{
	return original->is_readable();
}

int CondorFileBuffer::is_writeable()
{
	return original->is_writeable();
}

int CondorFileBuffer::is_seekable()
{
	return original->is_seekable();
}

int CondorFileBuffer::get_size()
{
	return size;
}

char const * CondorFileBuffer::get_url()
{
	return url ? url : "";
}

int CondorFileBuffer::get_unmapped_fd()
{
	return original->get_unmapped_fd();
}

int CondorFileBuffer::is_file_local()
{
	return original->is_file_local();
}

/*
Trim this buffer down to size.  As long as it is too big,
select the block that is least recently used, and write it out.
*/

void CondorFileBuffer::trim()
{
	CondorChunk *best_chunk,*i;
	int space_used;

	while(1) {
		space_used = 0;
		best_chunk = head;

		for( i=head; i; i=i->next ) {
			if( i->last_used < best_chunk->last_used ) {
				best_chunk = i;
			}
			space_used += i->size;
		}

		if( space_used <= buffer_size) return;

		evict( best_chunk );
	}
}

void CondorFileBuffer::flush( int deallocate )
{
	CondorChunk *c,*n;

	c=head;
	while(c) {
		clean(c);
		n = c->next;
		if(deallocate) {
			delete c;
		}
		c = n;
	}

	if(deallocate) head=0;

	original->flush();
}

/*
Clean this chunk by writing it out to disk.
*/

void CondorFileBuffer::clean( CondorChunk *c )
{
	if(c && c->dirty) {
		int result = original->write(c->begin,c->data,c->size);
		if(result!=c->size) _condor_error_retry("Unable to write buffered data to %s! (%s)",original->get_url(),strerror(errno));
		c->dirty = 0;
	}
}

/*
Evict this block by cleaning it, then removing it.
*/

void CondorFileBuffer::evict( CondorChunk *c )
{
	CondorChunk *i;

	clean(c);

	if( c==head ) {
		head = c->next;
		delete c;
	} else {
		for( i=head; i; i=i->next ) {
			if( i->next==c ) {
				i->next = c->next;
				delete c;
				break;
			}
		}
	}
}
