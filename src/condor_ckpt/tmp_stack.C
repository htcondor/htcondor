/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#include "image.h"
/*
  Switch to a temporary stack area in the DATA segment, then execute the
  given function.  Note: we save the address of the function in a
  global data location - referencing a value on the stack after the SP
  is moved would be an error.
*/
static void (*SaveFunc)();
static jmp_buf Env;

// The stack is to be aligned on an 8-byte boundary on some
// machines. This is required so that double's can be pushed
// on the stack without any alignment problems.

#if defined(LINUX)
/* Not sure why I have to do this... */
const int	TmpStackSize = sizeof(char)*65536/sizeof(double);
#else
const int	TmpStackSize = sizeof(char)*4096/sizeof(double);
#endif
static double	TmpStack[ TmpStackSize ];
void
ExecuteOnTmpStk( void (*func)() )
{
	jmp_buf	env;
	SaveFunc = func;

	if( SETJMP(env) == 0 ) {
			// First time through - move SP
		if( StackGrowsDown() ) {
			// when StackGrowsDown, set the stack pointer inside the
			// TmpStack and give some padding -- previously values
			// were getting overwritten above the stack -- Jim B. 2/7/96
			JMP_BUF_SP(env) = (long)(TmpStack + TmpStackSize - 2);
		} else {
			JMP_BUF_SP(env) = (long)TmpStack;
		}
		LONGJMP( env, 1 );
	} else {
			// Second time through - call the function
		SaveFunc();
	}
}
