/*
  This routine is for linking with the standalone checkpointing library.  Since
  programs linked that way should never attempt remote system calls, it is
  a fatal error if this routine ever gets called.
*/

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

int
REMOTE_syscall( int syscall_num, ... )
{
	EXCEPT( "Linked for standalone checkpointing, but called REMOTE_syscall()");
}
