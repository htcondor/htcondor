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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_commands.h"
#include "daemon.h"
#include "dc_startd.h"



DCStartd::DCStartd( const char* name, const char* pool ) 
	: Daemon( DT_STARTD, name, pool )
{
	capability = NULL;
}


DCStartd::~DCStartd( void )
{
	if( capability ) {
		free( capability );
	}
}


bool
DCStartd::setCapability( const char* cap_str ) 
{
	if( ! cap_str ) {
		return false;
	}
	if( capability ) {
		free( capability );
	}
	capability = strdup( cap_str );
	return true;
}


bool 
DCStartd::deactivateClaim( bool graceful )
{
	dprintf( D_FULLDEBUG, "Entering DCStartd::deactivateClaim(%s)\n",
			 graceful ? "graceful" : "forceful" );

	if( ! capability ) {
		dprintf( D_ALWAYS, "DCStartd::deactivateClaim "
				 "called with NULL capability, failing\n" );
		return false;
	}
	if( ! _addr ) {
		locate();
	}
	if( ! _addr ) {
		dprintf( D_ALWAYS, "DCStartd::deactivateClaim: "
				 "Can't locate startd: %s\n", _error ? _error : 
				 "unknown error" );
		return false;
	}

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCStartd::deactivateClaim: "
				 "Failed to connect to startd (%s)\n", _addr );
		return false;
	}
	int cmd;
	if( graceful ) {
		cmd = DEACTIVATE_CLAIM;
	} else {
		cmd = DEACTIVATE_CLAIM_FORCIBLY;
	}
	result = startCommand( cmd, (Sock*)&reli_sock ); 
	if( ! result ) {
		dprintf( D_ALWAYS, "DCStartd::deactivateClaim: "
				 "Failed to send command (%s) to the startd\n", 
				 graceful ? "DEACTIVATE_CLAIM" :
				 "DEACTIVATE_CLAIM_FORCIBLY" );
		return false;
	}
		// Now, send the capability
	if( ! reli_sock.code(capability) ) {
		dprintf( D_ALWAYS, "DCStartd::deactivateClaim: "
				 "Failed to send capability to the startd\n" );
		return false;
	}
	if( ! reli_sock.eom() ) {
		dprintf( D_ALWAYS, "DCStartd::deactivateClaim: "
				 "Failed to send EOM to the startd\n" );
		return false;
	}
		// we're done
	dprintf( D_FULLDEBUG, "DCStartd::deactivateClaim: "
			 "successfully sent command\n" );
	return true;
}


int
DCStartd::activateClaim( ClassAd* job_ad, int starter_version,
						 ReliSock** claim_sock_ptr ) 
{
	int reply;
	dprintf( D_FULLDEBUG, "Entering DCStartd::activateClaim()\n" );

	if( claim_sock_ptr ) {
			// our caller wants a pointer to the socket we used to
			// successfully activate the claim.  right now, set it to
			// NULL to signify error, and if everything works out,
			// we'll give them a pointer to the real object.
		*claim_sock_ptr = NULL;
	}

	if( ! capability ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim "
				 "called with NULL capability, failing\n" );
		return NOT_OK;
	}
	if( ! _addr ) {
		locate();
	}
	if( ! _addr ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim: "
				 "Can't locate startd: %s\n", _error ? _error : 
				 "unknown error" );
		return NOT_OK;
	}

	Sock* tmp;
	tmp = startCommand( ACTIVATE_CLAIM, Stream::reli_sock, 20 ); 
	if( ! tmp ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim: "
				 "Failed to send command (%s) to the startd\n", 
				 "ACTIVATE_CLAIM" );
		return NOT_OK;
	}
	if( ! tmp->code(capability) ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim: "
				 "Failed to send capability to the startd\n" );
		delete tmp;
		return NOT_OK;
	}
	if( ! tmp->code(starter_version) ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim: "
				 "Failed to send starter_version to the startd\n" );
		delete tmp;
		return NOT_OK;
	}
	if( ! job_ad->put(*tmp) ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim: "
				 "Failed to send job_ad to the startd\n" );
		delete tmp;
		return NOT_OK;
	}
	if( ! tmp->end_of_message() ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim: "
				 "Failed to send EOM to the startd\n" );
		delete tmp;
		return NOT_OK;
	}

		// Now, try to get the reply
	tmp->decode();
	if( !tmp->code(reply) || !tmp->end_of_message()) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim: "
				 "failed to receive reply from %s\n", _addr );
		delete tmp;
		return NOT_OK;
	}

	dprintf( D_FULLDEBUG, "DCStartd::activateClaim: "
			 "successfully sent command, reply is: %d\n", reply ); 

	if( reply == OK && claim_sock_ptr ) {
		*claim_sock_ptr = (ReliSock*)tmp;
	} else {
			// in any other case, we're going to leak this ReliSock
			// object if we don't delete it here...
		delete tmp;
	}
	return reply;
}
