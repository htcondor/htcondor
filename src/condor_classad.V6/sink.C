#include "sink.h"

Sink::
Sink ()
{
	sink  = NO_SINK;
	pp = NULL;
	terminal = '\0';
}


Sink::
~Sink()
{
}


	
void Sink::
SetSink( char *&s, int &len, bool useInitial )
{
	sink = STRING_SINK;
	last = 0;

	// we have to perform the memory management for the string
	if( useInitial ) {
		buffer = s;
		bufLen = len;
	} else {
		buffer = new char [1024];
		bufLen = 1024;
	}

	// set up references for update; perform first update
	bufRef = &s;
	bufLenRef = &len;
	*bufRef = buffer;
	*bufLenRef = bufLen;
}


void Sink::
SetSink( char *s, int sz )
{
	// buffer provided by user, don't use references
	sink = STRING_SINK;
	buffer = s;
	bufLen = sz;
	bufRef = NULL;
	bufLenRef = NULL;
}


void Sink::
SetSink (Sock &s)
{
	sink = CEDAR_SINK;
	sock = &s;
}


void Sink::
SetSink (int i)
{
    sink = FILE_DESC_SINK;
    fd = i;
}


void Sink::
SetSink (FILE *f)
{
	sink = FILE_SINK;
	file = f;
}


bool Sink::
SendToSink (void *bytes, int len)
{
	char 	*buf = (char *) bytes;
	int 	rval;

	switch (sink)
	{
		case STRING_SINK:
			// first check that we have enough room
			if (last+len >= bufLen) {
				// no ... return false if we cannot resize
				if (bufRef == NULL) return false;

				// resize to twice original size
				char *temp = new char [2*bufLen];
				if (!temp) {
					delete [] buffer;
					return false;
				}
				bufLen *= 2;
				strcpy( temp, buffer );

				// make the switch
				delete [] buffer;
				buffer = temp;
				*bufRef = buffer;
				*bufLenRef = bufLen;
			}

			// have enough room; copy and update lengths
			strcpy( buffer+last, buf );
			last += len;

			return true;


		case FILE_SINK:
			// writing 'len' number of objects, each of size 1
			rval = fwrite (bytes, 1, len, file);
			return (rval == len);


		case FILE_DESC_SINK:
			rval = write (fd, bytes, len);
			return (rval == len);


		case CEDAR_SINK:
			rval = sock->put_bytes (bytes, len);
			return (rval == len);


		default:
			return false;
	}

	// should not reach here
	return false;
}


bool Sink::
FlushSink ()
{
	if (!SendToSink((void*)&terminal, 1)) return false;

	switch (sink)
	{
		case STRING_SINK:
			return( terminal == '\0' || SendToSink((void*)"\0", 1) );

		case FILE_SINK:
			// fflush() returns EOF on error; 0 otherwise
			return (fflush(file) == 0);

		case FILE_DESC_SINK:
		case CEDAR_SINK:
			// no-op
			return true;
	}

	return false;
}


FormatOptions::
FormatOptions( )
{
	indentClassAds = false;
	indentLists = false;
	indentLevel = 0;
	classadIndentLen = 4;
	listIndentLen = 4;
	wantQuotes = true;
	precedenceLevel = -1;
	marginWrapCols = 79;
	marginIndentLen = 3;
}


FormatOptions::
~FormatOptions( )
{
}


void FormatOptions::
SetClassAdIndentation( bool i, int len )
{
	indentClassAds = i;
	classadIndentLen = len;
}


void FormatOptions::
SetListIndentation( bool i, int len )
{
	indentLists = i;
	listIndentLen = len;
}	

void FormatOptions::
GetClassAdIndentation( bool &i, int &len )
{
	i = indentClassAds;
	len = classadIndentLen;
}


void FormatOptions::
GetListIndentation( bool &i, int &len )
{
	i = indentLists;
	len = listIndentLen;
}	


bool FormatOptions::
Indent( Sink& s )
{
	for( int i = 0 ; i < indentLevel/4; i++ ) {
		if( !s.SendToSink( (void*)"    ", 4 ) ) {
			return false;
		}
	}

	for( int i = 0 ; i < indentLevel%4; i++ ) {
		if( !s.SendToSink( (void*)" ", 1 ) ) {
			return false;
		}
	}
	
	return true;
}


void FormatOptions::
SetIndentLevel( int i )
{
	indentLevel = i;
}


int FormatOptions::
GetIndentLevel( )
{
	return indentLevel;
}

void FormatOptions::
SetWantQuotes( bool wq )
{
	wantQuotes = wq;
}

bool FormatOptions::
GetWantQuotes( )
{
	return( wantQuotes );
}

void FormatOptions::
SetMinimalParentheses( bool m )
{
	minimalParens = m;
}

bool FormatOptions::
GetMinimalParentheses( )
{
	return( minimalParens );
}

void FormatOptions::
SetPrecedenceLevel( int pl )
{
	precedenceLevel = pl;
}

int FormatOptions::
GetPrecedenceLevel( )
{
	return( precedenceLevel );
}

void FormatOptions::
SetMarginWrap( int cols, int indentLen )
{
	marginWrapCols = cols;
	marginIndentLen = indentLen;
}

void FormatOptions::
GetMarginWrap( int &cols, int &indentLen )
{
	cols = marginWrapCols;
	indentLen = marginIndentLen;
}
