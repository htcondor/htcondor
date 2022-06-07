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
#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_secman.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "authentication.h"

#include "qmgmt.h"
#include "condor_qmgr.h"
#include "qmgmt_constants.h"

#define syscall_sock qmgmt_sock

#if defined(assert)
#undef assert
#endif

#define assert(x) if (!(x)) return -1;

extern char *CondorCertDir;

extern int active_cluster_num;

static bool QmgmtMayAccessAttribute( char const *attr_name ) {
	return !ClassAdAttributeIsPrivateAny( attr_name );
}

	// When in NoAck mode for SetAttribute, we don't have any
	// opportunity to send an error message back to the client;
	// instead, we'll buffer errors here and send them back to
	// the client at attempted commit.
static std::unique_ptr<CondorError> g_transaction_error;

int
do_Q_request(QmgmtPeer &Q_PEER, bool &may_fork)
{
	int	request_num = -1;
	int	rval = -1;

	ReliSock *syscall_sock = Q_PEER.getReliSock();
	syscall_sock->decode();

	assert( syscall_sock->code(request_num) );

	dprintf(D_SYSCALLS, "Got request #%d\n", request_num);

	switch( request_num ) {

	case CONDOR_InitializeConnection:
	{
		// dprintf( D_ALWAYS, "InitializeConnection()\n" );
		bool authenticated = true;

		// Authenticate socket, if not already done by daemonCore
		if( !syscall_sock->triedAuthentication() ) {
			if( IsDebugLevel(D_SECURITY) ) {
				auto methods = SecMan::getAuthenticationMethods(WRITE);
				dprintf(D_SECURITY,"Calling authenticate(%s) in qmgmt_receivers\n", methods.c_str());
			}
			CondorError errstack;
			if( ! SecMan::authenticate_sock(syscall_sock, WRITE, &errstack) ) {
					// Failed to authenticate
				dprintf( D_ALWAYS, "SCHEDD: authentication failed: %s\n",
						 errstack.getFullText().c_str() );
				authenticated = false;
			}
		}

		if ( authenticated ) {
			InitializeConnection( syscall_sock->getOwner(),
					syscall_sock->getDomain() );
		} else {
			InitializeConnection( NULL, NULL );
		}
		return 0;
	}

	case CONDOR_InitializeReadOnlyConnection:
	{
		// dprintf( D_ALWAYS, "InitializeReadOnlyConnection()\n" );

		// We need to record if this is a read-only connection so that
		// we can avoid expanding $$ in GetJobAd; simply checking if the
		// connection is authenticated isn't sufficient, because the
		// security session cache means that read-only connection could
		// be authenticated by a previous authenticated connection from
		// the same address (when using host-based security) less than
		// the expiration period ago.
		Q_PEER.setReadOnly(true);

		// same as InitializeConnection but no authenticate()
		InitializeConnection( NULL, NULL );

		may_fork = true;
		return 0;
	}

	case CONDOR_SetAllowProtectedAttrChanges:
	{
		int val;
		int terrno;

		assert( syscall_sock->get(val) );
		assert( syscall_sock->end_of_message() );

		rval = QmgmtSetAllowProtectedAttrChanges( val );
		terrno = errno;

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );

		dprintf(D_SYSCALLS, "\tSetAllowProtectedAttrChanges(%d)\n", val);
		dprintf(D_SYSCALLS, "\trval %d, errno %d\n", rval, terrno);

		return 0;
	}

	case CONDOR_SetEffectiveOwner:
	{
		MyString owner;
		int terrno;

		assert( syscall_sock->get(owner) );
		assert( syscall_sock->end_of_message() );

		rval = QmgmtSetEffectiveOwner( owner.c_str() );
		terrno = errno;

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );

		char const *fqu = syscall_sock->getFullyQualifiedUser();
		dprintf(D_SYSCALLS, "\tSetEffectiveOwner\n");
		dprintf(D_SYSCALLS, "\tauthenticated user = '%s'\n", fqu ? fqu : "");
		dprintf(D_SYSCALLS, "\trequested owner = '%s'\n", owner.c_str());
		dprintf(D_SYSCALLS, "\trval %d, errno %d\n", rval, terrno);

		return 0;
	}

	case CONDOR_NewCluster:
	  {
		int terrno;

		if (!g_transaction_error) g_transaction_error.reset(new CondorError());
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = NewCluster( );
		terrno = errno;
		dprintf(D_SYSCALLS, 
				"\tNewCluster: rval = %d, errno = %d\n",rval,terrno );
		if ( rval > 0 ) {
			dprintf( D_AUDIT, *syscall_sock, 
					 "Submitting new job %d.0\n", rval );
		}

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;

		dprintf(D_FULLDEBUG,"schedd: NewCluster rval %d errno %d\n",rval,terrno);

		return 0;
	}

	case CONDOR_NewProc:
	  {
		int cluster_id = -1;
		int terrno;
		if (!g_transaction_error) g_transaction_error.reset(new CondorError());

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = NewProc( cluster_id );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		if ( rval > 0 ) {
			dprintf( D_AUDIT, *syscall_sock, 
					 "Submitting new job %d.%d\n", cluster_id, rval );
		}

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;

		dprintf(D_FULLDEBUG,"schedd: NewProc rval %d errno %d\n",rval,terrno);

		return 0;
	}

	case CONDOR_DestroyProc:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = DestroyProc( cluster_id, proc_id );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;

		dprintf(D_FULLDEBUG,"schedd: DestroyProc cluster %d proc %d rval %d errno %d\n",cluster_id,proc_id,rval,terrno);

		return 0;
	}

	case CONDOR_DestroyCluster:
	  {
		int cluster_id = -1;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = DestroyCluster( cluster_id );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

#if 0
	case CONDOR_DestroyClusterByConstraint:
	  {
		char *constraint=NULL;
		int terrno;

		assert( syscall_sock->code(constraint) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = DestroyClusterByConstraint( constraint );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		free( (char *)constraint );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}
#endif

	case CONDOR_SetAttributeByConstraint:
	case CONDOR_SetAttributeByConstraint2:
	  {
		std::string attr_name;
		char *attr_value=NULL;
		char *constraint=NULL;
		int terrno;
		SetAttributeFlags_t flags = 0;

		assert( syscall_sock->code(constraint) );
		dprintf( D_SYSCALLS, "  constraint = %s\n",constraint);
		assert( syscall_sock->code(attr_value) );
		assert( syscall_sock->code(attr_name) );
		if( request_num == CONDOR_SetAttributeByConstraint2 ) {
			SetAttributePublicFlags_t wflags = (SetAttributePublicFlags_t)flags;
			assert( syscall_sock->code( wflags ) );
			flags = (SetAttributeFlags_t)(wflags & SetAttribute_PublicFlagsMask);
		}
		assert( syscall_sock->end_of_message() );;

		if (strcmp (attr_name.c_str(), ATTR_MYPROXY_PASSWORD) == 0) {
			dprintf( D_SYSCALLS, "SetAttributeByConstraint (MyProxyPassword) not supported...\n");
			rval = 0;
			terrno = 0;
		} else {

			errno = 0;
			rval = SetAttributeByConstraint( constraint, attr_name.c_str(), attr_value, flags );
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
			if ( rval == 0 ) {
				dprintf( D_AUDIT, *syscall_sock,
						 "Set Attribute By Constraint %s, "
						 "%s = %s\n",
						 constraint, attr_name.c_str(), attr_value);
			}

		}

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		free( (char *)constraint );
		free( (char *)attr_value );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_SetAttribute:
	case CONDOR_SetAttribute2:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		std::string attr_name;
		char *attr_value=NULL;
		int terrno;
		SetAttributeFlags_t flags = 0;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_value) );
		assert( syscall_sock->code(attr_name) );
		if( request_num == CONDOR_SetAttribute2 ) {
			SetAttributePublicFlags_t wflags = (SetAttributePublicFlags_t)flags;
			assert( syscall_sock->code( wflags ) );
			flags = (SetAttributeFlags_t)(wflags & SetAttribute_PublicFlagsMask);
		}
		if (!attr_name.empty()) dprintf(D_SYSCALLS,"\tattr_name = %s\n",attr_name.c_str());
		assert( syscall_sock->end_of_message() );;
		if (attr_value) { 
			dprintf(D_SYSCALLS,"\tattr_value = %s\n",attr_value);		
		} else {
			// This shouldn't happen...
			dprintf(D_ALWAYS, "SetAttribute got NULL value for %s\n", attr_name.c_str());
			if( flags & SetAttribute_NoAck ) {
				return -1;
			}
			syscall_sock->encode();
			rval = -1;
			terrno = EINVAL; 
			assert( syscall_sock->code(rval) );
			assert( syscall_sock->code(terrno) );
			assert( syscall_sock->end_of_message() );
			return -1;
		}

			// We are unable to send responses in NoAck mode and will always fail
			// the transaction upon an attempted commit.  Hence, we ignore SetAttribute
			// calls of this type after the first error.
		if (g_transaction_error && !g_transaction_error->empty() &&
			(flags & SetAttribute_NoAck))
		{
			dprintf( D_SYSCALLS, "\tIgnored due to previous error\n");
			return 0;
		}

		// ckireyev:
		// We do NOT want to include MyProxy password in the ClassAd (since it's a secret)
		// I'm not sure if this is the best place to do this, but....
		if (!attr_name.empty() && strcmp (attr_name.c_str(), ATTR_MYPROXY_PASSWORD) == 0) {
			dprintf( D_SYSCALLS, "Got MyProxyPassword, stashing...\n");
			errno = 0;
			rval = SetMyProxyPassword (cluster_id, proc_id, attr_value);
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
			
		}
		else {
			errno = 0;

			rval = SetAttribute( cluster_id, proc_id, attr_name.c_str(), attr_value, flags, g_transaction_error.get() );
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
				// If we're modifying a previously-submitted job AND either
				// the client's username is not HTCondor's (i.e. not a
				// daemon) OR the client says we should log...
			if( ( IsDebugCategory( D_AUDIT ) ) &&
			    ( cluster_id != active_cluster_num ) &&
			    ( rval == 0 ) &&
			    ( ( strcmp(syscall_sock->getOwner(), get_condor_username()) &&
			        strcmp(syscall_sock->getFullyQualifiedUser(), CONDOR_CHILD_FQU) ) ||
			      ( flags & SHOULDLOG ) ) ) {

				dprintf( D_AUDIT, *syscall_sock, 
						 "Set Attribute for job %d.%d, "
						 "%s = %s\n",
						 cluster_id, proc_id, attr_name.c_str(), attr_value);
			}
		}

		free( (char *)attr_value );

		if( flags & SetAttribute_NoAck ) {
			if( rval < 0 ) {
				// Failures are deferred until we try to commit
				// Unlike SetAttribute with explicit Ack, this is error is going to be
				// fatal to the transaction.  Hence we'll short circuit any subsequent
				// SetAttribute_NoAck in this transaction.
				return 0;
			}
		}
		else {
			syscall_sock->encode();
			assert( syscall_sock->code(rval) );
			if( rval < 0 ) {
				assert( syscall_sock->code(terrno) );
			}
			assert( syscall_sock->end_of_message() );
		}
		return 0;
	}

	case CONDOR_SendJobQueueAd:
	{
		int cluster_id, ad_type;
		unsigned int flags;
		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(ad_type) );
		dprintf( D_SYSCALLS, "	ad_type = %d\n", ad_type );
		assert( syscall_sock->code(flags) );
		dprintf( D_SYSCALLS, "	flags = %d\n", flags );

		ClassAd ad;
		assert( getClassAd(qmgmt_sock, ad) );
		dprintf( D_SYSCALLS, "	ad = %p (%d attrs)\n", &ad, ad.size() );
		assert ( syscall_sock->end_of_message() );

		int terrno = 0;
		int rval = -1;
		if (ad_type == SENDJOBAD_TYPE_JOBSET) {
			rval = QmgmtHandleSendJobsetAd(cluster_id, ad, flags, terrno);
		} else {
			terrno = EINVAL;
			rval = -1;
		}

		syscall_sock->encode();
		if ( ! syscall_sock->code(rval) || ((rval < 0) && !syscall_sock->code(terrno))) {
			return -1;
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	} break;

	case CONDOR_SetJobFactory:
	case CONDOR_SetMaterializeData:
	{
		int cluster_id, num;
		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(num) );
		dprintf( D_SYSCALLS, "	num = %d\n", num );
		char * filename = NULL;
		assert( syscall_sock->code(filename) );
		dprintf( D_SYSCALLS, "	factory_filename = %s\n", filename ? filename : "NULL" );
		char * text = NULL;
		assert( syscall_sock->code(text) );
		if (text) { dprintf( D_SYSCALLS, "	factory_text = %s\n", text ); }
		assert( syscall_sock->end_of_message() );

			// this is only valid during submission of a cluster
		int terrno = 0;
		if (cluster_id != active_cluster_num) {
			rval = -1;
			terrno = EPERM;
		} else if (request_num == CONDOR_SetMaterializeData) {
			// obsolete, don't do anything with this RPC
			rval = -1;
			terrno = EPERM;
		} else {
			errno = 0;
			const SetAttributeFlags_t flags = 0;
			rval = SetAttributeInt( cluster_id, -1, ATTR_JOB_MATERIALIZE_LIMIT, num, flags );
			if (rval >= 0) {
				rval = QmgmtHandleSetJobFactory(cluster_id, filename, text);
				if (rval == 0) {
					//PRAGMA_REMIND("FUTURE: allow first proc id to be > 0 ?")
					const int first_proc_id = 0;
					rval = SetAttributeInt(cluster_id, -1, ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, first_proc_id, flags);
				}
			}
			terrno = errno;
		}

		if (filename) { free(filename); filename = NULL; } 
		if (text)     { free(text); text = NULL; }

		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		// send a status reply
		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );
		return 0;
	} break;

	case CONDOR_SendMaterializeData:
	{
		int cluster_id, flags, row_count = 0;
		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(flags) );
		dprintf( D_SYSCALLS, "	flags = 0x%x\n", flags );

		int terrno = 0;
		std::string filename; // set to the filename that the data was spooled to.
		if (cluster_id != active_cluster_num) {
			return -1;
		}

		rval = QmgmtHandleSendMaterializeData(cluster_id, syscall_sock, filename, row_count, terrno);
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d, filename = %s\n", rval, terrno, filename.empty() ? "NULL" : filename.c_str() );
		if (rval < 0) {
			errno = terrno;
			return rval;
		}
		assert( syscall_sock->end_of_message() );

		// send a status reply and also the name of the file we wrote the itemdata to
		syscall_sock->encode();
		assert( syscall_sock->code(filename) );
		assert( syscall_sock->code(row_count) );
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );
		return 0;
	} break;

	case CONDOR_GetCapabilities: {
		int mask;
		assert( syscall_sock->code(mask) );
		assert( syscall_sock->end_of_message() );

		errno = 0;
		ClassAd reply;
		GetSchedulerCapabilities(mask, reply);
		syscall_sock->encode();
		assert( putClassAd( syscall_sock, reply ) );
		assert( syscall_sock->end_of_message() );;
		return 0;
	} break;

	case CONDOR_SetTimerAttribute:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		std::string attr_name;
		int duration = 0;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		if (!attr_name.empty()) dprintf(D_SYSCALLS,"\tattr_name = %s\n",attr_name.c_str());
		assert( syscall_sock->code(duration) );
		dprintf(D_SYSCALLS,"\tduration = %d\n",duration);
		assert( syscall_sock->end_of_message() );;

		errno = 0;

		rval = SetTimerAttribute( cluster_id, proc_id, attr_name.c_str(), duration );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		dprintf( D_AUDIT, *syscall_sock, 
				 "Set Timer Attribute for job %d.%d, "
				 "attr_name = %s, duration = %d\n",
				 cluster_id, proc_id, attr_name.c_str(), duration);

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_BeginTransaction:
	  {
		int terrno;
		g_transaction_error.reset(new CondorError());

		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = 0;	// BeginTransaction returns void (sigh), so always success
		BeginTransaction( );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_AbortTransaction:
	{
		int terrno;
		g_transaction_error.reset();

		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = 0;	// AbortTransaction returns void (sigh), so always success

		AbortTransaction( );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );


		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}

		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_CommitTransactionNoFlags:
	case CONDOR_CommitTransaction:
	  {
		int terrno;
		int flags;

		if( request_num == CONDOR_CommitTransaction ) {
			assert( syscall_sock->code(flags) );
		}
		else {
			flags = 0;
		}
		assert( syscall_sock->end_of_message() );

		std::unique_ptr<CondorError> errstack;
		if (g_transaction_error && !g_transaction_error->empty()) {
			errstack = std::move(g_transaction_error);
			AbortTransaction();
			terrno = errstack->code();
			if (terrno == 0) terrno = -1;
			else if (terrno > 0) terrno = -terrno;
		} else {
			errstack.reset(new CondorError());
			errno = 0;
			rval = CommitTransactionAndLive( flags, errstack.get() );
			terrno = errno;
		}
		dprintf( D_SYSCALLS, "\tflags = %d, rval = %d, errno = %d\n", flags, rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		const CondorVersionInfo *vers = syscall_sock->get_peer_version();
		bool send_classad = vers && vers->built_since_version(8, 3, 4);
		bool always_send_classad = vers && vers->built_since_version(8, 7, 4);
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval < 0 && send_classad ) {
			// Send a classad, for less backwards-incompatibility.
			int code = 1;
			const char * reason = "QMGMT rejected job submission.";
			if(! errstack->empty()) {
				code = 2;
				reason = errstack->message();
			}

			ClassAd reply;
			reply.Assign( "ErrorCode", code );
			reply.Assign( "ErrorReason", reason );
			assert( putClassAd( syscall_sock, reply ) );
		} else if( always_send_classad ) {
			ClassAd reply;

			std::string reason;
			if(! errstack->empty()) {
				reason = errstack->getFullText();
				reply.Assign( "WarningReason", reason );
			}

			assert( putClassAd( syscall_sock, reply ) );
		}

		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetAttributeFloat:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		std::string attr_name;
		float value = 0.0;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		dprintf(D_SYSCALLS,"\tattr_name = %s\n",attr_name.c_str());
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		if( QmgmtMayAccessAttribute( attr_name.c_str())) {
			rval = GetAttributeFloat( cluster_id, proc_id, attr_name.c_str(), &value );
		}
		else {
			errno = EACCES;
			rval = -1;
		}
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( syscall_sock->code(value) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetAttributeInt:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		std::string attr_name;
		int value = 0;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		dprintf( D_SYSCALLS, "\tattr_name = %s\n", attr_name.c_str() );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		if( QmgmtMayAccessAttribute( attr_name.c_str())) {
			rval = GetAttributeInt( cluster_id, proc_id, attr_name.c_str(), &value );
		}
		else {
			errno = EACCES;
			rval = -1;
		}
		terrno = errno;
		if (rval < 0) {
			dprintf( D_SYSCALLS, "GetAttributeInt(%d, %d, %s) not found.\n",
					cluster_id, proc_id, attr_name.c_str());
		} else {
			dprintf( D_SYSCALLS, "  value: %d\n", value );
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		}

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( syscall_sock->code(value) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetAttributeString:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		std::string attr_name;
		std::string value;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name));
		dprintf(D_SYSCALLS, "\tattr_name = %s\n", attr_name.c_str());
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		if( QmgmtMayAccessAttribute( attr_name.c_str())) {
			rval = GetAttributeString( cluster_id, proc_id, attr_name.c_str(), value );
		}
		else {
			errno = EACCES;
			rval = -1;
		}

		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( syscall_sock->code(value));
		}
		assert( syscall_sock->end_of_message() );
		return 0;
	}

	case CONDOR_GetAttributeExpr:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		std::string attr_name;

		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		dprintf(D_SYSCALLS,"\tattr_name = %s\n",attr_name.c_str());
		assert( syscall_sock->end_of_message() );;

		char *value = NULL;

		errno = 0;
		if( QmgmtMayAccessAttribute( attr_name.c_str())) {
			rval = GetAttributeExprNew( cluster_id, proc_id, attr_name.c_str(), &value );
		}
		else {
			errno = EACCES;
			rval = -1;
		}
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();

		if ( !syscall_sock->code(rval) ) {
			free(value);
			return -1;
		}
		if( rval < 0 ) {
			if ( !syscall_sock->code(terrno) ) {
					free(value);
					return -1;
			}
		}
		if( rval >= 0 ) {
			if ( !syscall_sock->code(value) ) {
					free(value);
					return -1;
			}
		}
		free( (char *)value );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetDirtyAttributes:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		ClassAd updates;

		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = GetDirtyAttributes( cluster_id, proc_id, &updates );

		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();

		if ( !syscall_sock->code(rval) ) {
			return -1;
		}
		if( rval < 0 ) {
			if ( !syscall_sock->code(terrno) ) {
					return -1;
			}
		}
		if( rval >= 0 ) {
			assert( putClassAd(syscall_sock, updates) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_DeleteAttribute:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		std::string attr_name;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		dprintf(D_SYSCALLS,"\tattr_name = %s\n",attr_name.c_str());
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = DeleteAttribute( cluster_id, proc_id, attr_name.c_str());
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetJobAd:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		ClassAd *ad = NULL;
		int terrno;
		bool delete_ad = false;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->end_of_message() );;

		// dprintf( D_ALWAYS, "(%d.%d) isAuthenticated() = %d\n", cluster_id, proc_id, syscall_sock->isAuthenticated() );
		// dprintf( D_ALWAYS, "(%d.%d) getOwner() = %s\n", cluster_id, proc_id, syscall_sock->getOwner() );

		errno = 0;
		// Only fetch the jobad for legal values of cluster/proc
		if( cluster_id >= 1 ) {
			if( proc_id >= 0 ) {
				if( !Q_PEER.getReadOnly() ) {
					// expand $$() macros in the jobad as required by GridManager.
					// The GridManager depends on the fact that the following call
					// expands $$ and saves the expansions to disk in case of
					// restart.
					ad = GetJobAd_as_ClassAd( cluster_id, proc_id, true, true );
					delete_ad = true;
					// note : since we expanded the ad, ad is now a deep
					// copy of the ad in memory, so we must delete it below.
				} else {
					ad = GetJobAd_as_ClassAd( cluster_id, proc_id, false, false );
				}
			} else if( proc_id == -1 ) {
				// allow cluster ad to be queried as required by preen, but
				// do NOT ask to expand $$() macros in a cluster ad!
				ad = GetJobAd_as_ClassAd( cluster_id, proc_id, false, false );
			}
		}
		terrno = errno;
		rval = ad ? 0 : -1;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( putClassAd(syscall_sock, *ad, PUT_CLASSAD_NO_PRIVATE) );
		}
		// If we called GetJobAd() with the third bool argument set
		// to True (expandedAd), it does a deep copy of the ad in the
		// queue in order to expand the $$() attributes.  So we must
		// delete it.
		if (delete_ad) delete ad;
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetJobByConstraint:
	  {
		char *constraint=NULL;
		ClassAd *ad;
		int terrno;

		assert( syscall_sock->code(constraint) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		ad = GetJobByConstraint_as_ClassAd( constraint );
		terrno = errno;
		rval = ad ? 0 : -1;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( putClassAd(syscall_sock, *ad, PUT_CLASSAD_NO_PRIVATE) );
		}
		FreeJobAd(ad);
		free( (char *)constraint );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetNextJob:
	  {
		ClassAd *ad;
		int initScan = 0;
		int terrno;

		assert( syscall_sock->code(initScan) );
		dprintf( D_SYSCALLS, "	initScan = %d\n", initScan );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		ad = GetNextJob( initScan );
		terrno = errno;
		rval = ad ? 0 : -1;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( putClassAd(syscall_sock, *ad, PUT_CLASSAD_NO_PRIVATE) );
		}
		FreeJobAd(ad);
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetNextJobByConstraint:
	  {
		char *constraint=NULL;
		ClassAd *ad;
		int initScan = 0;
		int terrno;

		assert( syscall_sock->code(initScan) );
		dprintf( D_SYSCALLS, "	initScan = %d\n", initScan );
		if ( !(syscall_sock->code(constraint)) ) {
			if (constraint != NULL) {
				free(constraint);
				constraint = NULL;
			}
			return -1;
		}
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		ad = GetNextJobByConstraint( constraint, initScan );
		terrno = errno;
		rval = ad ? 0 : -1;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( putClassAd(syscall_sock, *ad, PUT_CLASSAD_NO_PRIVATE) );
		}
		FreeJobAd(ad);
		free( (char *)constraint );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}
	case CONDOR_GetNextDirtyJobByConstraint:
	{
		char *constraint=NULL;
		ClassAd *ad;
		int initScan = 0;
		int terrno;

		assert( syscall_sock->code(initScan) );
		dprintf( D_SYSCALLS, "  initScan = %d\n", initScan );
		if ( !(syscall_sock->code(constraint)) ) {
			if (constraint != NULL) {
				free(constraint);
				constraint = NULL;
			}
			return -1;
		}
		assert( syscall_sock->end_of_message() );

		errno = 0;
		ad = GetNextDirtyJobByConstraint( constraint, initScan );
		terrno = errno;
		rval = ad ? 0 : -1;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( putClassAd(syscall_sock, *ad, PUT_CLASSAD_NO_PRIVATE) );
		}
		FreeJobAd(ad);
		free( (char *)constraint );
		assert( syscall_sock->end_of_message() );
		return 0;
	}

	case CONDOR_SendSpoolFile:
	  {
		char *filename=NULL;
		int terrno;

		assert( syscall_sock->code(filename) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = SendSpoolFile( filename );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
#if 0
		// SendSpoolFile() sends the reply before receiving the file.
		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;
#endif
		free( (char *)filename );
		return 0;
	}

	case CONDOR_SendSpoolFileIfNeeded:
	  {
		int terrno;

		ClassAd ad;
		assert( getClassAd(syscall_sock, ad) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = SendSpoolFileIfNeeded(ad);
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		return 0;
	}

	case CONDOR_GetAllJobsByConstraint:
	  {
		char *constraint=NULL;
		char *projection=NULL;
		ClassAd *ad;
		int terrno;
		int initScan = 1;
		classad::References proj;

		if ( !(syscall_sock->code(constraint)) ) {
			if (constraint != NULL) {
				free(constraint);
				constraint = NULL;
			}
			return -1;
		}
		if ( !(syscall_sock->code(projection)) ) {
			if (projection != NULL) {
				free(constraint);
				free(projection);
				projection = NULL;
			}
			return -1;
		}
		dprintf( D_SYSCALLS, "	constraint = %s\n", constraint );
		dprintf( D_SYSCALLS, "	projection = %s\n", projection ? projection : "");

		assert( syscall_sock->end_of_message() );;

		// if there is a projection, convert it into a set of attribute names
		if (projection) {
			StringTokenIterator list(projection);
			const std::string * attr;
			while ((attr = list.next_string())) { proj.insert(*attr); }
		}

		syscall_sock->encode();

		do {
			errno = 0;

			ad = GetNextJobByConstraint( constraint, initScan );
			initScan=0; // one first time through, otherwise 0

			terrno = errno;
			rval = ad ? 0 : -1;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

			assert( syscall_sock->code(rval) );

			if( rval < 0 ) {
				assert( syscall_sock->code(terrno) );
			}

			// Condor-C relies on the ServerTimer attribute
			if( rval >= 0 ) {
				assert( putClassAd(syscall_sock, *ad, PUT_CLASSAD_NO_PRIVATE | PUT_CLASSAD_SERVER_TIME, proj.empty() ? NULL : &proj) );
				FreeJobAd(ad);
			}
		} while (rval >= 0);
		assert( syscall_sock->end_of_message() );;

		free( (char *)constraint );
		free( (char *)projection );
		return 0; 
	}

	case CONDOR_CloseSocket:
	{
		assert( syscall_sock->end_of_message() );;
		return -1;
	}

	} /* End of switch */

	return -1;
} /* End of function */
