#include "image.h"

/*
  Return starting address of the data segment
*/
#include <sys/param.h>
extern int etext;
long
data_start_addr()
{
	return ((long)&etext + DATA_ALIGN - 1) / DATA_ALIGN * DATA_ALIGN;
}

/*
  Return ending address of the data segment
*/
long
data_end_addr()
{
	return (long)sbrk(0);
}

/*
  Return TRUE if the stack grows toward lower addresses, and FALSE
  otherwise.
*/
BOOL StackGrowsDown()
{
	return TRUE;
}

/*
  Return the index into the jmp_buf where the stack pointer is stored.
  Expect that the jmp_buf will be viewed as an array of integers for
  this.
*/
int JmpBufSP_Index()
{
	return 2;
}

/*
  Return starting address of stack segment.
*/
long
stack_start_addr()
{
	long	answer;

	jmp_buf env;
	(void)SETJMP( env );
	return JMP_BUF_SP(env) & ~1023; // Curr sp, rounded down
}

/*
  Return ending address of stack segment.
*/
#include <sys/vmparam.h>
long
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
	// Nothing needed
}
