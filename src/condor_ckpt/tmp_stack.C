/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

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
