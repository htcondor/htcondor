#include "condor_common.h"
#include "cedar_io.h"

BEGIN_NAMESPACE( classad )

CedarSource::
CedarSource( )
{
	strm = NULL;
}

CedarSource::
~CedarSource( )
{
}

void CedarSource::
Initialize( Stream *s, int maxlen )
{
	SetLength( maxlen );
	strm = s;
}

bool CedarSource::
_GetChar( int &ch )
{
	char chr;

	if( strm && strm->get_bytes( &chr, 1 )==1 ) {
		ch = chr;
		return( true );
	}

	return( false );
}


CedarSink::
CedarSink( )
{
	strm = NULL;
}

CedarSink::
~CedarSink( )
{
}

void CedarSink::
Initialize( Stream *s, int maxlen )
{
	SetLength( maxlen );
	strm = s;
}

bool CedarSink::
_PutBytes( const void *buf, int buflen )
{
	if( strm && strm->put_bytes( buf, buflen )==buflen ) {
		return( true );
	}
	return( false );
}

bool CedarSink::
_Flush( )
{
	return( true );
}

#if 0
bool CedarStream::
_Connect( std::string url )
{
    Socket * s = NULL;

    switch (this->SockType()) {
      case Sock_STREAM: s = new ReliSock(); break;
      case Sock_DGRAM:  s = new SafeSock(); break;
      default: assert (false);
    }

    ((CedarSource*)m_src)->strm = s;
    ((CedarSource*)m_src)->strm = s;
}
#endif

END_NAMESPACE // classad
