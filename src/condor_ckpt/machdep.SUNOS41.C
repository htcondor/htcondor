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

/*
  Return starting address of the data segment
*/
#include <sys/param.h>
extern int etext;
long
data_start_addr()
{
	return ((long)&etext + DATA_ALIGN - 1) / DATA_ALIGN * DATA_ALIGN;
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
	return 2;
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
#include <sys/vmparam.h>
#include <sys/utsname.h>
#include "condor_syscall_mode.h"
long
stack_end_addr()
{
    struct utsname buf;
	int		status;
	int		scm;

    /*
      We do this becuase on some gcc installations, the sun4m
      include file gets used on sun4c machines, and the value
      given for USRSTACK is wrong.
    */

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
    status = uname( &buf );		// do on the executing machine - not remotely
	SetSyscalls( scm );

    if( status < 0 ) {
        return USRSTACK;  // hope it's a sun4m...
    }
    if( strcmp(buf.machine,"sun4c") == 0 ) {
        return 0xf8000000;
    }

    return USRSTACK;
}

/*
  Patch any registers whose vaules should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void *generic_ptr )
{
	// Nothing needed
}
