/* 
** Copyright 1997 by the Condor Team
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
*/ 

/* I/O server
   Written by : Abhinav Gupta
                S.Chandrasekar
*/

#include <ios_common.h>
#include <ios_stub.h>

OpenMsg_t::OpenMsg_t( const char *p, int of, mode_t m ) 
{
    mtype = OPEN ;
    path = new char [ strlen(p)+1 ] ;
    strcpy( path, p ) ;
    oflag = of ;
    mode = m ;
}

int OpenMsg_t::Send( int s)
{
    char *msg_str ;
    int msg_len ;
    int path_len = strlen(path);;
    int temp_int ;

    msg_len = 1 + sizeof(int) + path_len + sizeof( oflag ) + 
	sizeof( mode ) ;
    msg_str = new char [ msg_len ] ;
    msg_str[0] = (char) mtype ;
    temp_int = htonl( path_len ) ;
    memcpy( msg_str + 1, &temp_int, sizeof(int));
    memcpy( msg_str + 1 + sizeof(int), path, path_len ) ;
    temp_int = htonl( oflag ) ;
    memcpy( msg_str + 1 + sizeof(int) + path_len, &temp_int, sizeof(int));
    temp_int = htonl( mode ) ;
    memcpy( msg_str + 1 + sizeof(int) + path_len + sizeof(int), 
	&temp_int, sizeof(int));

    temp_int = htonl( msg_len ) ;
    write( s, (char *)&temp_int, sizeof(int));
    int ret =  write( s, msg_str, msg_len);
    delete msg_str ;

    return ret ;
 }  

OpenMsg_t::~OpenMsg_t()
{
    delete path ;
}

CloseMsg_t::CloseMsg_t( int f )
{
    mtype = CLOSE ;
    filedes = f ;
}

int CloseMsg_t::Send( int s)
{
    char *msg_str ;
    int msg_len ;
    int temp_int ;

    msg_len = 1 + sizeof(int) ;
    msg_str = new char [ msg_len ] ;
    msg_str[0] = (char) mtype ;
    temp_int = htonl( filedes ) ;
    memcpy( msg_str + 1, &temp_int, sizeof(int));

    temp_int = htonl( msg_len ) ;
    write( s, (char *)&temp_int, sizeof(int));
    int ret =  write( s, msg_str, msg_len);
    delete msg_str ;

    return ret ;
}  

ReadMsg_t::ReadMsg_t( int f, int l )
{
    mtype = READ ;
    filedes = f ;
    len = l ;
}

int ReadMsg_t::Send( int s)
{
    char *msg_str ;
    int msg_len ;
    int temp_int ;

    msg_len = 1 + 2*sizeof(int) ;
    msg_str = new char [ msg_len ] ;
    msg_str[0] = (char) mtype ;
    temp_int = htonl( filedes ) ;
    memcpy( msg_str + 1, &temp_int, sizeof(int));
    temp_int = htonl( len ) ;
    memcpy( msg_str + 1 + sizeof(int), &temp_int, sizeof(int));

    temp_int = htonl( msg_len ) ;
    int ret = write( s, (char *)&temp_int, sizeof(int));
    if (ret >= 0) ret =  write( s, msg_str, msg_len);
    delete msg_str ;

    return ret ;
}  


WriteMsg_t::WriteMsg_t( int f, const char *p, int l )
{
    mtype = WRITE ;
    buf = new char [ l ] ;
    memcpy( buf, p, l ) ;
    filedes = f ;
    len = l ;
}

int WriteMsg_t::Send( int s)
{
    char *msg_str ;
    int msg_len ;
    int temp_int ;

    msg_len = 1 + 2*sizeof(int) + len ;
    msg_str = new char [ msg_len ] ;
    msg_str[0] = (char) mtype ;
    temp_int = htonl( filedes ) ;
    memcpy( msg_str + 1, &temp_int, sizeof(int));
    temp_int = htonl( len ) ;
    memcpy( msg_str + 1 + sizeof(int), &temp_int, sizeof(int));
    memcpy( msg_str + 1 + 2*sizeof(int), buf, len ) ;

    temp_int = htonl( msg_len ) ;
    write( s, (char *)&temp_int, sizeof(int));
    int ret =  write( s, msg_str, msg_len);
    delete msg_str ;

    return ret ;

}  

WriteMsg_t::~WriteMsg_t()
{
    delete buf ;
}

LseekMsg_t::LseekMsg_t( int f, off_t off, int wh )
{
    mtype = LSEEK ;
    filedes = f ;
    offset = off ;
    whence = wh ;
}

int LseekMsg_t::Send( int s)
{
    char *msg_str ;
    int msg_len ;
    int temp_int ;

    msg_len = 1 + 3*sizeof(int) ;
    msg_str = new char [ msg_len ] ;
    msg_str[0] = (char) mtype ;
    temp_int = htonl( filedes ) ;
    memcpy( msg_str + 1, &temp_int, sizeof(int));
    temp_int = htonl( offset ) ;
    memcpy( msg_str + 1 + sizeof(int), &temp_int, sizeof(int));
    temp_int = htonl( whence ) ;
    memcpy( msg_str + 1 +2*sizeof(int), &temp_int, sizeof(int));

    temp_int = htonl( msg_len ) ;
    write( s, (char *)&temp_int, sizeof(int));
    int ret =  write( s, msg_str, msg_len);
    delete msg_str ;

    return ret ;
}  

FstatMsg_t::FstatMsg_t( int f )
{
    mtype =FSTAT ;
    filedes = f ;
}

int FstatMsg_t::Send( int s)
{
    char *msg_str ;
    int msg_len ;
    int temp_int ;

    msg_len = 1 + sizeof(int) ;
    msg_str = new char [ msg_len ] ;
    msg_str[0] = (char) mtype ;
    temp_int = htonl( filedes ) ;
    memcpy( msg_str + 1, &temp_int, sizeof(int));

    temp_int = htonl( msg_len ) ;

    write( s, (char *)&temp_int, sizeof(int));
    int ret =  write( s, msg_str, msg_len);
    delete msg_str ;

    return ret ;
}  

FstatMsg_t::~FstatMsg_t()
{
}

