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
** Authors:  Allan Bricker and Michael J. Litzkow, Todd Tannenbaum,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*********************************************************************
* Ask a machine which has been hosting a condor job to perform a 
* periodic checkpoint of the job.
*********************************************************************/


#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "debug.h"
#include "except.h"
#include "trace.h"
#include "sched.h"
#include "expr.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

XDR		*xdr_Init();

usage( str )
char	*str;
{
	fprintf( stderr, "Usage: %s hostname\n", str );
	exit( 1 );
}

main( argc, argv )
int		argc;
char	*argv[];
{
	int			sock = -1;
	int			cmd;
	XDR			xdr, *xdrs = NULL;

	if( argc != 2 ) {
		usage( argv[0] );
	}

	config( argv[0], (CONTEXT *)0 );

		/* Connect to the specified host */
	if( (sock = do_connect(argv[1], "condor_startd", START_PORT)) < 0 ) {
		dprintf( D_ALWAYS, "Can't connect to startd on %s\n", argv[1] );
		exit( 1 );
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	cmd = PCKPT_FRGN_JOB;
	ASSERT( xdr_int(xdrs, &cmd) );
	ASSERT( xdrrec_endofrecord(xdrs,TRUE) );

	printf( "Sent PCKPT_FRGN_JOB command to startd on %s\n", argv[1] );

	exit( 0 );
}

SetSyscalls(){}
