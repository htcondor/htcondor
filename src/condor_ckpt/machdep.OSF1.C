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
