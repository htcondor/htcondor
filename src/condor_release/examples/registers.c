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


#include <stdio.h>

main( argc, argv )
int argc;
char **argv;
{
	foo( 0, 0, 0, 0, 0, 0, 0, 0 );
	exit( 0 );
}

foo( arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7 )
register int	arg0;
register int	arg1;
register int	arg2;
register int	arg3;
register int	arg4;
register int	arg5;
register int	arg6;
register int	arg7;
{
	register int	local0;
	register int	local1;
	register int	local2;
	register int	local3;
	register int	local4;
	register int	local5;
	register int	local6;
	register int	local7;

	if( arg0 > 34 ) {
		printf( "About to checkpoint\n" );
		fflush( stdout );
		ckpt();
		printf( "Returned from checkpoint\n" );
		printf( "args: %d %d %d %d %d %d %d %d\n",
				arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7 );
		fflush( stdout );
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

	printf( "args: %d %d %d %d %d %d %d %d\n",
			arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7 );
	printf( "locals: %d %d %d %d %d %d %d %d\n\n",
	      local0, local1, local2, local3, local4, local5, local6, local7 );
	fflush( stdout );
}

