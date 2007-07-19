
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
#include "condor_qmgr.h"

#if defined(assert)
#undef assert
#endif

#define assert(x) if (!(x)) { errno = ETIMEDOUT; return -1; }

static int CurrentSysCall;
extern ReliSock *qmgmt_sock;
int terrno;

int
InitializeConnection( const char *owner, const char * /* domain */ )
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
DestroyCluster( int cluster_id, const char * /*reason*/ )
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
SetAttributeByConstraint( char const *constraint, char const *attr_name, char const *attr_value )
{
	int	rval;

		CurrentSysCall = CONDOR_SetAttributeByConstraint;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->put(constraint) );
		assert( qmgmt_sock->put(attr_value) );
		assert( qmgmt_sock->put(attr_name) );
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
SetAttribute( int cluster_id, int proc_id, char const *attr_name, char const *attr_value, SetAttributeFlags_t flags )
{
	int	rval;

		CurrentSysCall = CONDOR_SetAttribute;
		if( flags ) {
				// Added in 6.9.4, so any use of flags!=0 (e.g. in
				// Condor-C) will break compatibility with earlier
				// versions.
			CurrentSysCall = CONDOR_SetAttribute2;
		}

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->put(attr_value) );
		assert( qmgmt_sock->put(attr_name) );
		if( flags ) {
			assert( qmgmt_sock->code(flags) );
		}
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
SetTimerAttribute( int cluster_id, int proc_id, char const *attr_name, int duration )
{
	int	rval;

		CurrentSysCall = CONDOR_SetTimerAttribute;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->put(attr_name) );
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
BeginTransaction_imp()
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

void
BeginTransaction()
{
	BeginTransaction_imp();
}

int
AbortTransaction_imp()
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

void
AbortTransaction()
{
	AbortTransaction_imp();
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
GetAttributeInt( int cluster_id, int proc_id, char const *attr_name, int *value )
{
	int	rval;

		CurrentSysCall = CONDOR_GetAttributeInt;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->put(attr_name) );
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
GetAttributeStringNew( int cluster_id, int proc_id, char const *attr_name, char **val )
{
	int	rval;

	CurrentSysCall = CONDOR_GetAttributeString;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );
	assert( qmgmt_sock->code(cluster_id) );
	assert( qmgmt_sock->code(proc_id) );
	assert( qmgmt_sock->put(attr_name) );
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
GetAttributeExpr( int cluster_id, int proc_id, char const *attr_name, char *value )
{
	int	rval;

		CurrentSysCall = CONDOR_GetAttributeExpr;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->put(attr_name) );
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
DeleteAttribute( int cluster_id, int proc_id, char const *attr_name )
{
	int	rval;

		CurrentSysCall = CONDOR_DeleteAttribute;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->code(cluster_id) );
		assert( qmgmt_sock->code(proc_id) );
		assert( qmgmt_sock->put(attr_name) );
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
GetJobAd( int cluster_id, int proc_id, bool /*expStartdAttrs*/ )
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
GetJobByConstraint( char const *constraint )
{
	int	rval;

		CurrentSysCall = CONDOR_GetJobByConstraint;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->put(constraint) );
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
GetNextJobByConstraint( char const *constraint, int initScan )
{
	int	rval;

	CurrentSysCall = CONDOR_GetNextJobByConstraint;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );
	assert( qmgmt_sock->code(initScan) );
	assert( qmgmt_sock->put(constraint) );
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

ClassAd *
GetAllJobsByConstraint_imp( char const *constraint, char const *projection, ClassAdList &list)
{
	int	rval;

		CurrentSysCall = CONDOR_GetAllJobsByConstraint;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->put(constraint) );
		assert( qmgmt_sock->put(projection) );
		assert( qmgmt_sock->end_of_message() );


		qmgmt_sock->decode();
		while (true) {
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
			list.Insert(ad);

		};
		assert( qmgmt_sock->end_of_message() );

	return 0;
}

void
GetAllJobsByConstraint( char const *constraint, char const *projection, ClassAdList &list)
{
	GetAllJobsByConstraint_imp(constraint,projection,list);
}

int
CloseSocket()
{
	CurrentSysCall = CONDOR_CloseSocket;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );
	assert( qmgmt_sock->end_of_message() );

	return 0;
}
