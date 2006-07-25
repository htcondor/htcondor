/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_syscall_mode.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include "condor_debug.h"
#include <string.h>

extern "C" {
#include <sys/addrconf.h>
}

static struct addressconf AddrTab[ AC_N_AREAS ];
const int AddrTabSize = sizeof( AddrTab );

void
init_addr_tab()
{
	static BOOL initialized = FALSE;
	int		scm;

	if( initialized ) {
		return;
	}

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	if( getaddressconf(AddrTab,AddrTabSize) != AddrTabSize ) {
		SetSyscalls( scm );
		dprintf( D_ALWAYS, "getaddressconf", strerror(errno) );
		Suicide();
	}
	SetSyscalls( scm );
}

/*
  Return starting address of the data segment
*/
long
data_start_addr()
{
	init_addr_tab();
	return (long)AddrTab[ AC_DATA ].ac_base;
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
	unsigned flag;

	init_addr_tab();
	flag = AddrTab[AC_STACK].ac_flags;
	switch( flag ) {
	  case AC_UPWARD:
		return 0;
	  case AC_DOWNWARD:
		return 1;
	  default:
		fprintf( stderr, "Unexpected ac_flags for stack (0x%X)\n", flag );
		exit( 1 );
	}
}

/*
  Return the index into the jmp_buf where the stack pointer is stored.
  Expect that the jmp_buf will be viewed as an array of integers for
  this.
*/
int JmpBufSP_Index()
{
	return 34;
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
	return 0x120000000;
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
