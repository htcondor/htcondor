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

#include "condor_common.h"
#include "condor_io.h"
#include "condor_classad.h"
#include "condor_fix_assert.h"
#include "qmgmt_constants.h"
#include "condor_qmgr.h"

// ETIMEDOUT is used to indicate one and all types of network errors.
// I am preserving this questionable tradition for now.
#define neg_on_error(x) if (!(x)) { errno = ETIMEDOUT; return -1; }
#define null_on_error(x) if (!(x)) { errno = ETIMEDOUT; return NULL; }

static int CurrentSysCall;
extern ReliSock *qmgmt_sock;
int terrno;

int
QmgmtSetAllowProtectedAttrChanges( int val )
{
	int	rval = -1;

	CurrentSysCall = CONDOR_SetAllowProtectedAttrChanges;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->code(val) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

int
QmgmtSetEffectiveOwner(char const *o)
{
	int rval = -1;

	CurrentSysCall = CONDOR_SetEffectiveOwner;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	if( !o ) {
		o = "";
	}
	neg_on_error( qmgmt_sock->put(o) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->end_of_message() );

	return 0;
}

int
NewCluster(CondorError* errstack)
{
	int	rval = -1;

		CurrentSysCall = CONDOR_NewCluster;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );

			// Older versions of the SCHEDD do not return an error reply ad
			// To handle that, look at what's on the wire, rather than what we should expect.
			ClassAd reply;
			bool gotClassad = false;
			if ( ! qmgmt_sock->peek_end_of_message()) {
				gotClassad = getClassAd(qmgmt_sock, reply);
			}
			if ( ! qmgmt_sock->end_of_message() && ! terrno) { // prefer existing errno
				terrno = ETIMEDOUT;
			}

			if (errstack) {
				std::string reason;
				const char * errmsg = nullptr;
				int errCode = terrno;
				if (gotClassad) {
					if (reply.LookupString("ErrorReason", reason)) {
						errmsg = reason.c_str();
						reply.LookupInteger("ErrorCode", errCode);
					}
				}
				errstack->push( "SCHEDD", errCode, errmsg );
			}

			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}
// old form of NewCluster with no errstack
int NewCluster() { return NewCluster(nullptr); }



int
NewProc( int cluster_id )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_NewProc;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->code(cluster_id) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}


int
DestroyProc( int cluster_id, int proc_id )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_DestroyProc;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->code(cluster_id) );
		neg_on_error( qmgmt_sock->code(proc_id) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}


int
DestroyCluster( int cluster_id, const char * /*reason*/ )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_DestroyCluster;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->code(cluster_id) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

#if 0
static int SetFactoryInfo(int req, int cluster_id, int num, const char * filename, const char * text)
{
	int	rval;

	CurrentSysCall = req;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->code(cluster_id) );
	neg_on_error( qmgmt_sock->code(num) );
	neg_on_error( qmgmt_sock->put(filename) );
	neg_on_error( qmgmt_sock->put(text) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->end_of_message() );
	return rval;
}
#endif

bool GetScheddCapabilites(int mask, ClassAd & ad)
{
	CurrentSysCall = CONDOR_GetCapabilities;

	qmgmt_sock->encode();
	if ( ! qmgmt_sock->code(CurrentSysCall) ||
		 ! qmgmt_sock->code(mask) ||
		 ! qmgmt_sock->end_of_message()) {
		return false;
	}
	qmgmt_sock->decode();
	if ( ! getClassAd(qmgmt_sock, ad) ) {
		return false;
	}
	return qmgmt_sock->end_of_message() != 0;
}

int SetJobFactory(int cluster_id, int num, const char * filename, const char * text)
{
	int	rval = -1;

	CurrentSysCall = CONDOR_SetJobFactory;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->code(cluster_id) );
	neg_on_error( qmgmt_sock->code(num) );
	neg_on_error( qmgmt_sock->put(filename) );
	neg_on_error( qmgmt_sock->put(text) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->end_of_message() );
	return rval;
}

