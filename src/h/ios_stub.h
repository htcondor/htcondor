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
#ifndef _CLIENT_H
#define _CLIENT_H

#include "condor_common.h"
#include "ios_common.h"

#define DEFLT_MSG 1000

class OpenMsg_t 
{
    protected :
	Msgtype mtype ;
	char *path ;
	int oflag ;
	mode_t mode ;

    public :
	OpenMsg_t( const char *p, int of, mode_t m ) ;

        ~OpenMsg_t();

	int Send(int);
};

class CloseMsg_t
{
    protected :
	Msgtype mtype ;
	int filedes ;

    public :
	CloseMsg_t( int f ) ;

	int Send(int);
};

class LseekMsg_t
{
    protected :
	Msgtype mtype ;
	int filedes;
	off_t offset ;
	int whence ;

    public :
	LseekMsg_t( int f, off_t off, int wh ) ;

	int Send(int);
};

class ReadMsg_t
{
    protected :
	Msgtype mtype ;
	int filedes;
	int len ;

    public :
	ReadMsg_t( int f, int l) ;

	int Send(int);
};

class WriteMsg_t
{
    protected :
	Msgtype mtype ;
	int filedes; 
	char *buf ;
	int len ;

    public :
	WriteMsg_t( int fd, const char *p, int l ) ;

	int Send(int);

        ~WriteMsg_t();
};

class FstatMsg_t
{
    protected :
	Msgtype mtype ;
	int filedes; 

    public :
	FstatMsg_t( int fd ) ;

	int Send(int);

        ~FstatMsg_t();
};




extern "C" {

int ioserver_open(const char *path, int oflag, mode_t mode);

int ioserver_close(int filedes);

off_t ioserver_lseek(int filedes, off_t offset, int whence);

ssize_t ioserver_read(int filedes, char *buf, unsigned int size);

ssize_t ioserver_write(int filedes, const char *buf, unsigned int size);

int ioserver_fstat(int fildes, struct stat *buf);
}


#endif
