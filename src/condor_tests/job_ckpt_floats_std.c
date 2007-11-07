/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


/* I didn't write this program, but it took me a while to understand exactly 
   what it is doing, so I thought I would write some comments.  The program
   basically does a lot of tail recursion.  It starts with 8 0s, and recursivley
   calls foo() 34 times.  Each call incrememnts adds one to the arguments, 
   switches the sign, and calls foo again.  This continues until the 35th call
   which checkpoints and restarts the job.  

   After the restart, each function falls back in the recursion chain and
   prints out it's floating point register state.  This way, we can test
   whether condor correctly checkpoints floating point registers, and
   the call stack.  

   The expected output is a list of args and locals.  Each pair represents
   one of the recursive calls to foo().  The args and locals fall towards
   zero with the end of every function.  We start at 35 and count down
   with the args.  We start with -35 and cound up with the locals.  

   --Tom ( June 1, 1998 )
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "x_fake_ckpt.h"

void foo( register float arg0, register float arg1, register float arg2, register float arg3, register float arg4, register float arg5, register float arg6, register float arg7 );

int
main( int argc, char **argv )
{


	foo( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 );
	exit( 0 );
}

void foo( register float arg0, register float arg1, register float arg2, register float arg3, register float arg4, register float arg5, register float arg6, register float arg7 )
{
	register float	local0;
	register float	local1;
	register float	local2;
	register float	local3;
	register float	local4;
	register float	local5;
	register float	local6;
	register float	local7;

	if( arg0 > 34 ) {
		printf( "About to checkpoint\n" );
		fflush( stdout );
		ckpt_and_exit();
		printf( "Returned from checkpoint\n" );
		printf( "args: %f %f %f %f %f %f %f %f\n",
				arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7 );
		return;
	}
	local0 = -(arg0 +1);
	local1 = -(arg1 +1);
	local2 = -(arg2 +1);
	local3 = -(arg3 +1);
	local4 = -(arg4 +1);
	local5 = -(arg5 +1);
	local6 = -(arg6 +1);
	local7 = -(arg7 +1);
	foo( -local0, -local1, -local2, -local3,
		 -local4, -local5, -local6, -local7 );

	printf( "args: %f %f %f %f %f %f %f %f\n",
			arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7 );

	printf( "locals: %f %f %f %f %f %f %f %f\n\n",
	      local0, local1, local2, local3, local4, local5, local6, local7 );
}
