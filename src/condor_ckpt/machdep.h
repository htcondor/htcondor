#if defined(ULTRIX42)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)();

#elif defined(ULTRIX43)

	extern "C" char *brk( char * );
	extern "C" char *sbrk( int );
	typedef void (*SIG_HANDLER)();

#elif defined(SUNOS41)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)();

#elif defined(OSF1)

	extern "C" int brk( void * );
#	include <sys/types.h>
	extern "C" void *sbrk( ssize_t );
	typedef void (*SIG_HANDLER)( int );

#elif defined(HPUX9)

	extern "C" int brk( const void * );
	extern "C" void *sbrk( int );
#	include <signal.h>
	typedef void (*SIG_HANDLER)( __harg );

#elif defined(AIX32)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( int );

#else

#	error UNKNOWN PLATFORM

#endif
