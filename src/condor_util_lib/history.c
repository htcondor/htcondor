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
/* Solaris specific change 6/23 ..dhaval */
#if !defined(Solaris)
#include <sys/resource.h>
#endif

/* Solaris specific change */
#if defined(Solaris)
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#if !defined(Solaris251)
#include </usr/ucbinclude/sys/rusage.h>
#endif
#endif

#include "except.h"
#include "proc.h"

#if defined(HPUX9) || defined(Solaris)
#	include "fake_flock.h"
#endif

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
		memset( (char *)&p,0, sizeof(PROC) );/* ..dhaval 9/11/95 */
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
