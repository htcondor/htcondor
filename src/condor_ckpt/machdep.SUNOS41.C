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
