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
