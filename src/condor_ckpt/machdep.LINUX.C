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

 

#include "image.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include "condor_debug.h"
#include "condor_syscalls.h"
#define __KERNEL__
#include <asm/page.h>

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
	return 4;
}

/*
  Return starting address of stack segment.
*/
long
stack_start_addr()
{
	jmp_buf env;

	(void)SETJMP(env);

	return (long)JMP_BUF_SP(env) & PAGE_MASK;
}

/*
  Return ending address of stack segment.
*/
long
stack_end_addr()
{
	return (long)0xbfffffff;
}

/*
  Patch any registers whose vaules should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void *generic_ptr )
{
	//Any Needed??  Don't think so.
}
