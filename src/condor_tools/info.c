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
** Author:   Dhaval N. Shah
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*********************************************************************
* Send condor_vacate to startd's on non-instructional machines at the 
* time of reboot of instructional machines.
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

main(argc, argv)
int		argc;
char	*argv[]; 
{
	int			sock = -1;
	int			cmd,i;
	int count=0;
	RUNNING_JOB_INFO job_info;
	XDR			xdr, *xdrs = NULL;

	if( argc != 2 ) {
		usage( argv[0] );
	}

	/* config( argv[0], (CONTEXT *)0 ); */

		/* Connect to the specified host */
	if( (sock = do_connect(argv[1], "condor_schedd", SCHED_PORT)) < 0 ) {
		dprintf( D_ALWAYS, "Can't connect to schedd on %s\n", argv[1] );
		exit( 1 );
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	cmd = SEND_RUNNING_JOBS;
	ASSERT( xdr_int(xdrs, &cmd) );
	ASSERT( xdrrec_endofrecord(xdrs,TRUE) );

	printf( "Sent SEND_RUNNING_JOBS command to schedd on %s\n", argv[1] );
	xdrs->x_op=XDR_DECODE;
	if(!xdr_int(xdrs,&count))
	{
		perror("xdr_int:");
		close(sock);
		exit(1);
	}
	printf("count is %d\n",count);
	memset(&job_info, 0, sizeof(RUNNING_JOB_INFO));
    for(i=0;i<count;++i)
	{
	printf("REceiving %d jobs %d\n",count,sizeof(RUNNING_JOB_INFO));
	if(xdr_running_job_info(xdrs,&job_info))
	{
		printf("job_info %d.%d\t %s \t %s\n",job_info.clusterID,job_info.procID,job_info.host,ctime(&job_info.startTime));
	}
	else
	{
		perror("Error in xdr_running_job_info");
		close(sock);
		exit(1);
	}
	}
	xdr_destroy(xdrs);
	free(job_info.host);
    close(sock);
	exit( 0 );
}

SetSyscalls(){}
