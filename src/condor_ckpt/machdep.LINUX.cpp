/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 

#include "image.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include "condor_debug.h"
#include "condor_syscalls.h"
#define __KERNEL__

#if defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
#include <sys/user.h>
#else
#include <asm/page.h>
#endif

/* This is supposed to be defined in the above headerfile, but on x86_64 on 
rhel5 the file in question leads to an empty headerfile with no definition
for PAGE_SIZE and friends. So for now, I'll best guess one. */
#if defined(LINUX) && defined(GLIBC25) && defined(X86_64) && !defined(PAGE_SIZE)
#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)
#define PAGE_MASK       (~(PAGE_SIZE-1))

#define LARGE_PAGE_MASK (~(LARGE_PAGE_SIZE-1))
#define LARGE_PAGE_SIZE (1UL << PMD_SHIFT)
#endif

/* needed to tell us where the approximate begining of the data sgmt is */
#if defined (GLIBC)
extern int __data_start;
#else
extern char **__environ;
#endif

/*
  Return starting address of the data segment
*/
long
data_start_addr()
{
	unsigned long addr = 0;

	/* I'm so sorry about this.... I couldn't find a good way to calculate
		the beginning of the data segment, so I just found the first variable
		defined the earliest in the executable, and this was it. Hopefully
		this variable is close to the beginning of the data segment. Also,
		the beginning is dictated in the elf file format, so the linker
		really decides where to put the data segment, not the kernel. :(
		-pete 07/01/99 */
#if defined(GLIBC)	
	addr = ((unsigned long)&__data_start) & PAGE_MASK;
#else
	addr = ((unsigned long)&__environ) & PAGE_MASK;
#endif

	return addr;
}

/*
  Return ending address of the data segment
*/
long
data_end_addr()
{
	return (long)sbrk(0);
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

/* look in /usr/include/bits/setjmp.h */

#if defined(I386)

	/* In a 32-bit x86 jmp_buf layout, this is the stack index variable */
	return 4;

#elif defined(X86_64)

	/* In a 64-bit x86_64 jmp_buf layout, this is the stack index variable */
	return 6;

#endif
}

/*
  Return starting address of stack segment.
*/
long
stack_start_addr()
{
	jmp_buf env;
	unsigned long addr;

	(void)SETJMP(env);

	addr = (long)JMP_BUF_SP(env);
	PTR_DECRYPT(addr);

	return addr & PAGE_MASK;
}

/*
  Return ending address of stack segment.

  This value is a constant for any given Linux system, but depends
  on the memory configuration given when the kernel was compiled.
  On 1GB systems, this will be 0xc0000000.  On 2GB systems, it will
  be 0x80000000, and so on. On 2.6 kernels with the fast syscall page
  and the exec_domain set to PER_LINUX32 (i386), it will be 0xff000000--
  I don't know if there is a large/small memory model yet.

  The environment is always stored very close to the top of the stack,
  so we nab that value and lop off any page offset.  That gives us the
  bottom of the topmost page, i.e. 0x7ffff000.  To get the entire
  contents of that page, we add one more page and subtract one, i.e.
  0x7ffffff.
*/
long
stack_end_addr()
{
	return (((long)*environ)&PAGE_MASK) + PAGE_SIZE - 1;
}

/*
  Patch any registers whose vaules should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void * /*generic_ptr*/ )
{
	//Any Needed??  Don't think so.
}

#ifdef LINUX_SUPPORTS_SHARED_LIBRARIES

// Support for shared library linking
// Written by Michael J. Greger (greger@cae.wisc.edu) August, 1996
// Based on Solaris port by Mike Litzkow


struct map_t {
	long	mem_start;
	long	mem_end;
	long	offset;
	int		prot;
	int		flags;
	int		inode;
	char	r, w, x, p;
};

static map_t	my_map[MAX_SEGS];
static int		map_count=0;
static int		text_loc=-1, stack_loc=-1, heap_loc=-1;

int find_map_for_addr(long addr)
{
	int		i;

	/*fprintf(stderr, "Finding map for addr:0x%x (map_cnt=%d)\n", addr, map_count);*/
	for(i=0;i<map_count;i++) {
		/*fprintf(stderr, "0x%x 0x%x\n", my_map[i].mem_start, my_map[i].mem_end);*/
		if(addr >= my_map[i].mem_start && addr <= my_map[i].mem_end) {
			/*fprintf(stderr, "  Found:%d\n", i);*/
			return i;
		}
	}
	/*fprintf(stderr, "  NOT Found\n");*/
	return -1;
}


