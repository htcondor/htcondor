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

