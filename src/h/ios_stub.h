#ifndef _CLIENT_H
#define _CLIENT_H

#include <ios_common.h>

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




int ioserver_open(const char *path, int oflag, mode_t mode);

int ioserver_close(int filedes);

extern "C" {

off_t ioserver_lseek(int filedes, off_t offset, int whence);

ssize_t ioserver_read(int filedes, char *buf, unsigned int size);

ssize_t ioserver_write(int filedes, const char *buf, unsigned int size);

int ioserver_fstat(int fildes, struct stat *buf);
}


#endif
