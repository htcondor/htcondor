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


#include "condor_file_compress.h"
#include "condor_error.h"

CondorFileCompress::CondorFileCompress( CondorFile *o )
{
	int i;

	last_action = NONE;
	memset( &stream, 0, sizeof(stream) );
	original = o;
	poffset = 0;
	voffset = 0;
	crc = 0;
	url = NULL;

	for (i = 0; i < COMPRESS_BUFFER_SIZE; i++) {
		buffer[i] = 0;
	}
}

CondorFileCompress::~CondorFileCompress()
{
	delete original;
	free( url );
}

int CondorFileCompress::open( const char *url_in, int flags, int mode )
{
	char *junk = (char *)malloc(strlen(url_in)+1);
	char *sub_url = (char *)malloc(strlen(url_in)+1);

	free( url );
	url = strdup(url_in);

	/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't
		work with 8bit numbers */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	sscanf( url, "%[^:]:%[\x1-\x7F]", junk, sub_url );
	#else
	sscanf( url, "%[^:]:%[\x1-\xFF]", junk, sub_url );
	#endif
	free( junk );

	int result = original->open( sub_url, flags, mode );
	free( sub_url );
	return result;
}

int CondorFileCompress::close()
{
	end_compression();
	return original->close();
}

/* Stop the current compression, whatever it may be, and go to NONE. */

void CondorFileCompress::end_compression()
{
	if( last_action==WRITE ) {
		write_complete();
		deflateEnd(&stream);
	}

	if( last_action==READ ) {
		inflateEnd(&stream);
	}

	last_action = NONE;
}

/* Set up the decompression stream to read from the desired offset, and then decompress. */

int CondorFileCompress::read( int offset, char *data, int length )
{
	if( offset>=voffset && last_action==READ ) {

		// A read at or after the last known position
		// simply consumes data going forward.

	} else {

		// In any other case, reset the compression state,
		// go back to the beginning, and consume data
		// going forward.
				
		end_compression();

		poffset = voffset = 0;

		if(!read_header()) {
			errno = EBADF;
			return -1;
		}

		memset( &stream, 0, sizeof(stream) );
		inflateInit2( &stream, -MAX_WBITS );
	}

	// If needed, consume data up until the 
	// desired position.

	read_consume( offset-voffset );

	last_action = READ;
	return read_data( data, length );
}

#define THROWAWAY_SIZE 32768

/* Throwaway a certain number of bytes from the current logical position. */

int CondorFileCompress::read_consume( int bytes )
{
	char throwaway[THROWAWAY_SIZE];
	int result, chunk;

	while(bytes>0) {
		if( bytes>THROWAWAY_SIZE ) {
			chunk = THROWAWAY_SIZE;
		} else {
			chunk = bytes;
		}

		result = read_data( throwaway, chunk );
		if( result<=0 ) return 0;

		bytes-=result;
	}

	return 1;
}

/* Read data into the given pointer, working from the current logical position. */

int CondorFileCompress::read_data( char *data, int length )
{
	int result;
	int failure=0;
	int bytes_read=0;

	stream.next_out = (Bytef*) data;
	stream.avail_out = length;

	while(1) {
		result = inflate( &stream, Z_SYNC_FLUSH );

		if( result==Z_STREAM_END ) {

			// The end of the compressed stream has been reached.
			// Set the physical pointer to the end of the stream.

			poffset -= stream.avail_in;

			// Skip over the 8 byte gzip header.

			poffset += 8;

			// Look for a new header.  If found, reset the compression stream
			// and go on.  Otherwise, terminate the read.

			if(read_header()) {
				stream.avail_in = 0;
				stream.next_in = 0;
				inflateReset(&stream);
			} else {
				end_compression();
				break;
			}

		} else if( result!=Z_OK && stream.avail_in!=0 ) {
			_condor_warning(CONDOR_WARNING_KIND_NOTICE, "Error %d while uncompressing %s\n",result,url);
			failure=1;
			break;
		}

		if( stream.avail_out==0 ) {
			break;
		} else {
			result = original->read( poffset, buffer, COMPRESS_BUFFER_SIZE );
			if( result>0 ) {
				stream.avail_in = result;
				stream.next_in = (Bytef*) buffer;
				poffset += result;
				continue;
			} else if( result==0 ) {
				break;
			} else {
				failure=1;
				break;
			}
		}
	}

	bytes_read = length-stream.avail_out;
	voffset += bytes_read;

	if( bytes_read>0 ) {
		return bytes_read;
	} else if( failure ) {
		return -1;
	} else {
		return 0;
	}
}

