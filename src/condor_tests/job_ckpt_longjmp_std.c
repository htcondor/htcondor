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
#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>

void ThreadExecute( void (*func)() );
void ThreadReturn();
void func();
void loop( int n );
void check_buf( char *buf, char fill_char, size_t size );
void fill_buf( char *buf, char fill_char, size_t size );
#include "x_fake_ckpt.h"
#include "x_waste_second.h"


int
main( int argc, char *argv[] )
{
	int		i;
	char	buf[ 1024 * 1024 ];
	char	fill_char = 'a';

	for( i = 0; i<10; i++, fill_char++ ) {
		printf( "i = %d\n", i );
		printf( "This should be a STACK address 0x%p\n", &i );
		fflush( stdout );
		fill_buf( buf, fill_char, sizeof(buf) );

		printf( "About to Ckpt from STACK\n" );
		fflush( stdout );
		ckpt_and_exit();
		printf( "Returned from ckpt_and_exit\n" );
		fflush( stdout );

		loop( 1 );
		ThreadExecute( func );
		check_buf( buf, fill_char, sizeof(buf) );
	}
	printf( "Normal End_Of_Job\n" );
	exit( 0 );
}

void
fill_buf( char *buf, char fill_char, size_t size )
{
	size_t i;

	for( i=0; i<size; i++ ) {
		buf[i] = fill_char;
	}
}

void
check_buf( char *buf, char fill_char, size_t size )
{
	size_t i;

	for( i=0; i<size; i++ ) {
		assert( buf[i] == fill_char );
	}
}

void
func()
{
	int		i;

	printf( "This should be a DATA address 0x%p\n", &i );
	fflush( stdout );

	printf( "About to Ckpt from DATA\n" );
	fflush( stdout );
	ckpt_and_exit();
	printf( "Returned from ckpt_and_exit\n" );
	fflush( stdout );

	loop( 2 );
	ThreadReturn();
}

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

/* IRIX can use 64 bit numbers for the elements if the jump buf, while 
	long is defined to be 32 bits. */
#if defined(IRIX)
#define JMP_BUF_SP(env) (((env))[JMP_BUF_SP_INDEX])
#else
#define JMP_BUF_SP(env) (((long *)(env))[JMP_BUF_SP_INDEX])
#endif


#if defined(ULTRIX43)
#	define SETJMP _setjmp
#	define LONGJMP _longjmp
#	define StackGrowsDown TRUE
#	define JMP_BUF_SP_INDEX 32

#elif defined(SUNOS41)
#   define SETJMP _setjmp
#   define LONGJMP _longjmp
#	define StackGrowsDown TRUE
#	define JMP_BUF_SP_INDEX 2

#elif defined(SOLARIS2) || defined(Solaris)
#   define SETJMP setjmp
#   define LONGJMP longjmp
#	define StackGrowsDown TRUE
#if defined(X86)
#	define JMP_BUF_SP_INDEX 4
#else
#	define JMP_BUF_SP_INDEX 1
#endif

#elif defined(OSF1)
#   define SETJMP _setjmp
#   define LONGJMP _longjmp
#	define StackGrowsDown TRUE
#	define JMP_BUF_SP_INDEX 34

#elif defined(HPUX)
#   define SETJMP setjmp
#   define LONGJMP longjmp
#	define StackGrowsDown FALSE
#	define JMP_BUF_SP_INDEX 1

#elif defined(AIX32)
#   define SETJMP _setjmp
#   define LONGJMP _longjmp
#	define StackGrowsDown TRUE
#	define JMP_BUF_SP_INDEX 3

#elif defined(HPUX)
#   define SETJMP _setjmp
#   define LONGJMP _longjmp
#	define StackGrowsDown FALSE
#	define JMP_BUF_SP_INDEX 1

#elif defined(LINUX)
#   define StackGrowsDown TRUE
#   define JMP_BUF_SP_INDEX 4
#   define SETJMP setjmp
#   define LONGJMP longjmp

#elif defined(IRIX62)
#	define SETJMP setjmp
#	define LONGJMP longjmp
#	define StackGrowsDown TRUE
#	define JMP_BUF_SP_INDEX 39

#elif defined(IRIX65)
#	define SETJMP setjmp
#	define LONGJMP longjmp
#	define StackGrowsDown TRUE
#	define JMP_BUF_SP_INDEX 34

#else
#   error UNKNOWN PLATFORM
#endif

/*
  Switch to a temporary stack area in the DATA segment, then execute the
  given function.  Note: we save the address of the function in a
  global data location - referencing a value on the stack after the SP
  is moved would be an error.
*/
static void (*SaveFunc)();
static jmp_buf ReturnEnv;

/*
// The stack is to be aligned on an 8-byte boundary on some
// machines. This is required so that double's can be pushed
// on the stack without any alignment problems.
*/

/*
// Solaris seems to push some really large stack frames on during
// checkpoint....  -Jim B.
// Lately, we had added lots of code to the checkpointing process so this
// variable needs to be bigger on some platforms. I figure I'll make it
// "big enough". :) -psilord
// This test used to be C++, and we used to define the size of the
// TmpStack programatically.  However, to ensure we can compile and
// run this test with more compilers, I'm converting it to regular C
// code, where the following is illegal:
//
// const int	TmpStackSize = (sizeof(char)*524288)/sizeof(double);
// static double	TmpStack[ TmpStackSize ];
//
// So, now we do it the less-fancy way, but it works fine on C or C++
*/
#define TmpStackSize 65536
static double	TmpStack[ TmpStackSize ];

void
ThreadExecute( void (*func)() )
{
	jmp_buf	env;
	SaveFunc = func;

	if( SETJMP(ReturnEnv) == 0 ) {
		if( SETJMP(env) == 0 ) {
				// First time through - move SP
			if( StackGrowsDown ) {
				JMP_BUF_SP(env) = (long)(TmpStack + TmpStackSize - 2);
			} else {
				JMP_BUF_SP(env) = (long)TmpStack;
			}
			LONGJMP( env, 1 );
		} else {
				/* Second time through - call the function */
			SaveFunc();
		}
	} else {
		return;
	}
}

void
ThreadReturn()
{
	LONGJMP( ReturnEnv, 1 );
}


void
loop( int n )
{
	int		i;

	for( i=0; i<n; i++ ) {
		x_waste_a_second();
	}

}
