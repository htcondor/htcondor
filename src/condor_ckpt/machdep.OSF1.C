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
#include "condor_syscall_mode.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/vmparam.h>

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
		perror( "getaddressconf" );
		exit( 1 );
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
