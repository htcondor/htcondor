#include "image.h"

extern "C" char *sbrk( int );

#if defined(NULL)
#undef NULL			// sys/param defines this
#endif
#include <sys/param.h>
#include <sys/vmparam.h>

/*
  Return starting address of the data segment
*/
extern int __data_start;
int
data_start_addr()
{
	return USRDATA;
}

/*
  Return ending address of the data segment
*/
int
data_end_addr()
{
	return (int)sbrk(0);
}

/*
  Return TRUE if the stack grows toward lower addresses, and FALSE
  otherwise.
*/
int StackGrowsDown()
{
	return 1;
}

/*
  Return the index into the jmp_buf where the stack pointer is stored.
  Expect that the jmp_buf will be viewed as an array of integers for
  this.
*/
int JmpBufSP_Index()
{
	return 32;
}

/*
  Return starting address of stack segment.
*/
int
stack_start_addr()
{
	jmp_buf env;
	(void)SETJMP( env );
	return JMP_BUF_SP(env) / 1024 * 1024; // Curr sp, rounded down
}

/*
  Return ending address of stack segment.
*/
int
stack_end_addr()
{
	return USRSTACK;
}

/*
  Patch any registers whose vaules should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void *generic_ptr )
{
	// nothing needed
}