int SendMaterializeData(int cluster_id, int flags, int (*next)(void* pv, std::string&item), void* pv, std::string & filename, int* pnum_items)
{
	int	rval = -1;
	int num_items = -1;
	filename.clear();
	if (pnum_items) *pnum_items = num_items;

	CurrentSysCall = CONDOR_SendMaterializeData;
	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->code(cluster_id) );
	neg_on_error( qmgmt_sock->code(flags) );

	// read the items and send them in 64k (ish) chunks
	const size_t cbAlloc = 0x10000;
	unsigned char buf[cbAlloc];
	int ix = 0;
	std::string item;
	while ((rval = next(pv, item)) == 1) {
		if (item.size() + ix > cbAlloc) {
			if (ix == 0) { errno = E2BIG; return -1; } // single item > 64k !!!
			neg_on_error ( qmgmt_sock->code_bytes(buf, ix) );
			ix = 0;
		}
		memcpy(buf+ix, item.data(), item.size());
		ix += item.size();
	}

	// if next failed, just bail now.
	if (rval < 0) { errno = EINVAL; return rval; }

	// put the remainder (if any)
	if (ix) { neg_on_error ( qmgmt_sock->code_bytes(buf, ix) ); }

	// put end of stream, then switch to decode mode to read the reply
	neg_on_error( qmgmt_sock->end_of_message() );
	qmgmt_sock->decode();

	// reply is the spooled filename and the item count
	neg_on_error( qmgmt_sock->code(filename) );
	neg_on_error( qmgmt_sock->code(num_items) );
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->end_of_message() );
	if (pnum_items) *pnum_items = num_items;
	return rval;

}

// Future : expose this?
static int SendJobQueueAd(int cluster_id, int ad_type, const classad::ClassAd & ad, unsigned int flags)
{
	int rval = -1;
	CurrentSysCall = CONDOR_SendJobQueueAd;
	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->code(cluster_id) );
	neg_on_error( qmgmt_sock->code(ad_type) );
	neg_on_error( qmgmt_sock->code(flags) );
	neg_on_error ( putClassAd(qmgmt_sock, ad) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->end_of_message() );
	return rval;
}

int SendJobsetAd(int cluster_id, const classad::ClassAd & ad, unsigned int flags) {
	return SendJobQueueAd(cluster_id, SENDJOBAD_TYPE_JOBSET, ad, flags);
}

int
SetAttributeByConstraint( char const *constraint, char const *attr_name, char const *attr_value, SetAttributeFlags_t flags_in )
{
	int	rval = -1;

	// only some of the flags can be sent on the wire, the upper bits are private to the schedd
	SetAttributePublicFlags_t flags = (flags_in & SetAttribute_PublicFlagsMask);


		CurrentSysCall = CONDOR_SetAttributeByConstraint;
		if( flags ) {
			CurrentSysCall = CONDOR_SetAttributeByConstraint2;
		}

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->put(constraint) );
		neg_on_error( qmgmt_sock->put(attr_value) );
		neg_on_error( qmgmt_sock->put(attr_name) );
		if( flags ) {
			neg_on_error( qmgmt_sock->code(flags) );
		}
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}


int
SetAttribute( int cluster_id, int proc_id, char const *attr_name, char const *attr_value, SetAttributeFlags_t flags_in, CondorError *)
{
	int	rval = 0;

	// only some of the flags can be sent on the wire, the upper bits are private to the schedd
	SetAttributePublicFlags_t flags = (flags_in & SetAttribute_PublicFlagsMask);

		CurrentSysCall = CONDOR_SetAttribute;
		if( flags ) {
				// Added in 6.9.4, so any use of flags!=0 (e.g. in
				// Condor-C) will break compatibility with earlier
				// versions.
			CurrentSysCall = CONDOR_SetAttribute2;
		}

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->code(cluster_id) );
		neg_on_error( qmgmt_sock->code(proc_id) );
		neg_on_error( qmgmt_sock->put(attr_value) );
		neg_on_error( qmgmt_sock->put(attr_name) );
		if( flags ) {
			neg_on_error( qmgmt_sock->code(flags) );
		}
		neg_on_error( qmgmt_sock->end_of_message() );

		if( flags & SetAttribute_NoAck ) {
			rval = 0;
		}
		else {
			qmgmt_sock->decode();
			neg_on_error( qmgmt_sock->code(rval) );
			if( rval < 0 ) {
				neg_on_error( qmgmt_sock->code(terrno) );
				neg_on_error( qmgmt_sock->end_of_message() );
				errno = terrno;
				return rval;
			}
			neg_on_error( qmgmt_sock->end_of_message() );
		}

	return rval;
}

