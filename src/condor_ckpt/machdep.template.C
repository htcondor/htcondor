/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

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
