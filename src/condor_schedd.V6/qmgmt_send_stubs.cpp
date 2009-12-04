
/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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

	return( 0 );
}

int
InitializeReadOnlyConnection( const char *owner )
{
	CurrentSysCall = CONDOR_InitializeReadOnlyConnection;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );

	return( 0 );
}

int
QmgmtSetEffectiveOwner(char const *o)
{
	int rval = -1;

	CurrentSysCall = CONDOR_SetEffectiveOwner;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );
	if( !o ) {
		o = "";
	}
	assert( qmgmt_sock->put(o) );
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

	return 0;
}

int
NewCluster()
{
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;

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
GetAttributeStringNew( int cluster_id, int proc_id, char const *attr_name, char **val )
{
	int	rval = -1;

	*val = NULL;

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
		assert( qmgmt_sock->code(terrno) );
		assert( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	assert( qmgmt_sock->code(*val) );
	assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetAttributeExprNew( int cluster_id, int proc_id, char const *attr_name, char **value )
{
	int	rval = -1;

	CurrentSysCall = CONDOR_GetAttributeExpr;

	*value = NULL;

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
	assert( qmgmt_sock->code(*value) );
	assert( qmgmt_sock->end_of_message() );

	return rval;
}


int
DeleteAttribute( int cluster_id, int proc_id, char const *attr_name )
{
	int	rval = -1;

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
SendSpoolFile( char const *filename )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_SendSpoolFile;

		qmgmt_sock->encode();
		assert( qmgmt_sock->code(CurrentSysCall) );
		assert( qmgmt_sock->put(filename) );
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
SendSpoolFileIfNeeded( ClassAd& ad )
{
	int	rval = -1;

	CurrentSysCall = CONDOR_SendSpoolFileIfNeeded;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );
	assert( ad.put(*qmgmt_sock) );
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
CloseSocket()
{
	CurrentSysCall = CONDOR_CloseSocket;

	qmgmt_sock->encode();
	assert( qmgmt_sock->code(CurrentSysCall) );
	assert( qmgmt_sock->end_of_message() );

	return 0;
}

#if defined(assert)
#undef assert
#endif

#define assert(x) if (!(x)) { errno = ETIMEDOUT; return NULL; }

ClassAd *
GetJobAd( int cluster_id, int proc_id, bool /*expStartdAttrs*/, bool /*persist_expansions*/ )
{
	int	rval = -1;

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
	int	rval = -1;

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
	int	rval = -1;;

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
	int	rval = -1;

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
	int	rval = -1;

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
