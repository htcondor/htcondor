/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

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

