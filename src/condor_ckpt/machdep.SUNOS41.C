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
