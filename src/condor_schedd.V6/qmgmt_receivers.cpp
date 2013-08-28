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

#include "../condor_syscall_lib/syscall_param_sizes.h"

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
	return !ClassAdAttributeIsPrivate( attr_name );
}

int
do_Q_request(ReliSock *syscall_sock,bool &may_fork)
{
	int	request_num = -1;
	int	rval;

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
				MyString methods;
				SecMan::getAuthenticationMethods( WRITE, &methods );
				dprintf(D_SECURITY,"Calling authenticate(%s) in qmgmt_receivers\n", methods.Value());
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

		// Since InitializeConnection() does nothing, and we need
		// to record the fact that this is a read-only connection,
		// but we have to do it in the socket (since we don't have
		// any other persistent data structure, and it's probably
		// the right place anyway), set the FQU.
		//
		// We need to record if this is a read-only connection so that
		// we can avoid expanding $$ in GetJobAd; simply checking if the
		// connection is authenticated isn't sufficient, because the
		// security session cache means that read-only connection could
		// be authenticated by a previous authenticated connection from
		// the same address (when using host-based security) less than
		// the expiration period ago.
		syscall_sock->setFullyQualifiedUser( "read-only" );

		// same as InitializeConnection but no authenticate()
		InitializeConnection( NULL, NULL );

		may_fork = true;
		return 0;
	}

	case CONDOR_SetEffectiveOwner:
	{
		MyString owner;
		int terrno;

		assert( syscall_sock->get(owner) );
		assert( syscall_sock->end_of_message() );

		rval = QmgmtSetEffectiveOwner( owner.Value() );
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
		dprintf(D_SYSCALLS, "\trequested owner = '%s'\n", owner.Value());
		dprintf(D_SYSCALLS, "\trval %d, errno %d\n", rval, terrno);

		return 0;
	}

	case CONDOR_NewCluster:
	  {
		int terrno;

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
		char *attr_name=NULL;
		char *attr_value=NULL;
		char *constraint=NULL;
		int terrno;
		SetAttributeFlags_t flags = 0;

		assert( syscall_sock->code(constraint) );
		dprintf( D_SYSCALLS, "  constraint = %s\n",constraint);
		assert( syscall_sock->code(attr_value) );
		assert( syscall_sock->code(attr_name) );
		if( request_num == CONDOR_SetAttributeByConstraint2 ) {
			assert( syscall_sock->code( flags ) );
		}
		assert( syscall_sock->end_of_message() );;

		if (strcmp (attr_name, ATTR_MYPROXY_PASSWORD) == 0) {
			errno = 0;
			dprintf( D_SYSCALLS, "SetAttributeByConstraint (MyProxyPassword) not supported...\n");
			rval = 0;
			terrno = errno;
		} else {

			errno = 0;
			rval = SetAttributeByConstraint( constraint, attr_name, attr_value, flags );
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
			if ( rval == 0 ) {
				dprintf( D_AUDIT, *syscall_sock,
						 "Set Attribute By Constraint %s, "
						 "%s = %s\n",
						 constraint, attr_name, attr_value);
			}

		}

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		free( (char *)constraint );
		free( (char *)attr_value );
		free( (char *)attr_name );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_SetAttribute:
	case CONDOR_SetAttribute2:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		char *attr_name=NULL;
		char *attr_value=NULL;
		int terrno;
		SetAttributeFlags_t flags = 0;
		const char *users_username;
		const char *condor_username;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_value) );
		assert( syscall_sock->code(attr_name) );
		if( request_num == CONDOR_SetAttribute2 ) {
			assert( syscall_sock->code( flags ) );
		}
		users_username = syscall_sock->getOwner();
		condor_username = get_condor_username();
		if (attr_name) dprintf(D_SYSCALLS,"\tattr_name = %s\n",attr_name);
		if (attr_value) dprintf(D_SYSCALLS,"\tattr_value = %s\n",attr_value);		
		assert( syscall_sock->end_of_message() );;

		// ckireyev:
		// We do NOT want to include MyProxy password in the ClassAd (since it's a secret)
		// I'm not sure if this is the best place to do this, but....
		if (attr_name && attr_value && strcmp (attr_name, ATTR_MYPROXY_PASSWORD) == 0) {
			errno = 0;
			dprintf( D_SYSCALLS, "Got MyProxyPassword, stashing...\n");
			rval = SetMyProxyPassword (cluster_id, proc_id, attr_value);
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
			
		}
		else {
			errno = 0;

			rval = SetAttribute( cluster_id, proc_id, attr_name, attr_value, flags );
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
				// If we're modifying a previously-submitted job AND either
				// the client's username is not HTCondor's (i.e. not a
				// daemon) OR the client says we should log...
			if( (cluster_id != active_cluster_num) && (rval == 0) &&
				( strcmp(users_username, condor_username) || (flags & SHOULDLOG) ) ) { 

				dprintf( D_AUDIT, *syscall_sock, 
						 "Set Attribute for job %d.%d, "
						 "%s = %s\n",
						 cluster_id, proc_id, attr_name, attr_value);
			}
		}

		free( (char *)attr_value );
		free( (char *)attr_name );

		if( flags & SetAttribute_NoAck ) {
			if( rval < 0 ) {
				return -1;
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

	case CONDOR_SetTimerAttribute:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		char *attr_name=NULL;
		int duration = 0;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		if (attr_name) dprintf(D_SYSCALLS,"\tattr_name = %s\n",attr_name);
		assert( syscall_sock->code(duration) );
		dprintf(D_SYSCALLS,"\tduration = %d\n",duration);
		assert( syscall_sock->end_of_message() );;

		errno = 0;

		rval = SetTimerAttribute( cluster_id, proc_id, attr_name, duration );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		dprintf( D_AUDIT, *syscall_sock, 
				 "Set Timer Attribute for job %d.%d, "
				 "attr_name = %s, duration = %d\n",
				 cluster_id, proc_id, attr_name, duration);

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		free( (char *)attr_name );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_BeginTransaction:
	  {
		int terrno;

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
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		CommitTransaction( flags );
			// CommitTransaction() never returns on failure
		rval = 0;
		terrno = errno;
		dprintf( D_SYSCALLS, "\tflags = %d, rval = %d, errno = %d\n", flags, rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetAttributeFloat:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		char *attr_name=NULL;
		float value = 0.0;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		if( QmgmtMayAccessAttribute( attr_name ) ) {
			rval = GetAttributeFloat( cluster_id, proc_id, attr_name, &value );
		}
		else {
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
		free( (char *)attr_name );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetAttributeInt:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		char *attr_name=NULL;
		int value = 0;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		dprintf( D_SYSCALLS, "  attr_name = %s\n", attr_name );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		if( QmgmtMayAccessAttribute( attr_name ) ) {
			rval = GetAttributeInt( cluster_id, proc_id, attr_name, &value );
		}
		else {
			rval = -1;
		}
		terrno = errno;
		if (rval < 0) {
			dprintf( D_SYSCALLS, "GetAttributeInt(%d, %d, %s) not found.\n",
					cluster_id, proc_id, attr_name);
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
		free( (char *)attr_name );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetAttributeString:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		char *attr_name=NULL;
		char *value = NULL;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		if( QmgmtMayAccessAttribute( attr_name ) ) {
			rval = GetAttributeStringNew( cluster_id, proc_id, attr_name, &value );
		}
		else {
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
		free( (char *)value );
		free( (char *)attr_name );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetAttributeExpr:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		char *attr_name=NULL;

		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		assert( syscall_sock->end_of_message() );;

		char *value = NULL;

		errno = 0;
		if( QmgmtMayAccessAttribute( attr_name ) ) {
			rval = GetAttributeExprNew( cluster_id, proc_id, attr_name, &value );
		}
		else {
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
		free( (char *)attr_name );
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
		char *attr_name=NULL;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = DeleteAttribute( cluster_id, proc_id, attr_name );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		free( (char *)attr_name );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetJobAd:
	  {
		int cluster_id = -1;
		int proc_id = -1;
		ClassAd *ad = NULL;
		int terrno;

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
				const char * fqu = syscall_sock->getFullyQualifiedUser();
				if( fqu != NULL && strcmp( fqu, "read-only" ) != 0 ) {
					// expand $$() macros in the jobad as required by GridManager.
					// The GridManager depends on the fact that the following call
					// expands $$ and saves the expansions to disk in case of
					// restart.
					ad = GetJobAd( cluster_id, proc_id, true, true );
					// note : since we expanded the ad, ad is now a deep
					// copy of the ad in memory, so we must delete it below.
				} else {
					// Since we don't expand macros here, we need to make a
					// deep copy for the code below (at least according to the
					// original comment), despite appearances.
					ClassAd *cluster_ad = GetJobAd( cluster_id, proc_id, false, false );
					if ( cluster_ad ) {
						ad = new ClassAd(*cluster_ad);
					}
				}
			} else if( proc_id == -1 ) {
				// allow cluster ad to be queried as required by preen, but
				// do NOT ask to expand $$() macros in a cluster ad!
				ClassAd *cluster_ad = GetJobAd( cluster_id, proc_id, false, false );
				// since we did not expand, ad is not a deep copy.
				// thus we deep copy it now, since the below code assumes
				// "ad" is a deep copy and therefore below we can set
				// private attrs, delete it, etc, without messing up
				// the schedd's canonical copy.
				if ( cluster_ad ) {
					ad = new ClassAd(*cluster_ad);
				}
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
			assert( putClassAd(syscall_sock, *ad, true) );
		}
		// Here we must really, truely delete the ad.  Why? Because
		// when GetJobAd is called with the third bool argument set
		// to True (expandedAd), it does a deep copy of the ad in the
		// queue in order to expand the $$() attributes.  So we must
		// always delete it.
		if (ad) delete ad;	// need to really delete it cuz expanded
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
		ad = GetJobByConstraint( constraint );
		terrno = errno;
		rval = ad ? 0 : -1;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( putClassAd(syscall_sock, *ad, true) );
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
			assert( putClassAd(syscall_sock, *ad, true) );
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
			assert( putClassAd(syscall_sock, *ad, true) );
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
			assert( putClassAd(syscall_sock, *ad, true) );
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

		StringList sl(projection, "\n");

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

			if( rval >= 0 ) {
				if (sl.number() != 0) {
					sl.rewind();
					StringList internals;
					StringList externals; // shouldn't have any
					while (char *attr = sl.next()) {

						if( !ad->GetExprReferences(attr, internals, externals) ) {
							dprintf(D_FULLDEBUG,
									"GetAllJobsByConstraint failed to parse "
									"requested ClassAd expression: %s\n",attr);
						}
					}

					assert( putClassAd(syscall_sock, *ad, true, &internals) );
				} else {
					assert( putClassAd(syscall_sock, *ad, true) );
				}
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
