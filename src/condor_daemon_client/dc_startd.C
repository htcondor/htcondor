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
#include "condor_string.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "command_strings.h"
#include "daemon.h"
#include "dc_startd.h"



DCStartd::DCStartd( const char* name, const char* pool ) 
	: Daemon( DT_STARTD, name, pool )
{
	claim_id = NULL;
}


DCStartd::~DCStartd( void )
{
	if( claim_id ) {
		delete [] claim_id;
	}
}


bool
DCStartd::setCapability( const char* cap_str ) 
{
	return setClaimId( cap_str );
}


bool
DCStartd::setClaimId( const char* id ) 
{
	if( ! id ) {
		return false;
	}
	if( claim_id ) {
		delete [] claim_id;
		claim_id = NULL;
	}
	claim_id = strnewp( id );
	return true;
}


bool 
DCStartd::deactivateClaim( bool graceful )
{
	dprintf( D_FULLDEBUG, "Entering DCStartd::deactivateClaim(%s)\n",
			 graceful ? "graceful" : "forceful" );

	setCmdStr( "deactivateClaim" );
	if( ! checkClaimId() ) {
		return false;
	}
	if( ! checkAddr() ) {
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
		// Now, send the claim_id
	if( ! reli_sock.code(claim_id) ) {
		dprintf( D_ALWAYS, "DCStartd::deactivateClaim: "
				 "Failed to send claim_id to the startd\n" );
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

	setCmdStr( "activateClaim" );

	if( claim_sock_ptr ) {
			// our caller wants a pointer to the socket we used to
			// successfully activate the claim.  right now, set it to
			// NULL to signify error, and if everything works out,
			// we'll give them a pointer to the real object.
		*claim_sock_ptr = NULL;
	}

	if( ! claim_id ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim "
				 "called with NULL claim_id, failing\n" );
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
	if( ! tmp->code(claim_id) ) {
		dprintf( D_ALWAYS, "DCStartd::activateClaim: "
				 "Failed to send claim_id to the startd\n" );
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


bool
DCStartd::requestClaim( ClaimType type, const ClassAd* req_ad, 
						ClassAd* reply, int timeout )
{
	setCmdStr( "requestClaim" );

	MyString err_msg;
	switch( type ) {
	case CLAIM_COD:
	case CLAIM_OPPORTUNISTIC:
		break;
	default:
		err_msg = "Invalid ClaimType (";
		err_msg += (int)type;
		err_msg += ')';
		newError( err_msg.Value() );
		return false;
	}

	ClassAd req( *req_ad );
	char buf[1024]; 

		// Add our own attributes to the request ad we're sending
	sprintf( buf, "%s = \"%s\"", ATTR_COMMAND,
			 getCommandString(CA_REQUEST_CLAIM) );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_CLAIM_TYPE, getClaimTypeString(type) );
	req.Insert( buf );

	return sendCACmd( &req, reply, true, timeout );
}


bool
DCStartd::activateClaim( const ClassAd* job_ad, ClassAd* reply, 
						 int timeout ) 
{
	setCmdStr( "activateClaim" );
	if( ! checkClaimId() ) {
		return false;
	}
	ClassAd req( *job_ad );
	char buf[1024]; 

		// Add our own attributes to the request ad we're sending
	sprintf( buf, "%s = \"%s\"", ATTR_COMMAND,
			 getCommandString(CA_ACTIVATE_CLAIM) );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_CLAIM_ID, claim_id );
	req.Insert( buf );

	return sendCACmd( &req, reply, true, timeout );
}


bool
DCStartd::suspendClaim( ClassAd* reply, int timeout )
{
	setCmdStr( "suspendClaim" );
	if( ! checkClaimId() ) {
		return false;
	}

	ClassAd req;
	char buf[1024]; 

		// Add our own attributes to the request ad we're sending
	sprintf( buf, "%s = \"%s\"", ATTR_COMMAND,
			 getCommandString(CA_SUSPEND_CLAIM) );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_CLAIM_ID, claim_id );
	req.Insert( buf );

	return sendCACmd( &req, reply, true, timeout );
}


bool
DCStartd::resumeClaim( ClassAd* reply, int timeout )
{
	setCmdStr( "resumeClaim" );
	if( ! checkClaimId() ) {
		return false;
	}

	ClassAd req;
	char buf[1024]; 

		// Add our own attributes to the request ad we're sending
	sprintf( buf, "%s = \"%s\"", ATTR_COMMAND,
			 getCommandString(CA_RESUME_CLAIM) );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_CLAIM_ID, claim_id );
	req.Insert( buf );

	return sendCACmd( &req, reply, true, timeout );
}


bool
DCStartd::deactivateClaim( VacateType type, ClassAd* reply,
						   int timeout )
{
	setCmdStr( "deactivateClaim" );
	if( ! checkClaimId() ) {
		return false;
	}
	if( ! checkVacateType(type) ) {
		return false;
	}

	ClassAd req;
	char buf[1024]; 

		// Add our own attributes to the request ad we're sending
	sprintf( buf, "%s = \"%s\"", ATTR_COMMAND,
			 getCommandString(CA_DEACTIVATE_CLAIM) );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_CLAIM_ID, claim_id );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_VACATE_TYPE,
			 getVacateTypeString(type) ); 
	req.Insert( buf );

 		// since deactivate could take a while, if we didn't already
		// get told what timeout to use, set the timeout to 0 so we
 		// don't bail out prematurely...
	if( timeout < 0 ) {
		return sendCACmd( &req, reply, true, 0 );
	} else {
		return sendCACmd( &req, reply, true, timeout );
	}
}


bool
DCStartd::releaseClaim( VacateType type, ClassAd* reply, 
						int timeout ) 
{
	setCmdStr( "releaseClaim" );
	if( ! checkClaimId() ) {
		return false;
	}
	if( ! checkVacateType(type) ) {
		return false;
	}

	ClassAd req;
	char buf[1024]; 

		// Add our own attributes to the request ad we're sending
	sprintf( buf, "%s = \"%s\"", ATTR_COMMAND,
			 getCommandString(CA_RELEASE_CLAIM) );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_CLAIM_ID, claim_id );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_VACATE_TYPE,
			 getVacateTypeString(type) );
	req.Insert( buf );

 		// since release could take a while, if we didn't already get
		// told what timeout to use, set the timeout to 0 so we don't
		// bail out prematurely...
	if( timeout < 0 ) {
		return sendCACmd( &req, reply, true, 0 );
	} else {
		return sendCACmd( &req, reply, true, timeout );
	}
}


bool
DCStartd::checkClaimId( void )
{
	if( claim_id ) {
		return true;
	}
	MyString err_msg;
	if( _cmd_str ) {
		err_msg += _cmd_str;
		err_msg += ": ";
	}
	err_msg += "called with no ClaimId";
	newError( err_msg.Value() );
	return false;
}


bool
DCStartd::checkVacateType( VacateType t )
{
	MyString err_msg;
	switch( t ) {
	case VACATE_GRACEFUL:
	case VACATE_FAST:
		break;
	default:
		err_msg = "Invalid VacateType (";
		err_msg += (int)t;
		err_msg += ')';
		newError( err_msg.Value() );
		return false;
	}
	return true;
}
