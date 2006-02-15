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
#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_io.h"
#include "condor_classad.h"
#include "condor_fix_assert.h"
#include "qmgmt_constants.h"

#if defined(assert)
#undef assert
#endif

#define assert(x) if (!(x)) { errno = ETIMEDOUT; return -1; }

static int CurrentSysCall;
extern ReliSock *qmgmt_sock;
int terrno;

#if defined(__cplusplus)
extern "C" {
#endif

int
InitializeConnection( const char *owner )
{
	CurrentSysCall = CONDOR_InitializeConnection;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );

	qmgmt_sock->setOwner( owner );
	return( 0 );
}

int
InitializeReadOnlyConnection( const char *owner )
{
	CurrentSysCall = CONDOR_InitializeReadOnlyConnection;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );

	qmgmt_sock->setOwner( owner );
	return( 0 );
}

int
NewCluster()
{
	int	rval;

		CurrentSysCall = CONDOR_NewCluster;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
NewProc( int cluster_id )
{
	int	rval;

		CurrentSysCall = CONDOR_NewProc;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
DestroyProc( int cluster_id, int proc_id )
{
	int	rval;

		CurrentSysCall = CONDOR_DestroyProc;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
DestroyCluster( int cluster_id )
{
	int	rval;

		CurrentSysCall = CONDOR_DestroyCluster;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


#if 0
int
DestroyClusterByConstraint( char *constraint )
{
	int	rval;

		CurrentSysCall = CONDOR_DestroyClusterByConstraint;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(constraint) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}
#endif


int
SetAttributeByConstraint( char *constraint, char *attr_name, char *attr_value )
{
	int	rval;

		CurrentSysCall = CONDOR_SetAttributeByConstraint;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(constraint) );
		assert( qmgmt_sock->code(attr_value) );
		assert( qmgmt_sock->code(attr_name) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
SetAttribute( int cluster_id, int proc_id, char *attr_name, char *attr_value )
{
	int	rval;

		CurrentSysCall = CONDOR_SetAttribute;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->code(attr_value) );
		assert( qmgmt_sock->code(attr_name) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}

int
SetTimerAttribute( int cluster_id, int proc_id, char *attr_name, int duration )
{
	int	rval;

		CurrentSysCall = CONDOR_SetTimerAttribute;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->code(attr_name) );
		assert( qmgmt_sock->code(duration) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}

int
BeginTransaction()
{
	int	rval;

		CurrentSysCall = CONDOR_BeginTransaction;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}

int
AbortTransaction()
{
	int	rval;

		CurrentSysCall = CONDOR_AbortTransaction;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );

		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}

		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
CloseConnection()
{
	int	rval;

		CurrentSysCall = CONDOR_CloseConnection;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetAttributeFloat( int cluster_id, int proc_id, char *attr_name, float *value )
{
	int	rval;

		CurrentSysCall = CONDOR_GetAttributeFloat;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->code(attr_name) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->code(value) );
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetAttributeInt( int cluster_id, int proc_id, char *attr_name, int *value )
{
	int	rval;

		CurrentSysCall = CONDOR_GetAttributeInt;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->code(attr_name) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->code(value) );
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetAttributeString( int cluster_id, int proc_id, char *attr_name, char *value )
{
	int	rval;

		CurrentSysCall = CONDOR_GetAttributeString;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->code(attr_name) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->code(value) );
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetAttributeStringNew( int cluster_id, int proc_id, char *attr_name, char **val )
{
	int	rval;

	CurrentSysCall = CONDOR_GetAttributeString;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );
	assert( qmgmt_sock->code(cluster_id) );
	assert( qmgmt_sock->code(proc_id) );
	assert( qmgmt_sock->code(attr_name) );
	assert( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	assert( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		*val = (char *) calloc(1, sizeof(char));
		assert( qmgmt_sock->code(terrno) );
		assert( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	*val = NULL;
	assert( qmgmt_sock->code(*val) );
	assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetAttributeExpr( int cluster_id, int proc_id, char *attr_name, char *value )
{
	int	rval;

		CurrentSysCall = CONDOR_GetAttributeExpr;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->code(attr_name) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->code(value) );
		assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
DeleteAttribute( int cluster_id, int proc_id, char *attr_name )
{
	int	rval;

		CurrentSysCall = CONDOR_DeleteAttribute;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->code(attr_name) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}

int
SendSpoolFile( char *filename )
{
	int	rval;

		CurrentSysCall = CONDOR_SendSpoolFile;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(filename) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		assert( qmgmt_sock->end_of_message() );

	return rval;
}

#if defined(assert)
#undef assert
#endif

#define assert(x) if (!(x)) { errno = ETIMEDOUT; return NULL; }

ClassAd *
GetJobAd( int cluster_id, int proc_id )
{
	int	rval;

		CurrentSysCall = CONDOR_GetJobAd;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return NULL;
		}
		ClassAd *ad = new ClassAd;

		if ( !(ad->initFromStream(*qmgmt_sock)) ) {
			delete ad;
			errno = ETIMEDOUT;
			return NULL;
		}

		assert( qmgmt_sock->end_of_message() );

	return ad;
}


ClassAd *
GetJobByConstraint( char *constraint )
{
	int	rval;

		CurrentSysCall = CONDOR_GetJobByConstraint;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(constraint) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return NULL;
		}
		ClassAd *ad = new ClassAd;

		if ( !(ad->initFromStream(*qmgmt_sock)) ) {
			delete ad;
			errno = ETIMEDOUT;
			return NULL;
		}

		assert( qmgmt_sock->end_of_message() );

	return ad;
}


ClassAd *
GetNextJob( int initScan )
{
	int	rval;

		CurrentSysCall = CONDOR_GetNextJob;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(initScan) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return NULL;
		}
		
		ClassAd *ad = new ClassAd;

		if ( !(ad->initFromStream(*qmgmt_sock)) ) {
			delete ad;
			errno = ETIMEDOUT;
			return NULL;
		}

		assert( qmgmt_sock->end_of_message() );

	return ad;
}


ClassAd *
GetNextJobByConstraint( char *constraint, int initScan )
{
	int	rval;

		CurrentSysCall = CONDOR_GetNextJobByConstraint;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(initScan) );
		assert( qmgmt_sock->code(constraint) );
		assert( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		assert( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			assert( qmgmt_sock->code(terrno) );
			assert( qmgmt_sock->end_of_message() );
			errno = terrno;
			return NULL;
		}

		ClassAd *ad = new ClassAd;

		if ( ! (ad->initFromStream(*qmgmt_sock)) ) {
			delete ad;
			errno = ETIMEDOUT;
			return NULL;
		}

		assert( qmgmt_sock->end_of_message() );

	return ad;
}

#if defined(__cplusplus)
}
#endif
