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
