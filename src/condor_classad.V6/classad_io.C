#include "condor_common.h"
#include "classad_io.h"

bool ByteSource::
GetChar( int& chr )
{
		// if we've hit the max length, return end of input
	if( length >= 0 && index == length ) {
		chr = EOF;
		return true;
	}

		// get the next character
	if( !_GetChar( chr ) ) {
		return( false );
	}
	++index;

		// if the character is the sentinel, return end of input
	if( chr == sentinel ) {
		chr = EOF;
	}

	return( true );
}


StringSource::
StringSource( )
{
	buffer = 0;
}

StringSource::
~StringSource( )
{
}

void StringSource::
Initialize( const char *buf, int buflen )
{
	SetLength( buflen );
	buffer = buf;
}

bool StringSource::
_GetChar( int &chr )
{
	chr = buffer[index];
	return( true );
}


FileSource::
FileSource( )
{
	file = 0;
}

FileSource::
~FileSource( )
{
}


void FileSource::
Initialize( FILE *f, int len )
{
	SetLength( len );
	file = f;
}

bool FileSource::
_GetChar( int &chr )
{
	if( !file ) return( false );
	chr = getc( file );
	return( true );
}

FileDescSource::
FileDescSource( )
{
	fd = -1;
}

FileDescSource::
~FileDescSource( )
{
}

void FileDescSource::
Initialize( int f, int len )
{
	SetLength( len );
	fd = f;
}

bool FileDescSource::
_GetChar( int &chr )
{
	char ch;
	if( fd < 0 ) return( false );
	switch( read( fd, &ch, 1 ) ) {
		case 0:
			chr = EOF;
			return( true );

		case 1:
			chr = ch;
			return( true );

		default:
			return( false );
	}
}



bool ByteSink::
PutBytes( const void* buf, int buflen )
{
	bool rval;
	int  num;

		// check if we're already over the limit
	if( length >= 0 && index >= length - 1 ) {
		return( false );
	}

		// put in appropriate number of bytes
	if( length > 0 ) {
		num = ( length-index-1 ) >= buflen ? buflen : ( length-index-1 );
	} else {
		num = buflen;
	}

	rval = _PutBytes( buf, num );

		// increment number of bytes written so far
	if( rval ) index += num;

	return( rval && buflen == num );
}

bool ByteSink::
Flush( )
{
	if( _PutBytes( (void*)&terminal, 1 ) ) {
		index++;
		return( true );
	}
	return( false );
}


StringSink::
StringSink( )
{
	buffer = NULL;
}

StringSink::
~StringSink( )
{
}

void StringSink::
Initialize( char *buf, int maxlen )
{
	SetLength( terminal == '\0' ? maxlen : (maxlen-1) );
	buffer = buf;
}

bool StringSink::
_PutBytes( const void *bytes, int len )
{
	if( !buffer ) return( false );
	strncpy( buffer+index, (char*)bytes, len );
	return( true );
}

bool StringSink::
_Flush( )
{
	if( !buffer ) return( false );

	if( terminal != '\0' ) {
		buffer[index] = '\0';
	}
	return( true );
}


StringBufferSink::
StringBufferSink( )
{
	bufref = NULL;
	bufSize = 0;
}


StringBufferSink::
~StringBufferSink( )
{
}


void StringBufferSink::
Initialize( char *&buf, int &sz, int maxlen )
{
	SetLength( terminal == '\0' ? maxlen : (maxlen-1) );

	bufref = &buf;
	bufSize = &sz;

	if( !buf || sz <= 0 ) {
		if( ( buf = new char[1024] ) == NULL ) {
			bufref = NULL;
			bufSize = NULL;
		}
		sz = 1024;
	}
}


bool StringBufferSink::
_PutBytes( const void *bytes, int num )
{
	int newsize;

	if( !bufref || !bufSize ) return( false );

		// if we don't have enough room, resize
	if( index+num >= *bufSize ) {
		if( (*bufSize) > num ) {
			newsize = 2 * (*bufSize );
		} else {
			newsize = *bufSize + num + 64;
		}
		char *temp = new char[ newsize ];
		if( !temp ) {
			delete [] *bufref;
			return( false );
		}
		*bufSize = newsize;
		strcpy( temp, *bufref );
	}

	strncpy( (*bufref)+index, (const char*)bytes, num );
	return( true );
}

bool StringBufferSink::
_Flush( )
{
	if( terminal != '\0' ) {
		return( _PutBytes( (void*)"\0", 1 ) );
	}
	return( true );
}


FileSink::
FileSink( )
{
	file = NULL;
}

FileSink::
~FileSink( )
{
}

void FileSink::
Initialize( FILE *f, int maxlen )
{
	SetLength( maxlen );
	file = f;
}

bool FileSink::
_PutBytes( const void *buf, int sz )
{
	if( !file ) return false;
	return( fwrite( buf, 1, sz, file ) == (unsigned)sz );
}

bool FileSink::
_Flush( )
{
	if( !file ) return false;
	fflush( file );
	return( true );
}

FileDescSink::
FileDescSink( )
{
	fd = -1;
}

FileDescSink::
~FileDescSink( )
{
}

void FileDescSink::
Initialize( int f, int maxlen )
{
	SetLength( maxlen );
	fd = f;
}

bool FileDescSink::
_PutBytes( const void *buf, int sz )
{
	if( fd < 0 ) return( false );
	return( write( fd, buf, sz ) == sz );
}

