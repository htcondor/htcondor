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
#include <sys/types.h>

typedef unsigned int	Bit;
typedef int				BOOL;
typedef int				FD;

class OpenFileTable;

class File {
friend class OpenFileTable;
public:
	void	Init();
	void	Display();
	BOOL	isOpen()			{ return open; }
	BOOL	isDup()				{ return duplicate; }
	BOOL	isPreOpened()		{ return pre_opened; }
	BOOL	isRemoteAccess()	{ return remote_access; }
	BOOL	isShadowSock()		{ return shadow_sock; }
	BOOL	isReadable()		{ return readable; }
	BOOL	isWriteable()		{ return writeable; }
private:
	Bit	open : 1;			// file is open
	Bit	duplicate : 1;		// file is dup of another fd
	Bit	pre_opened : 1;		// file was pre_opened (stdin, stdout, stderr)
	Bit	remote_access : 1;	// access via remote sys calls (via the shadow)
	Bit	shadow_sock : 1;	// TCP connection to the shadow
	Bit	readable : 1;		// File open for read or read/write
	Bit	writeable : 1;		// File open for write or read/write
	off_t	offset;			// File pointer position
	FD		real_fd;		// File descriptor number
	FD		dup_of;			// File descriptor this is a dup of
	char	*pathname;		// *FULL* pathname of the file
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
	int DoOpen( const char *path, int flags, int fd, int is_remote );
#endif
	int DoClose( int fd );
	int DoDup( int fd );
	int DoDup2( int fd, int dupfd );
	int DoSocket( int addr_family, int type, int protocol );
	int	Map( int user_fd );
	BOOL IsLocalAccess( int user_fd );
	BOOL IsDup( int user_fd );
private:
	int		find_avail( int start );
	void	fix_dups( int user_fd );
	File	*file;		// array allocated at run time
};

char *string_copy( const char *);

#endif