/* Look for a gzip header at the current position. */

int CondorFileCompress::read_header()
{
	gzip_header header;

	int result = original->read(poffset,(char*)&header,sizeof(header));
	if( result!=sizeof(header) ) {
		/* Silent warning here -- no need to spew errors for empty files. */
		return 0;
	}

	if( header.magic0!=GZIP_MAGIC0 || header.magic1!=GZIP_MAGIC1 ) {
		_condor_warning(CONDOR_WARNING_KIND_BADURL, "%s does not appear to be a valid gzip file",url);
		return 0;
	}

	poffset += sizeof(header);

	if( header.method!=GZIP_METHOD_DEFLATED ) {
		_condor_warning(CONDOR_WARNING_KIND_BADURL, "I don't know how to read compression method %d in gzip %s",header.method,url);
		return 0;
	}

	if( header.flags&GZIP_FLAG_MULTI ) {
		_condor_warning(CONDOR_WARNING_KIND_BADURL, "I don't know how to read multi-part gzip %s",url);
		return 0;
	}

	if( header.flags&GZIP_FLAG_EXTRA ) {
		int elength = read_int( 2 );
		poffset += elength;		
	}

	if( header.flags&GZIP_FLAG_NAME ) {
		read_string();
	}

	if( header.flags&GZIP_FLAG_COMMENT ) {
		read_string();
	}

	if( header.flags&GZIP_FLAG_ENCRYPTED ) {
		_condor_warning(CONDOR_WARNING_KIND_BADURL, "I don't know how to read encrypted gzip %s",url);
		return 0;
	}

	if( header.flags&GZIP_FLAG_RESERVED ) {
		_condor_warning(CONDOR_WARNING_KIND_BADURL, "I don't understand flags 0x%x in %s",header.flags,url);
		return 0;
	}

	return 1;
}

/* Read an integer of the given number of bytes from the physical file.  */

int CondorFileCompress::read_int( int bytes )
{
	unsigned char b;
	int x=0;

	for( int i=0; i<bytes; i++ ) {
		original->read( poffset++, (char*)&b, 1 );		
		x = x + (b<<(8*i));
	}

	return x;
}

/* Read and discard a null-terminated string from the physical file. */

void CondorFileCompress::read_string()
{
	char ch;
	int rval;

	while(1) {
		rval = original->read( poffset++, &ch, 1 );
		if( rval!=1 || ch==0 ) return;
	}
}

/* Set up the decompression stream to work from the given offset, and then compress. */

int CondorFileCompress::write( int offset, char *data, int length )
{
	if( offset==voffset && last_action==WRITE ) {

		// continue from the last write
		// everything stays the same

	} else {

		// starting a new write:
		//    if offset==0, rewrite from beginning
		//    otherwise, append to the end.

		end_compression();

		if( offset==0 ) {
			voffset = poffset = 0;
		} else {
			voffset = offset;
			poffset = original->get_size();
		}

		if(!write_header()) {
			errno = EBADF;
			return -1;
		}

		memset( &stream, 0, sizeof(stream) );
		deflateInit2( &stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY );
		voffset = 0;
		crc = crc32(0,0,0);
		stream.next_out = (Bytef*) buffer;
		stream.avail_out = COMPRESS_BUFFER_SIZE;

	}

	last_action = WRITE;
	return write_data( data, length, Z_NO_FLUSH );
}

/* Write data to the current logical offset.  If flush==Z_FINISH, push all the data out. */

