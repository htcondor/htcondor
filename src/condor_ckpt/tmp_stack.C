#include "image.h"
/*
  Switch to a temporary stack area in the DATA segment, then execute the
  given function.  Note: we save the address of the function in a
  global data location - referencing a value on the stack after the SP
  is moved would be an error.
*/
static void (*SaveFunc)();
static jmp_buf Env;

const int	TmpStackSize = 4096;
static char	TmpStack[ TmpStackSize ];
void
ExecuteOnTmpStk( void (*func)() )
{
	jmp_buf	env;
	SaveFunc = func;

	if( SETJMP(env) == 0 ) {
			// First time through - move SP
		if( StackGrowsDown() ) {
			JMP_BUF_SP(env) = (long)TmpStack + TmpStackSize;
		} else {
			JMP_BUF_SP(env) = (long)TmpStack;
		}
		LONGJMP( env, 1 );
	} else {
			// Second time through - call the function
		SaveFunc();
	}
}
