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

#include "image.h"
#include <stdio.h>
#include <sys/procfs.h>		// for /proc calls
#include <sys/mman.h>		// for mmap() test
#include <sys/types.h>		// for open() and getpid()
#include <sys/stat.h>		// for open()
#include <fcntl.h>		// for open()
#include <unistd.h>		// for getpid()
#include <stdlib.h>		// for malloc() and free()
#include <sys/immu.h>
#include "condor_debug.h"
#include "condor_syscalls.h"
#undef SYSVoffset
#undef __SYS_S__
#include <sys.s>

/*
  Return starting address of the data segment
*/
long
data_start_addr()
{
	return USRDATA;
}

/*
  Return ending address of the data segment
*/
long
data_end_addr()
{
	return (long)sbrk( 0 );
}

/*
  Return TRUE if the stack grows toward lower addresses, and FALSE
  otherwise.
*/
BOOL StackGrowsDown()
{
	return TRUE;
}

/*
  Return the index into the jmp_buf where the stack pointer is stored.
  Expect that the jmp_buf will be viewed as an array of integers for
  this.
*/
int JmpBufSP_Index()
{
	return 39;
}

/*
  Return starting address of stack segment.
*/
long
stack_start_addr()
{
    jmp_buf env;

    (void)SETJMP( env );
    return JMP_BUF_SP(env) & ~1023; // Curr sp, rounded down
}

/*
  Return ending address of stack segment.
*/
long
stack_end_addr()
{
	return USERSTACK_32-1;
}

/*
  Patch any registers whose vaules should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void *generic_ptr )
{
}

static prmap_t my_map[ MAX_SEGS ];
static int	prmap_count = 0;
static int	text_loc = -1, stack_loc = -1, heap_loc = -1;

/*
  Find the segment in my_map which contains the address in addr.
  Used to find the text segment.
*/
int
find_map_for_addr(caddr_t addr)
{
	int		i;

	for (i = 0; i < prmap_count; i++) {
		if (addr >= my_map[i].pr_vaddr &&
			addr <= my_map[i].pr_vaddr + my_map[i].pr_size){
			return i;
		}
	}
	return -1;
}

/*
  Return the number of segments to be checkpointed.  Note that this
  number includes the text segment, which should be ignored.  On error,
  returns -1.
*/
int
num_segments( )
{
	int	fd;
	char	buf[100];

	int scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	sprintf(buf, "/proc/%d", SYSCALL(SYS_getpid));
	fd = SYSCALL(SYS_open, buf, O_RDWR, 0);
	if (fd < 0) {
		return -1;
	}
	SYSCALL(SYS_ioctl, fd, PIOCNMAP, &prmap_count);
	if (prmap_count > MAX_SEGS) {
		dprintf( D_ALWAYS, "Don't know how to grow segment map yet!\n" );
		Suicide();
	}		
	SYSCALL(SYS_ioctl, fd, PIOCMAP, my_map);
	/* find the text segment by finding where this function is
	   located */
	text_loc = find_map_for_addr((caddr_t) num_segments);
	/* identify the stack segment by looking for the bottom of the stack,
	   because the top of the stack may be in the data area, often the
	   case for programs which utilize threads or co-routine packages. */
	if ( StackGrowsDown() )
		stack_loc = find_map_for_addr((caddr_t) stack_end_addr());
	else
		stack_loc = find_map_for_addr((caddr_t) stack_start_addr());
	heap_loc = find_map_for_addr((caddr_t) data_start_addr());
	if (SYSCALL(SYS_close, fd) < 0) {
		dprintf(D_ALWAYS, "close: %s", strerror(errno));
	}
	SetSyscalls( scm );
	return prmap_count;
}

/*
  Assigns the bounds of the segment specified by seg_num to start
  and long, and the protections to prot.  Returns -1 on error, 1 if
  this segment is the text segment, 2 if this segment is the stack
  segment, 3 if this segment is in the data segment, and 0 otherwise.
*/
int
segment_bounds( int seg_num, RAW_ADDR &start, RAW_ADDR &end, int &prot )
{
	if (my_map == NULL)
		return -1;
	start = (long) my_map[seg_num].pr_vaddr;
	end = start + my_map[seg_num].pr_size;
	prot = my_map[seg_num].pr_mflags;
	if (seg_num == text_loc)
		return 1;
	else if (seg_num == stack_loc)
		return 2;
	else if (seg_num == heap_loc ||
		 (my_map[seg_num].pr_vaddr >= data_start_addr() &&
		  my_map[seg_num].pr_vaddr <= data_end_addr()))
	        return 3;
	return 0;
}

struct ma_flags {
	int	flag_val;
	char	*flag_name;
} MA_FLAGS[] = {{MA_READ, "MA_READ"},
				{MA_WRITE, "MA_WRITE"},
				{MA_EXEC, "MA_EXEC"},
				{MA_SHARED, "MA_SHARED"},
				{MA_BREAK, "MA_BREAK"},
				{MA_STACK, "MA_STACK"}};

/*
  For use in debugging only.  Displays the segment map of the current
  process.
*/
void
display_prmap()
{
	int		i, j;

	num_segments();
	for (i = 0; i < prmap_count; i++) {
	  dprintf( D_ALWAYS, "addr = 0x%p, size = 0x%lx, offset = 0x%x",
		 my_map[i].pr_vaddr, my_map[i].pr_size, my_map[i].pr_off);
	  for (j = 0; j < sizeof(MA_FLAGS) / sizeof(MA_FLAGS[0]); j++) {
	    if (my_map[i].pr_mflags & MA_FLAGS[j].flag_val) {
	      dprintf( D_ALWAYS, " %s", MA_FLAGS[j].flag_name);
	    }
	  }
	  dprintf( D_ALWAYS, "\n");
	}
}
