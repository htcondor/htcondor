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

 

#ifndef _FILE_STATE_H
#define _FILE_STATE_H

#include <limits.h>
#include <string.h>
#include <sys/types.h>

const int MAXBUF = 131072;
const int PREFETCH = 2048;

typedef unsigned int	Bit;
typedef int				BOOL;
typedef int				FD;

struct BufElem {

  friend class File;
  friend class OpenFileTable;

 public:

 private:

  int offset;
  int len;
  char *buf;
  BufElem *next;

};

class OpenFileTable;

class File {
friend class OpenFileTable;
public:
	void	Init();
	void	Display();
        void    setOffset(off_t temp)   { offset = temp; }
        void    setSize(off_t temp)     { size = temp; }
        off_t   getOffset()             { return offset; }
        off_t   getSize()               { return size; }
	BOOL	isOpen()		{ return open; }
	BOOL	isDup()			{ return duplicate; }
	BOOL	isPreOpened()		{ return pre_opened; }
	BOOL	isRemoteAccess()	{ return remote_access; }
	BOOL	isIOServerAccess()	{ return ioserver_access; }
	BOOL	isShadowSock()		{ return shadow_sock; }
	BOOL	isReadable()		{ return readable; }
	BOOL	isWriteable()		{ return writeable; }
private:
	Bit	open : 1;		// file is open
	Bit	duplicate : 1;		// file is dup of another fd
	Bit	pre_opened : 1;		// file was pre_opened (stdin, stdout, stderr)
	Bit	remote_access : 1;	// access via remote sys calls (via the shadow)
	Bit	ioserver_access : 1;	// access via IO server
	Bit	shadow_sock : 1;	// TCP connection to the shadow
	Bit	readable : 1;		// File open for read or read/write
	Bit	writeable : 1;		// File open for write or read/write
	off_t   size;                   // File size
	off_t	offset;			// File pointer position
	char	*ioservername;		// The hostname of the IO server
	int	ioserverport;		// The port for contacting IO server
	int	ioserversocket;		// The socket descriptor for contacting
					// the IO server
	int	flags;			// The flags when the file was opened
					// This was not needed initially but
					// with the addition of IO server files
					// may be reopened.
	mode_t	mode;			// Mode when file was opened.
	FD		real_fd;	// File descriptor number
	FD		dup_of;		// File descriptor this is a dup of
	char	*pathname;		// *FULL* pathname of the file
	BufElem *firstBuf;              // first buffer element.
};

class OpenFileTable {
public:
	void Init();
	void Display();
	void Save();
	void Restore();
	int PreOpen( int fd, BOOL readable, BOOL writeable, BOOL shadow_connection);
#if 0
	int DoOpen( const char *path, int flags, int mode, int fd, int is_remote );
#else
	int DoOpen( const char *path, int flags, int fd, int is_remote, int status = -1);
#endif
	off_t DoLseek(int fd, off_t offset, int whence);
	int DoClose( int fd );
	int DoDup( int fd );
	int DoDup2( int fd, int dupfd );
	int DoSocket( int addr_family, int type, int protocol );
	int DoAccept( int s, struct sockaddr *addr, int *addrlen );
	int	Map( int user_fd );

	// Methods for setting various parameters needed to contact 
	// another IO server if necessary.

	int setServerName( int fd, char *name );
	int setServerPort( int fd, int port );
	int setSockFd( int fd, int sockfd );
	int setRemoteFd( int fd, int realfd );
	int setOffset( int fd, int off );
	int setFileName( int fd, char *name );
	int setFlag( int fd, int flag );
	int setMode( int fd, mode_t modes );

	char	*getServerName( int fd );
	int	getServerPort( int fd );
	int	getSockFd( int fd );
	int	getRemoteFd( int fd );
	int	getOffset( int fd );
	char	*getFileName( int fd ) ;
	int	getFlag( int fd );
	mode_t	getMode( int fd );
	BOOL IsLocalAccess( int user_fd );
	BOOL isIOServerAccess( int fd );
	BOOL IsDup( int user_fd );
	void SetOffset(int i, off_t temp)  { file[i].setOffset(temp); }
	void SetSize(int i, off_t temp)    { file[i].setSize(temp); }
	off_t GetOffset(int i)             { return file[i].getOffset(); }
	off_t GetSize(int i)               { return file[i].getSize(); }
	void IncreBufCount( int i )        { bufCount += i; }
	BOOL IsReadable(int i)             { return file[i].isReadable(); }
	BOOL IsWriteable(int i)            { return file[i].isWriteable(); }
	ssize_t InsertR( int, void *, int, int, int, BufElem *, BufElem * );
	ssize_t InsertW( int, const void *, int, int, BufElem *, BufElem * );
	ssize_t OverlapR1( int, void *, int, int, int, BufElem *, BufElem * );
	ssize_t OverlapR2( int, void *, int, int, int, BufElem *, BufElem * );
	ssize_t OverlapW1( int, const void *, int, int, BufElem *, BufElem * );
	ssize_t OverlapW2( int, const void *, int, int, BufElem *, BufElem * );
	ssize_t PreFetch( int, void *, size_t );
	ssize_t Buffer( int, const void *, size_t );
	void FlushBuf();
	void DisplayBuf();
	int find_avail( int start );
private:
	int     bufCount;       // running count of the used buffer. 
	void	fix_dups( int user_fd );
	char	cwd[ _POSIX_PATH_MAX ];
	File	*file;		// array allocated at run time
};

char *string_copy( const char *);

#endif