int CondorFileCompress::write_data( char *data, int length, int flushData )
{
	int result;
	int failure=0;
	int bytes_written=0;

	stream.next_in = (Bytef*) data;
	stream.avail_in = length;

	while(1) {
		deflate( &stream, flushData );

		if( flushData==Z_NO_FLUSH ) {
			if( stream.avail_in==0 ) break;
		} else {
			if( stream.avail_out==COMPRESS_BUFFER_SIZE ) break;
		}

		result = original->write( poffset, buffer, COMPRESS_BUFFER_SIZE-stream.avail_out );
		if( result>0 ) {
			stream.avail_out = COMPRESS_BUFFER_SIZE;
			stream.next_out = (Bytef*) buffer;
			poffset += result;
			continue;
		} else if( result==0 ) {
			break;
		} else {
			failure=1;
			break;
		}
	}

	bytes_written = length-stream.avail_in;
	voffset += bytes_written;

	if( bytes_written>0 ) {
		crc = crc32(crc,(Bytef*)data,bytes_written);
		return bytes_written;
	} else if( failure ) {
		return -1;
	} else {
		return 0;
	}
}

/* Flush all buffered data and tack on the 8 byte gzip trailer */

void CondorFileCompress::write_complete()
{
	write_data( 0, 0, Z_FINISH );
	write_int( 4, crc );
	write_int( 4, stream.total_in );
}

/* Write a gzip header to the current physical position. */

int CondorFileCompress::write_header()
{
	gzip_header header;

	memset( &header, 0, sizeof(header) );

	header.magic0 = GZIP_MAGIC0;
	header.magic1 = GZIP_MAGIC1;
	header.method = GZIP_METHOD_DEFLATED;
	header.system = GZIP_SYSTEM_UNIX;

	int result = original->write( poffset, (char*) &header, sizeof(header));

	if( result==sizeof(header) ) {
		poffset += sizeof(header);
		return 1;
	} else {
		return 0;
	}
}

/* Write an integer to the current physical position. */

int CondorFileCompress::write_int( int bytes, int x )
{
	for( int i=0; i<bytes; i++ ) {
		char b = x & 0xff;
		original->write( poffset++, &b, 1 );
		x = x>>8;
	}
	return 0;
}

int CondorFileCompress::fcntl( int cmd, int arg )
{
	return original->fcntl(cmd,arg);
}

int CondorFileCompress::ioctl( int cmd, long arg )
{
	return original->ioctl(cmd,arg);
}

int CondorFileCompress::ftruncate( size_t /* length */ )
{
	_condor_warning(CONDOR_WARNING_KIND_BADURL, "You can't ftruncate() a compressed file (%s).",
					 get_url() ); 
	errno = EINVAL;
	return -1;
}

int CondorFileCompress::fstat( struct stat* /* buf */ )
{
	_condor_warning(CONDOR_WARNING_KIND_BADURL, "You can't fstat() a compressed file (%s).", 
					 get_url() );
	errno = EINVAL;
	return -1;
}

int CondorFileCompress::fsync()
{
	flush();
	return original->fsync();
}

int CondorFileCompress::flush()
{
	// In order to really flush the data so the file can be read,
	// we must force all the data out, including a trailer.
	// The next write will give a new header.

	end_compression();
	return original->flush();
}


int CondorFileCompress::is_readable()
{
	return original->is_readable();
}

int CondorFileCompress::is_writeable()
{
	return original->is_writeable();
}

/*
If the underlying file is seekable, this file is, too...
...except for a very specific condition in write(), which is documented there.
*/

int CondorFileCompress::is_seekable()
{
	return original->is_seekable();
}

/* Getting the size of the uncompressed file is impossible without actually reading it. */

int CondorFileCompress::get_size()
{
	return -1;
}

char const * CondorFileCompress::get_url()
{
	return original->get_url();
}

int CondorFileCompress::get_unmapped_fd()
{
	return original->get_unmapped_fd();
}

int CondorFileCompress::is_file_local()
{
	return original->is_file_local();
}
