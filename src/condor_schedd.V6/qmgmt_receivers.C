/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_secman.h"
#include "condor_attributes.h"

#include "../condor_syscall_lib/syscall_param_sizes.h"

#include "condor_qmgr.h"
#include "qmgmt_constants.h"

#define syscall_sock qmgmt_sock

#if defined(assert)
#undef assert
#endif

#define assert(x) if (!(x)) return -1;

extern char *CondorCertDir;

int
do_Q_request(ReliSock *syscall_sock)
{
	int	request_num;
	int	rval;

	syscall_sock->decode();

	assert( syscall_sock->code(request_num) );

	dprintf(D_SYSCALLS, "Got request #%d\n", request_num);

	switch( request_num ) {

	case CONDOR_InitializeConnection:
	{
		bool authenticated = true;
	
			// Authenticate socket, if not already done by daemonCore
		if( !syscall_sock->isAuthenticated() ) {
			char * p = SecMan::getSecSetting( "SEC_%s_AUTHENTICATION_METHODS", "WRITE" );
			MyString methods;
			if (p) {
				methods = p;
				free (p);
			} else {
				methods = SecMan::getDefaultAuthenticationMethods();
			}
			dprintf(D_SECURITY,"Calling authenticate(%s) in qmgmt_receivers\n", methods.Value());
			CondorError errstack;
			if( ! syscall_sock->authenticate(methods.Value(), &errstack) ) {
					// Failed to authenticate
				dprintf( D_ALWAYS, "SCHEDD: authentication failed: %s\n", 
						 errstack.getFullText() );
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
			// same as InitializeConnection but no authenticate()
		InitializeConnection( NULL, NULL );			
		return 0;
	}

	case CONDOR_NewCluster:
	  {
		int terrno;

		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = NewCluster( );
		terrno = errno;
		dprintf(D_SYSCALLS,"\tNewCluster: trval = %d, errno = %d\n",rval,terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;

		dprintf(D_FAILURE,"schedd: NewCluster rval %d errno %d\n",rval,terrno);

		return 0;
	}

	case CONDOR_NewProc:
	  {
		int cluster_id;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = NewProc( cluster_id );
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );;

		dprintf(D_FAILURE,"schedd: NewProc rval %d errno %d\n",rval,terrno);

		return 0;
	}

	case CONDOR_DestroyProc:
	  {
		int cluster_id;
		int proc_id;
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

		dprintf(D_FAILURE,"schedd: DestroyProc cluster %d proc %d rval %d errno %d\n",cluster_id,proc_id,rval,terrno);

		return 0;
	}

	case CONDOR_DestroyCluster:
	  {
		int cluster_id;
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
	  {
		char *attr_name=NULL;
		char *attr_value=NULL;
		char *constraint=NULL;
		int terrno;

		assert( syscall_sock->code(constraint) );
		dprintf( D_SYSCALLS, "  constraint = %s\n",constraint);
		assert( syscall_sock->code(attr_value) );
		assert( syscall_sock->code(attr_name) );
		assert( syscall_sock->end_of_message() );;

		if (strcmp (attr_name, ATTR_MYPROXY_PASSWORD) == 0) {
			errno = 0;
			dprintf( D_SYSCALLS, "SetAttributeByConstraint (MyProxyPassword) not supported...\n");
			rval = 0;
			terrno = errno;
		} else {

			errno = 0;
			rval = SetAttributeByConstraint( constraint, attr_name, attr_value );
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
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
	  {
		int cluster_id;
		int proc_id;
		char *attr_name=NULL;
		char *attr_value=NULL;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_value) );
		assert( syscall_sock->code(attr_name) );
		if (attr_name) dprintf(D_SYSCALLS,"\tattr_name = %s\n",attr_name);
		if (attr_value) dprintf(D_SYSCALLS,"\tattr_value = %s\n",attr_value);
		assert( syscall_sock->end_of_message() );;

		// ckireyev:
		// We do NOT want to include MyProxy password in the ClassAd (since it's a secret)
		// I'm not sure if this is the best place to do this, but....
		if (strcmp (attr_name, ATTR_MYPROXY_PASSWORD) == 0) {
			errno = 0;
			dprintf( D_SYSCALLS, "Got MyProxyPassword, stashing...\n");
			rval = SetMyProxyPassword (cluster_id, proc_id, attr_value);
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
			
		}
		else {
			errno = 0;

			rval = SetAttribute( cluster_id, proc_id, attr_name, attr_value );
			terrno = errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		}

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		free( (char *)attr_value );
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


	case CONDOR_CloseConnection:
	  {
		int terrno;

		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = CloseConnection( );
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

	case CONDOR_GetAttributeFloat:
	  {
		int cluster_id;
		int proc_id;
		char *attr_name=NULL;
		float value;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = GetAttributeFloat( cluster_id, proc_id, attr_name, &value );
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
		int cluster_id;
		int proc_id;
		char *attr_name=NULL;
		int value;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		dprintf( D_SYSCALLS, "  attr_name = %s\n", attr_name );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = GetAttributeInt( cluster_id, proc_id, attr_name, &value );
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
		int cluster_id;
		int proc_id;
		char *attr_name=NULL;
		// XXX: shouldn't need a fixed size here -- at least keep
		// it off the stack to avoid overflow attacks
		char *value = (char *)malloc(ATTRLIST_MAX_EXPRESSION*sizeof(char));
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = GetAttributeString( cluster_id, proc_id, attr_name, value );
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
		int cluster_id;
		int proc_id;
		char *attr_name=NULL;
		// XXX: shouldn't need a fixed size here -- at least keep
		// it off the stack to avoid overflow attacks
		char *value = (char *)malloc(ATTRLIST_MAX_EXPRESSION*sizeof(char));
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->code(attr_name) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = GetAttributeExpr( cluster_id, proc_id, attr_name, value );
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

	case CONDOR_DeleteAttribute:
	  {
		int cluster_id;
		int proc_id;
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
		int cluster_id;
		int proc_id;
		ClassAd *ad;
		int terrno;

		assert( syscall_sock->code(cluster_id) );
		dprintf( D_SYSCALLS, "	cluster_id = %d\n", cluster_id );
		assert( syscall_sock->code(proc_id) );
		dprintf( D_SYSCALLS, "	proc_id = %d\n", proc_id );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		ad = GetJobAd( cluster_id, proc_id, true );
		terrno = errno;
		rval = ad ? 0 : -1;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( ad->put(*syscall_sock) );
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
			assert( ad->put(*syscall_sock) );
		}
		FreeJobAd(ad);
		free( (char *)constraint );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetNextJob:
	  {
		ClassAd *ad;
		int initScan;
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
			assert( ad->put(*syscall_sock) );
		}
		FreeJobAd(ad);
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_GetNextJobByConstraint:
	  {
		char *constraint=NULL;
		ClassAd *ad;
		int initScan;
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
			assert( ad->put(*syscall_sock) );
		}
		FreeJobAd(ad);
		free( (char *)constraint );
		assert( syscall_sock->end_of_message() );;
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

	} /* End of switch */

	return -1;
} /* End of function */
