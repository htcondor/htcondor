#include "image.h"

/*
  Return starting address of the data segment
*/
#include <sys/param.h>
extern int etext;
int
data_start_addr()
{
	return ((long)&etext + DATA_ALIGN - 1) / DATA_ALIGN * DATA_ALIGN;
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
	return env[StkPtrIdx] / 1024 * 1024; // Curr sp, rounded down - 1K boundary
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
	// Nothing needed
}
