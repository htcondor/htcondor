/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
