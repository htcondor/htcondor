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

#ifndef _MACHDEP_H
#define _MACHDEP_H

#if defined(ULTRIX42)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)();
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(ULTRIX43)

	extern "C" char *brk( char * );
	extern "C" char *sbrk( int );
	typedef void (*SIG_HANDLER)();
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(SUNOS41)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( ... );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(SOLARIS2)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)();
#	define SETJMP setjmp
#	define LONGJMP longjmp

#elif defined(OSF1)

	extern "C" int brk( void * );
#	include <sys/types.h>
	extern "C" void *sbrk( ssize_t );
	typedef void (*SIG_HANDLER)( int );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(HPUX9)

	extern "C" int brk( const void * );
	extern "C" void *sbrk( int );
#	include <signal.h>
	typedef void (*SIG_HANDLER)( __harg );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(AIX32)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( int );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(IRIX4)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( int );
#	define SETJMP setjmp
#	define LONGJMP longjmp
	extern "C" int kill( pid_t, int );

#elif defined(IRIX5)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)();
#	define SETJMP setjmp
#	define LONGJMP longjmp
	extern "C" int kill( pid_t, int );

#else

#	error UNKNOWN PLATFORM

#endif

#endif
