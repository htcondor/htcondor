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
	typedef void (*SIG_HANDLER)();
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
