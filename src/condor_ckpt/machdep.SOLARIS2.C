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
#include <stdio.h>

/* a few notes:
 * this is a pretty rough port
 *
 * data_start_addr() is basically an educated guess based on dump(1)
 * it is probably not entirely correct, but does seem to work!
 *
 * stack_end_addr() is generally well known for sparc machines
 * however it doesn't seem to appear in solaris header files
 * 
 * JmpBufSP_Index() was derived by dumping the jmp_buf, allocating a
 * large chunk of memory on the stack, and dumping the jmp_buf again
 * whichever value changed by about sizeof(chuck) is the stack pointer
 *
 */

/*
  Return starting address of the data segment
*/
#include <sys/elf_SPARC.h>
extern int _etext;
long
data_start_addr()
{
	return (  (((long)&_etext) + ELF_SPARC_MAXPGSZ) + 0x8 - 1 ) & ~(0x8-1);
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
	return 1;	
}

/*
  Return starting address of stack segment.
*/
long
stack_start_addr()
{
	long	answer;

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
	/* return 0xF8000000; -- for sun4[c] */
	return 0xF0000000; /* -- for sun4m */
}

/*
  Patch any registers whose values should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void *generic_ptr )
{
	// Nothing needed
}
