#include "image.h"

/*
  Return starting address of the data segment
*/
extern int __data_start;
int
data_start_addr()
{
}

/*
  Return ending address of the data segment
*/
int
data_end_addr()
{
}

/*
  Return TRUE if the stack grows toward lower addresses, and FALSE
  otherwise.
*/
int StackGrowsDown()
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
int
stack_start_addr()
{
}

/*
  Return ending address of stack segment.
*/
int
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
