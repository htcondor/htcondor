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

 

#ifndef _IMAGE_H
#define _IMAGE_H

#include "condor_common.h"
#include "machdep.h"


#define NAME_LEN 64
typedef unsigned long RAW_ADDR;
typedef int BOOL;

const int MAGIC = 0xfeafea;
const int COMPRESS_MAGIC = 0xfeafeb;
const int SEG_INCR = 25;
const int  MAX_SEGS = 200;
const int ALT_HEAP_SIZE = 10*1024*1024;	// 10MB
const int RESERVED_HEAP = 1024*1024*1024; // 1GB

class Header {
public:
	void Init();
	void IncrSegs() { n_segs += 1; }
	int	N_Segs() { return n_segs; }
	RAW_ADDR AltHeap() { return alt_heap; }
	void AltHeap(RAW_ADDR loc) { alt_heap = loc; }
	void Display();
	int Magic() { return magic; }
	void ResetMagic();
private:
	int		magic;
	int		n_segs;
	RAW_ADDR	alt_heap;
	char	pad[ 1024 - 2 * sizeof(int) - NAME_LEN - sizeof(RAW_ADDR)];
};

class SegMap {
public:
	void Init( const char *name, RAW_ADDR core_loc, long len, int prot );
	ssize_t Read( int fd, ssize_t pos );
	ssize_t Write( int fd, ssize_t pos );
	ssize_t SetPos( ssize_t my_pos );
	BOOL Contains( void *addr );
	char *GetName() { return name; }
	RAW_ADDR GetLoc() { return core_loc; }
	long GetLen() { return len; }
	void MSync();
	void Display();
private:
	char		name[14];
	off_t		file_loc;
	RAW_ADDR	core_loc;
	long		len;
	int			prot;		// segment protection mode
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
	void RestoreAllSegsExceptStack();
	void SetFd( int fd );
	void SetFileName( char *ckpt_name );
	void SetMode( int syscall_mode );
	void MSync();
#if defined(COMPRESS_CKPT)
	void *FindAltHeap();
#endif
	ExecutionMode	GetMode() { return mode; }
	size_t			GetLen()  { return len; }
	int				GetFd()   { return fd; }
protected:
	RAW_ADDR	GetStackLimit();
	void AddSegment( const char *name, RAW_ADDR start, RAW_ADDR end,
			int prot );
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

/* We would like to access the global image from elsewhere. */
extern Image MyImage;

void RestoreStack();

#if defined(HPUX10)
extern "C" void Checkpoint( int, siginfo_t *, void * );
#else
extern "C" void Checkpoint( int, int, void * );
#endif

extern "C" {
	void ckpt();
	void restart();
	void init_image_with_file_name( char *ckpt_name );
	void init_image_with_file_descriptor( int ckpt_fd );
	void _condor_prestart( int syscall_mode );
	void Suicide();
}

#define DUMP( leader, name, fmt ) \
	printf( "%s%s = " #fmt "\n", leader, #name, name )


#include "condor_fix_setjmp.h"


long data_start_addr();
long data_end_addr();
long stack_start_addr();
long stack_end_addr();
BOOL StackGrowsDown();
int JmpBufSP_Index();
void ExecuteOnTmpStk( void (*func)() );
void patch_registers( void  *);
#if defined(Solaris) || defined(IRIX)
     int find_map_for_addr(caddr_t addr);
     int num_segments( );
     int segment_bounds( int seg_num, RAW_ADDR &start, RAW_ADDR &end,
	int &prot );
     void display_prmap();
	 extern "C" int open_ckpt_file( const char *name,
								   int flags, size_t n_bytes );
#endif
#if defined(LINUX)
extern "C" {
     int find_map_for_addr(long addr);
     int num_segments( );
     int segment_bounds( int seg_num, RAW_ADDR &start, RAW_ADDR &end,
	int &prot );
     void display_prmap();
     unsigned long find_correct_vm_addr(unsigned long, unsigned long, int);
};
#endif

#if defined(IRIX)
#	define JMP_BUF_SP(env) ((env)[JmpBufSP_Index()])
#else
#	define JMP_BUF_SP(env) (((long*)(env))[JmpBufSP_Index()])
#endif

#endif
