#include "condor_common.h"
#include "sink.h"
#include "formatOptions.h"

BEGIN_NAMESPACE( classad )

Sink::
Sink ()
{
	sink = NULL;
	pp = NULL;
}


Sink::
~Sink()
{
	if( sink ) delete sink;
}


void Sink::
SetTerminalChar( int c )
{
	if( sink ) sink->SetTerminal( c );
}

	
void Sink::
SetSink( char *&s, int &len, bool useInitial )
{
	StringBufferSink *snk;

	if( sink ) delete sink;
	if( ( snk = new StringBufferSink ) == NULL ) {
		return;
	}
	if( !useInitial ) {
		s = NULL;
		len = 0;
	}
	snk->Initialize( s, len );
	sink = snk;
}

void Sink::
SetSink( char *s, int sz )
{
	StringSink	*snk;

	if( sink ) delete sink;
	if( ( snk = new StringSink ) == NULL ) {
		return;
	}

	snk->Initialize( s, sz );
	sink = snk;
}


void Sink::
SetSink (int i)
{
	FileDescSink	*snk;

	if( sink ) delete sink;
	if( ( snk = new FileDescSink ) == NULL ) {
		return;
	}

	snk->Initialize( i );
	sink = snk;
}


void Sink::
SetSink (FILE *f)
{
	FileSink	*snk;

	if( sink ) delete sink;
	if( ( snk = new FileSink ) == NULL ) {
		return;
	}

	snk->Initialize( f );
	sink = snk;
}

void Sink::
SetSink( ByteSink *snk )
{
	if( sink ) delete sink;
	sink = snk;
}

bool Sink::
SendToSink (void *bytes, int len)
{
	if( !sink ) return( false );
	return( sink->PutBytes( bytes, len ) );
}


bool Sink::
Terminate ()
{
	if( !sink ) return( false );
	return( sink->Terminate( ) );
}

bool Sink::
FlushSink ()
{
	if( !sink ) return( false );
	return( sink->Flush( ) );
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
	minimalParens = true;
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

END_NAMESPACE // classad