int
SetTimerAttribute( int cluster_id, int proc_id, char const *attr_name, time_t duration )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_SetTimerAttribute;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->code(cluster_id) );
		neg_on_error( qmgmt_sock->code(proc_id) );
		neg_on_error( qmgmt_sock->put(attr_name) );
		neg_on_error( qmgmt_sock->code(duration) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

int
BeginTransaction_imp()
{
	int	rval = -1;

		CurrentSysCall = CONDOR_BeginTransaction;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

int
BeginTransaction()
{
	return BeginTransaction_imp();
}

int
AbortTransaction_imp()
{
	int	rval = -1;

		CurrentSysCall = CONDOR_AbortTransaction;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );

		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}

		neg_on_error( qmgmt_sock->end_of_message() );

		return rval;
}

int
AbortTransaction()
{
	return AbortTransaction_imp();
}

int
RemoteCommitTransaction(SetAttributeFlags_t flags_in, CondorError *errstack)
{
	int	rval = -1;

	// only some of the flags can be sent on the wire, the upper bits are private to the schedd
	SetAttributePublicFlags_t flags = (flags_in & SetAttribute_PublicFlagsMask);
	if( flags == 0 ) {
			// for compatibility with schedd's from before 7.5.0
		CurrentSysCall = CONDOR_CommitTransactionNoFlags;
	}
	else {
		CurrentSysCall = CONDOR_CommitTransaction;
	}

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	if( CurrentSysCall == CONDOR_CommitTransaction ) {
		neg_on_error( qmgmt_sock->put((int)flags) );
	}
	neg_on_error( qmgmt_sock->end_of_message() );


	ClassAd reply;
	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
	}

	// In some situations, we (the client) won't know the server's version,
	// but the server will know our version.  To handle that, we have to
	// look at what's on the wire, rather than what we should expect.
	// Luckily, the only thing after the terrno, if any, is a classad, so
	// this shouldn't cause future version incompabilities.
	bool gotClassAd = false;
	if(! qmgmt_sock->peek_end_of_message()) {
		neg_on_error( getClassAd( qmgmt_sock, reply ) );
		gotClassAd = true;
	}

	if( rval < 0 ) {
		if( gotClassAd ) {
			std::string errmsg;
			if( errstack && reply.LookupString( "ErrorReason", errmsg ) ) {
				int errCode = terrno;
				reply.LookupInteger( "ErrorCode", errCode );
				errstack->push( "SCHEDD", errCode, errmsg.c_str() );
			}
		}
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	} else if( gotClassAd ) {
		std::string warningReason;
		if( errstack && reply.LookupString( "WarningReason", warningReason ) ) {
			if(! warningReason.empty()) {
				errstack->push( "SCHEDD", 0, warningReason.c_str() );
			}
		}
	}
	neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

int
GetAttributeFloat( int cluster_id, int proc_id, char *attr_name, float *value )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_GetAttributeFloat;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->code(cluster_id) );
		neg_on_error( qmgmt_sock->code(proc_id) );
		neg_on_error( qmgmt_sock->code(attr_name) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->code(*value) );
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetAttributeInt( int cluster_id, int proc_id, char const *attr_name, long long *value )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_GetAttributeInt;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->code(cluster_id) );
		neg_on_error( qmgmt_sock->code(proc_id) );
		neg_on_error( qmgmt_sock->put(attr_name) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->code(*value) );
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

int
GetAttributeInt( int cluster_id, int proc_id, char const *attr_name, int *value )
{
	long long full_value = *value;
	int rval = GetAttributeInt(cluster_id, proc_id, attr_name, &full_value);
	*value = (int)full_value;
	return rval;
}

int
GetAttributeInt( int cluster_id, int proc_id, char const *attr_name, long *value )
{
	long long full_value = *value;
	int rval = GetAttributeInt(cluster_id, proc_id, attr_name, &full_value);
	*value = (long)full_value;
	return rval;
}

