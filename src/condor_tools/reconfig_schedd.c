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

/*********************************************************************
* Tell the condor_schedd to re-read the configuration files and take
* appropriate actions.  This program is used by the Condor.throttle
* script to change the number of condor jobs a machine will "farm
* out" in parallel.
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
	char	hostname[512];

	config( argv[0], (CONTEXT *)0 );

	if( argc < 2 ) {
		gethostname( hostname, sizeof hostname );
		reconfig( hostname );
		exit( 0 );
	}

	for( argv++; *argv; argv++ ) {
		reconfig( *argv );
	}
}


reconfig( host )
char	*host;
{
	int			sock = -1;
	int			cmd = RECONFIG;
	XDR			xdr, *xdrs = NULL;

		/* Connect to the specified host */
	if( (sock = do_connect(host, "condor_schedd", SCHED_PORT)) < 0 ) {
		fprintf( stderr, "Can't connect to schedd on %s\n", host );
		return;
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	if( !xdr_int(xdrs, &cmd) ) {
		fprintf( stderr, "Can't send to schedd on %s\n", host );
		xdr_destroy( xdrs );
		close( sock );
		return;
	}

	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		fprintf( stderr, "Can't send to schedd on %s\n", host );
		xdr_destroy( xdrs );
		close( sock );
		return;
	}


	printf( "Sent RECONFIG command to schedd on %s\n", host );
	xdr_destroy( xdrs );
	close( sock );
}

SetSyscalls(){}