int num_segments()
{
	FILE	*pfs;
	char	proc[128];
	char	rperm, wperm, xperm, priv;
	int		scm;
	int		num_seg=0;
	int		major, minor, inode;
	int		mem_start, mem_end;
	int		offset;
	char	e, f;


	scm=SetSyscalls(SYS_LOCAL | SYS_UNMAPPED);

	sprintf(proc, "/proc/%d/maps", syscall(SYS_getpid));
	pfs=safe_fopen_wrapper(proc, "r");
	if(!pfs) {
		SetSyscalls(scm);
		return -1;
	}

	// Count the numer of mmapped files in use by the executable
	while(1) {
		int result = fscanf(pfs, "%x-%x %c%c%c%c %x %d:%d %d\n",
			&mem_start, &mem_end, &rperm, &wperm,
			&xperm, &priv, &offset, &major, &minor, &inode);

		if(result!=10) break;

		/*fprintf(stderr, "0x%x 0x%x %c%c%c%c 0x%x %d:%d %d\n", 
			mem_start, mem_end, rperm, wperm, xperm, priv,
			offset, major, minor, inode);*/
		my_map[num_seg].mem_start=mem_start;
		my_map[num_seg].mem_end=mem_end-1;
		/* FIXME - Greger */
		if(my_map[num_seg].mem_end-my_map[num_seg].mem_start==0x34000-1)
			my_map[num_seg].mem_end=my_map[num_seg].mem_start+0x320ff;
		my_map[num_seg].offset=offset;
		my_map[num_seg].prot=(rperm=='r'?PROT_READ:0) |
							 (wperm=='w'?PROT_WRITE:0) |
							 (xperm=='x'?PROT_EXEC:0);
		my_map[num_seg].flags=(priv=='p'?:0);
		my_map[num_seg].inode=inode;
		my_map[num_seg].r=rperm;
		my_map[num_seg].w=wperm;
		my_map[num_seg].x=xperm;
		my_map[num_seg].p=priv;
		num_seg++;
	}
	fclose(pfs);

	map_count=num_seg;
	text_loc=find_map_for_addr((long)num_segments);
	if(StackGrowsDown())
		stack_loc=find_map_for_addr((long)stack_end_addr());
	else
		stack_loc=find_map_for_addr((long)stack_start_addr());
	heap_loc=find_map_for_addr((long)data_start_addr());

	/*fprintf(stderr, "%d segments\n", num_seg);*/
	SetSyscalls(scm);
	return num_seg;
}

int segment_bounds(int seg_num, RAW_ADDR &start, RAW_ADDR &end, int &prot)
{
	start=(long)my_map[seg_num].mem_start;
	end=(long)my_map[seg_num].mem_end;
	prot=my_map[seg_num].prot;
	if(seg_num==text_loc)
		return 1;
	else if(seg_num==stack_loc)
		return 2;
	else if(seg_num==heap_loc || 
		(my_map[seg_num].mem_start >= data_start_addr() &&
		 my_map[seg_num].mem_start <= data_end_addr()))
	 	return 3;
	//else if(!my_map[seg_num].inode)
		//return 1;

	return 0;
}

struct ma_flags {
	int 	flag_val;
	char	*flag_name;
};

void display_prmap()
{
	int		i;

	for(i=0;i<map_count;i++) {
		dprintf(D_ALWAYS, "addr= 0x%p, size= 0x%x, offset= 0x%x\n",
			my_map[i].mem_start, my_map[i].mem_end-my_map[i].mem_start,
			my_map[i].offset);
		dprintf(D_ALWAYS, "Flags: %c%c%c%c inode %d\n", 
			my_map[i].r, my_map[i].w, my_map[i].x, my_map[i].p,
			my_map[i].inode);
	}
}

/*
 * This is a hack!  The values found in /proc/pid/maps do not always seem
 * to be correct.  This function works its way down from the end addr
 * until it finds an addr that we can read from... - Greger
 */
unsigned long find_correct_vm_addr(unsigned long start, unsigned long end,
	int prot)
{
	int scm=SetSyscalls(SYS_LOCAL | SYS_UNMAPPED);

	if((mprotect((char *)start, end-start, PROT_READ))==0) {
		/*
		 * We were allowed to do read access to the chunk.  Change
		 * protection back to what it was before the call.
		 */
		if((mprotect((char *)start, end-start, prot))==0) {
			SetSyscalls(scm);
			return end;
		} else {
			dprintf(D_ALWAYS, "Fatal error, Can't restore memory protection\n");
		}
	} else {
		/*
		 * We were not allowed read permission to the whole block.  Find
		 * a smaller block that we do have access to...
		 */
		for(;end>start;end-=4) { /* Most lib stuff is alligned on 4 bytes or less??... */
			if((mprotect((char *)start, end-start, PROT_READ))==0) {
				/*
		 		* We were allowed to do read access to the chunk.  Change
		 		* protection back to what it was before the call.
		 		*/
				if((mprotect((char *)start, end-start, prot))==0) {
					SetSyscalls(scm);	
					return end;
				} else {
					dprintf(D_ALWAYS, "Fatal error, Can't restore memory protection\n");
				}
			}
		}
		dprintf(D_ALWAYS, "Fatal error, Can't read any of this block\n");
	}
	SetSyscalls(scm);
}

#endif
