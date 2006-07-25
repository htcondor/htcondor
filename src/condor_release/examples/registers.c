/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

