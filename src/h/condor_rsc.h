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

#if defined(AIX31) || defined(AIX32)
#define AIX_KLUDGE wait_for_reply()
#else
#define AIX_KLUDGE
#endif

/*
**	GET_RVAL will get the returned value from the peer.
**	If the result is less than zero, it will retrieve
**	the error number and return, otherwise the rest
**	of the result parameters will be pulled off the wire.
*/
#define GET_RVAL(xdrs) \
	ASSERT(xdrrec_endofrecord((xdrs), TRUE)); \
	AIX_KLUDGE; /* This should NOT be needed. */ \
	(xdrs)->x_op = XDR_DECODE; \
	ASSERT(xdrrec_skiprecord((xdrs))); \
	ASSERT(xdr_int((xdrs), &rval)); \
	if( rval < 0 ) { \
		ASSERT(xdr_int((xdrs), &errno)); \
		goto RETURN; \
	}

#define PUT_RVAL(xdrs, rval) { \
	extern errno; \
	int result = (rval); \
	int terrno = errno; \
	(xdrs)->x_op = XDR_ENCODE; \
	ASSERT(xdr_int((xdrs), &result)); \
	if( result < 0 ) { \
		ASSERT(xdr_int((xdrs), &terrno)); \
		END_RVAL(xdrs, 0); \
	} \
}

#define END_RVAL(xdrs, rval) { \
	ASSERT(xdrrec_endofrecord((xdrs), TRUE)); \
	if( MemToFree != NULL ) { \
		free( MemToFree ); \
		MemToFree = NULL; \
	} \
	return( rval ); \
}
