#include <sys/types.h>
#include "machdep.h"

#define NAME_LEN 64
typedef long RAW_ADDR;
typedef int BOOL;

#if defined(TRUE)
#	undef TRUE
#	undef FALSE
#endif

const BOOL	TRUE = 1;
const BOOL	FALSE = 0;

#if defined(SUNOS41) || defined(ULTRIX42) || defined(ULTRIX43)
typedef int ssize_t; // should be included in <sys/types.h>, but some don't
#endif

class Header {
public:
	Header();
	Header( int cluster, int proc, const char *owner );
	void Init( int cluster, int proc, const char *owner );
	void IncrSegs() { n_segs += 1; }
	int	N_Segs() { return n_segs; }
	void Display();
private:
	int		magic;
	int		n_segs;
	int		cluster;
	int		proc;
	char	owner[NAME_LEN];
	char	pad[ 1024 - 4 * sizeof(int) - NAME_LEN ];
};

class SegMap {
public:
	SegMap();
	SegMap( const char *name, RAW_ADDR core_loc, long len );
	void Init( const char *name, RAW_ADDR core_loc, long len );
	ssize_t Read( int fd, ssize_t pos );
	ssize_t Write( int fd, ssize_t pos );
	ssize_t SetPos( ssize_t my_pos );
	BOOL Contains( void *addr );
	char *GetName() { return name; }
	void Display();
private:
	char		name[14];
	off_t		file_loc;
	RAW_ADDR	core_loc;
	long		len;
};

class Image {
public:
	Image();
	void Save();
	void Save( int cluster, int proc, const char *owner );
	int Write( int fd );
	int Write( const char *name );
	int Read( int fd );
	int Read( const char *name );
	void Restore();
	char *FindSeg( void *addr );
	void Display();
	void RestoreSeg( const char *seg_name );
protected:
	RAW_ADDR	GetStackLimit();
	void AddSegment( const char *name, RAW_ADDR start, RAW_ADDR end );
	void SwitchStack( char *base, size_t len );
	Header	head;
	int		max_segs;
	SegMap	*map;
	BOOL	valid;
	int		fd;
	ssize_t	pos;
};
void RestoreStack();

extern "C" void Checkpoint( int, int, void * );
extern "C" {
void ckpt();
void restart();
}


#define DUMP( leader, name, fmt ) \
	printf( "%s%s = " #fmt "\n", leader, #name, name )

const int MAGIC = 0xfeafea;
const int SEG_INCR = 25;

#if defined(AIX32)
#include "setjmp.AIX32.h"
#else
#include <setjmp.h>
#endif

#define SETJMP _setjmp
#define LONGJMP _longjmp
extern "C" {
	int SETJMP( jmp_buf env );
	void LONGJMP( jmp_buf env, int retval );
}

long data_start_addr();
long data_end_addr();
long stack_start_addr();
long stack_end_addr();
BOOL StackGrowsDown();
int JmpBufSP_Index();
void ExecuteOnTmpStk( void (*func)() );
void patch_registers( void  *);

#	define JMP_BUF_SP(env) (((long *)(env))[JmpBufSP_Index()])
