#ifndef _IMAGE_H
#define _IMAGE_H

#include <sys/types.h>
#include <limits.h>
#include "machdep.h"

#define NAME_LEN 64
typedef long RAW_ADDR;
typedef int BOOL;

#if defined(TRUE)
#	undef TRUE
#	undef FALSE
#endif

#include "condor_constants.h"

#if defined(SUNOS41) || defined(ULTRIX42) || defined(ULTRIX43) || defined(IRIX4) || defined(IRIX5)
typedef int ssize_t; // should be included in <sys/types.h>, but some don't
#endif

const int MAGIC = 0xfeafea;
const int SEG_INCR = 25;
const int  MAX_SEGS = 25;

class Header {
public:
	void Init();
	void IncrSegs() { n_segs += 1; }
	int	N_Segs() { return n_segs; }
	void Display();
private:
	int		magic;
	int		n_segs;
	char	pad[ 1024 - 2 * sizeof(int) - NAME_LEN ];
};

class SegMap {
public:
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

typedef enum { STANDALONE, REMOTE } ExecutionMode;

class Image {
public:
	void Save();
	int	Write();
	int Write( int fd );
	int Write( const char *name );
	int Read();
	int Read( int fd );
	int Read( const char *name );
	void Close();
	void Restore();
	char *FindSeg( void *addr );
	void Display();
	void RestoreSeg( const char *seg_name );
	void SetFd( int fd );
	void SetFileName( char *ckpt_name );
	void SetMode( int syscall_mode );
	ExecutionMode	GetMode() { return mode; }
	size_t			GetLen()  { return len; }
	int				GetFd()   { return fd; }
protected:
	RAW_ADDR	GetStackLimit();
	void AddSegment( const char *name, RAW_ADDR start, RAW_ADDR end );
	void SwitchStack( char *base, size_t len );
	char	*file_name;
	Header	head;
	SegMap	map[ MAX_SEGS ];
	ExecutionMode	mode;	// executing in standalone/remote mode
	BOOL	valid;		// initialized and ready to write ckpt file or restore
	int		fd;		// descriptor pointing to ckpt file
	ssize_t	pos;	// position in ckpt file of seg currently reading/writing
	size_t	len;	// size of our ckpt file
};
void RestoreStack();

extern "C" void Checkpoint( int, int, void * );
extern "C" {
void ckpt();
extern "C" void restart();
extern "C" void init_image_with_file_name( char *ckpt_name );
extern "C" void init_image_with_file_descriptor( int ckpt_fd );
extern "C" void _condor_prestart( int syscall_mode );
}


#define DUMP( leader, name, fmt ) \
	printf( "%s%s = " #fmt "\n", leader, #name, name )


#include "setjmp.h"
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
#endif