int
GetAttributeStringNew( int cluster_id, int proc_id, char const *attr_name, char **val )
{
	int	rval = -1;

	*val = nullptr;

	CurrentSysCall = CONDOR_GetAttributeString;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->code(cluster_id) );
	neg_on_error( qmgmt_sock->code(proc_id) );
	neg_on_error( qmgmt_sock->put(attr_name) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->code(*val) );
	neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetAttributeExprNew( int cluster_id, int proc_id, char const *attr_name, char **value )
{
	int	rval = -1;

	CurrentSysCall = CONDOR_GetAttributeExpr;

	*value = nullptr;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->code(cluster_id) );
	neg_on_error( qmgmt_sock->code(proc_id) );
	neg_on_error( qmgmt_sock->put(attr_name) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->code(*value) );
	neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}


int
GetDirtyAttributes(int cluster_id, int proc_id, ClassAd *updated_attrs)
{
	int	rval = -1;

	CurrentSysCall = CONDOR_GetDirtyAttributes;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->code(cluster_id) );
	neg_on_error( qmgmt_sock->code(proc_id) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}

	if ( !(getClassAd(qmgmt_sock, *updated_attrs)) ) {
		errno = ETIMEDOUT;
		return 0;
	}
	neg_on_error( qmgmt_sock->end_of_message() != 0 );

	return rval;
}


int
DeleteAttribute( int cluster_id, int proc_id, char const *attr_name )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_DeleteAttribute;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->code(cluster_id) );
		neg_on_error( qmgmt_sock->code(proc_id) );
		neg_on_error( qmgmt_sock->put(attr_name) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

int
SendSpoolFile( char const *filename )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_SendSpoolFile;

		qmgmt_sock->encode();
		neg_on_error( qmgmt_sock->code(CurrentSysCall) );
		neg_on_error( qmgmt_sock->put(filename) );
		neg_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		neg_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			neg_on_error( qmgmt_sock->code(terrno) );
			neg_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return rval;
		}
		neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

int
SendSpoolFileIfNeeded( ClassAd& ad )
{
	int	rval = -1;

	CurrentSysCall = CONDOR_SendSpoolFileIfNeeded;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( putClassAd(qmgmt_sock, ad) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return rval;
	}
	neg_on_error( qmgmt_sock->end_of_message() );

	return rval;
}

int
CloseSocket()
{
	CurrentSysCall = CONDOR_CloseSocket;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->end_of_message() );

	return 0;
}


ClassAd *
GetJobAd( int cluster_id, int proc_id, bool /*expStartdAttrs*/, bool /*persist_expansions*/ )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_GetJobAd;

		qmgmt_sock->encode();
		null_on_error( qmgmt_sock->code(CurrentSysCall) );
		null_on_error( qmgmt_sock->code(cluster_id) );
		null_on_error( qmgmt_sock->code(proc_id) );
		null_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		null_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			null_on_error( qmgmt_sock->code(terrno) );
			null_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return nullptr;
		}
		auto *ad = new ClassAd;

		if ( !(getClassAd(qmgmt_sock, *ad)) ) {
			delete ad;
			errno = ETIMEDOUT;
			return nullptr;
		}

		null_on_error( qmgmt_sock->end_of_message() );

	return ad;
}


ClassAd *
GetJobByConstraint( char const *constraint )
{
	int	rval = -1;

		CurrentSysCall = CONDOR_GetJobByConstraint;

		qmgmt_sock->encode();
		null_on_error( qmgmt_sock->code(CurrentSysCall) );
		null_on_error( qmgmt_sock->put(constraint) );
		null_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		null_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			null_on_error( qmgmt_sock->code(terrno) );
			null_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return nullptr;
		}
		auto *ad = new ClassAd;

		if ( !(getClassAd(qmgmt_sock, *ad)) ) {
			delete ad;
			errno = ETIMEDOUT;
			return nullptr;
		}

		null_on_error( qmgmt_sock->end_of_message() );

	return ad;
}


ClassAd *
GetNextJob( int initScan )
{
	int	rval = -1;;

		CurrentSysCall = CONDOR_GetNextJob;

		qmgmt_sock->encode();
		null_on_error( qmgmt_sock->code(CurrentSysCall) );
		null_on_error( qmgmt_sock->code(initScan) );
		null_on_error( qmgmt_sock->end_of_message() );

		qmgmt_sock->decode();
		null_on_error( qmgmt_sock->code(rval) );
		if( rval < 0 ) {
			null_on_error( qmgmt_sock->code(terrno) );
			null_on_error( qmgmt_sock->end_of_message() );
			errno = terrno;
			return nullptr;
		}
		
		auto *ad = new ClassAd;

		if ( !(getClassAd(qmgmt_sock, *ad)) ) {
			delete ad;
			errno = ETIMEDOUT;
			return nullptr;
		}

		null_on_error( qmgmt_sock->end_of_message() );

	return ad;
}


