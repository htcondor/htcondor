#include "image.h"

/*
  Find starting and ending addresses of the data segment
*/
#if defined(ULTRIX42) || defined(ULTRIX43)
	// Data starts at well known value USRDATA
	// Data ends at sbrk(0)
#	undef NULL
	int data_start_addr() { return USRDATA; }
	int data_end_addr() { return (int)sbrk(0); }
#elif defined(SUNOS41)
	// Data starts at first DATA_ALIGN boundary after end of text
	// Data ends at sbrk(0)
	extern int etext;
	int
	data_start_addr()
	{
		return ((long)&etext + DATA_ALIGN - 1) / DATA_ALIGN * DATA_ALIGN;
	}
	int data_end_addr() { return (int)sbrk(0); }
#else
	// Unknown platforms fall through to here, generating compile time error
#	error
#endif


/*
  Stuff to manipulate the stack pointer using setjmp() and longjmp().
*/
#if defined(ULTRIX42) || defined(ULTRIX43)
	const int StkPtrIdx = 32;
	const int StackGrowsDown = 1;
#elif defined(SUNOS41)
	const int StkPtrIdx = 2;
	const int StackGrowsDown = 1;
#else
	// Unknown platforms fall through to here, generating compile time error
#	error
#endif

/*
  Find starting address of stack segment.

  Portability: A single method works on all the platforms we have tried
  so far, but this will not be true for all platforms.
*/
int
stack_start_addr()
{
#if defined(ULTRIX42) || defined(ULTRIX43) || defined(SUNOS41)
	jmp_buf env;
	(void)SETJMP( env );
	return env[StkPtrIdx] / 1024 * 1024; // Curr sp, rounded down - 1K boundary
#else
	// Unknown platforms fall through to here, generating compile time error
#	error
#endif
}

/*
  Find ending address of stack segment.

  Portability: A single method works on all the platforms we have tried
  so far, but this will not be true for all platforms.
*/
int
stack_end_addr()
{
#if defined(ULTRIX42) || defined(ULTRIX43) || defined(SUNOS41)
	return USRSTACK; // Well known value
#else
	// Unknown platforms fall through to here, generating compile time error
#	error
#endif
}


/*
  Switch to a temporary stack area in the DATA segment, then execute the
  given function.  Note: we save the address of the function in a
  global data location - referencing a value on the stack after the SP
  is moved would be an error.

  Portability: Given the above machine dependent definitions of SETJMP,
  LONGJMP, and StkPtrIdx, it is likely that this code won't have to
  vary for different platforms.
*/
static void (*SaveFunc)();

const int	TmpStackSize = 4096;
static char	TmpStack[ TmpStackSize ];
void
ExecuteOnTmpStk( void (*func)() )
{
	jmp_buf	env;
	SaveFunc = func;

	if( SETJMP(env) == 0 ) {
			// First time through - move SP
		if( StackGrowsDown ) {
			env[StkPtrIdx] = (int)TmpStack + TmpStackSize;
		} else {
			env[StkPtrIdx] = (int)TmpStack;
		}
		LONGJMP( env, 1 );
	} else {
			// Second time through - call the function
		SaveFunc();
	}
}
