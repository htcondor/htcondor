#include "image.h"

/*
  Return starting address of the data segment
*/
long
data_start_addr()
{
}

/*
  Return ending address of the data segment
*/
long
data_end_addr()
{
}

/*
  Return TRUE if the stack grows toward lower addresses, and FALSE
  otherwise.
*/
BOOL StackGrowsDown()
{
}

/*
  Return the index into the jmp_buf where the stack pointer is stored.
  Expect that the jmp_buf will be viewed as an array of integers for
  this.
*/
int JmpBufSP_Index()
{
}

/*
  Return starting address of stack segment.
*/
long
stack_start_addr()
{
}

/*
  Return ending address of stack segment.
*/
long
stack_end_addr()
{
}

/*
  Patch any registers whose vaules should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void *generic_ptr )
{
}
