#include <limits.h>
#include <sys/types.h>

typedef unsigned int	Bit;
typedef int				BOOL;
typedef unsigned short	FD;

class OpenFileTable;

class File {
friend class OpenFileTable;
public:
	File();
	~File();
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
	FD		fd_num;			// File descriptor number
	FD		dup_of;			// File descriptor this is a dup of
	char	*pathname;		// *FULL* pathname of the file
};

class OpenFileTable {
public:
	OpenFileTable();
	void Display();
	int RecordOpen( int fd, const char *path, int flags, int method );
	void RecordClose( int fd );
	int RecordPreOpen( int fd );
	int RecordDup( int fd );
	int RecordDup2( int fd, int dupfd );
	int	Map( int user_fd );
private:
	int		find_avail( int start );
	File	file[_POSIX_OPEN_MAX];
};

char *string_copy( const char *);
