/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
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
** Author:  Mike Litzkow
**
*/ 

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
	int Map( int user_fd );
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
	int InsertR( int, void *, int, int, int, BufElem *, BufElem * );
	int InsertW( int, const void *, int, int, BufElem *, BufElem * );
	int OverlapR1( int, void *, int, int, int, BufElem *, BufElem * );
	int OverlapR2( int, void *, int, int, int, BufElem *, BufElem * );
	int OverlapW1( int, const void *, int, int, BufElem *, BufElem * );
	int OverlapW2( int, const void *, int, int, BufElem *, BufElem * );
	int PreFetch( int, void *, size_t );
	int Buffer( int, const void *, size_t );
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



