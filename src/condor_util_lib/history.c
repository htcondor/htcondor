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
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


/**************************************************************************
**
** Maintain a history of condor jobs.  Every job gets its PROC struct recorded
** here after termination.
**
** Operations:
** 
**	XDR *OpenHistory( file, xdrs, fd )
**		char	*file;
**		XDR		*xdrs;
**		int		*fd;
** 
** 	CloseHistory( H )
** 		XDR		*H;
** 
** 	AppendHistory( H, proc )
** 		XDR		*H;
** 		PROC	*proc;
** 
** 	ScanHistory( H, func )
** 		XDR		*H;
** 		int		(*func)();
**
**	LockHistory( H, operation )
**		XDR		*H;
**		int		operation;
**
******************************************************************************/

#include <sys/file.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "except.h"
#include "proc.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

XDR *
OpenHistory( file, xdrs, fd )
char	*file;
XDR		*xdrs;
int		*fd;
{
	XDR		*xdr_Init();
	
	if( (*fd=open(file,O_RDWR,0)) < 0 ) {
		EXCEPT( "open(%s,O_RDWR,0)", file );
	}
	return xdr_Init( fd, xdrs );
}

CloseHistory( H )
XDR		*H;
{
	int		*fd = ((int **)H->x_private)[0];

	/*
	** Some NFS implementations (e.g. IRIX), might not remove the
	** lock when the file is closed.  Do it here explicitly just
	** to make *sure*.
	*/
	(void)LockHistory( H, LOCK_UN );

	if( close(*fd) < 0 ) {
		EXCEPT( "close(%d)", *fd );
	}
	xdr_destroy( H );
}

AppendHistory( H, p )
XDR		*H;
PROC	*p;
{
	int		*fd = ((int **)H->x_private)[0];

	if( lseek(*fd,0,L_XTND) < 0 ) {
		EXCEPT( "lseek(%d,0,L_XTND)", *fd );
	}

	H->x_op = XDR_ENCODE;
	if( !xdr_proc(H,p) ) {
		EXCEPT( "xdr_proc" );
	}

	if( !xdrrec_endofrecord(H,TRUE) ) {
		EXCEPT( "xdrrec_endofrecord" );
	}
}

ScanHistory( H, func )
XDR		*H;
int		(*func)();
{
	PROC	p;
	int		*fd = ((int **)H->x_private)[0];

	if( lseek(*fd,0,L_XTND) < 0 ) {
		EXCEPT( "lseek(%d,0,L_EXTND)", *fd );
	}
	xdrrec_skiprecord( H );
	if( lseek(*fd,0,L_SET) < 0 ) {
		EXCEPT( "lseek(%d,0,L_SET)", *fd );
	}
	H->x_op = XDR_DECODE;

	for(;;) {
		bzero( (char *)&p, sizeof(PROC) );
		if( xdr_proc(H,&p) ) {
			xdrrec_skiprecord( H );
			func( &p );
		} else {
			return;
		}
	}
}

LockHistory( H, op )
XDR		*H;
int		op;
{
	int		*fd = ((int **)H->x_private)[0];

	return flock(*fd,op);
}