ClassAd *
GetNextJobByConstraint( char const *constraint, int initScan )
{
	int	rval = -1;

	CurrentSysCall = CONDOR_GetNextJobByConstraint;

	qmgmt_sock->encode();
	null_on_error( qmgmt_sock->code(CurrentSysCall) );
	null_on_error( qmgmt_sock->code(initScan) );
	null_on_error( qmgmt_sock->put(constraint) );
	null_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	null_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		null_on_error( qmgmt_sock->code(terrno) );
		null_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return nullptr;
	}

	auto *ad = new ClassAd;

	if ( ! (getClassAd(qmgmt_sock, *ad)) ) {
		delete ad;
		errno = ETIMEDOUT;
		return nullptr;
	}

	null_on_error( qmgmt_sock->end_of_message() );

	return ad;
}

ClassAd *
GetAllJobsByConstraint_imp( char const *constraint, char const *projection, ClassAdList &list)
{
	int	rval = -1;

		CurrentSysCall = CONDOR_GetAllJobsByConstraint;

		qmgmt_sock->encode();
		null_on_error( qmgmt_sock->code(CurrentSysCall) );
		null_on_error( qmgmt_sock->put(constraint) );
		null_on_error( qmgmt_sock->put(projection) );
		null_on_error( qmgmt_sock->end_of_message() );


		qmgmt_sock->decode();
		while (true) {
			null_on_error( qmgmt_sock->code(rval) );
			if( rval < 0 ) {
				null_on_error( qmgmt_sock->code(terrno) );
				null_on_error( qmgmt_sock->end_of_message() );
				errno = terrno;
				return nullptr;
			}

			auto *ad = new ClassAd;

			if ( ! (getClassAd(qmgmt_sock, *ad)) ) {
				delete ad;
				errno = ETIMEDOUT;
				return nullptr;
			}
			list.Insert(ad);

		};
		null_on_error( qmgmt_sock->end_of_message() );

	return nullptr;
}

void
GetAllJobsByConstraint( char const *constraint, char const *projection, ClassAdList &list)
{
	GetAllJobsByConstraint_imp(constraint,projection,list);
}

int
GetAllJobsByConstraint_Start( char const *constraint, char const *projection)
{
	CurrentSysCall = CONDOR_GetAllJobsByConstraint;

	qmgmt_sock->encode();
	neg_on_error( qmgmt_sock->code(CurrentSysCall) );
	neg_on_error( qmgmt_sock->put(constraint) );
	neg_on_error( qmgmt_sock->put(projection) );
	neg_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	return 0;
}

int
GetAllJobsByConstraint_Next( ClassAd &ad )
{
	int rval = -1;

	ASSERT( CurrentSysCall == CONDOR_GetAllJobsByConstraint );

	neg_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		neg_on_error( qmgmt_sock->code(terrno) );
		neg_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return -1;
	}

	if ( ! (getClassAd(qmgmt_sock, ad)) ) {
		errno = ETIMEDOUT;
		return -1;
	}

	return 0;
}

ClassAd *
GetNextDirtyJobByConstraint( char const *constraint, int initScan )
{
	int	rval = -1;

	CurrentSysCall = CONDOR_GetNextDirtyJobByConstraint;

	qmgmt_sock->encode();
	null_on_error( qmgmt_sock->code(CurrentSysCall) );
	null_on_error( qmgmt_sock->code(initScan) );
	null_on_error( qmgmt_sock->put(constraint) );
	null_on_error( qmgmt_sock->end_of_message() );

	qmgmt_sock->decode();
	null_on_error( qmgmt_sock->code(rval) );
	if( rval < 0 ) {
		null_on_error( qmgmt_sock->code(terrno) );
		null_on_error( qmgmt_sock->end_of_message() );
		errno = terrno;
		return nullptr;
	}

	auto *ad = new ClassAd;

	if ( ! (getClassAd(qmgmt_sock, *ad)) ) {
		delete ad;
		errno = ETIMEDOUT;
		return nullptr;
	}

	null_on_error( qmgmt_sock->end_of_message() );

	return ad;
}
