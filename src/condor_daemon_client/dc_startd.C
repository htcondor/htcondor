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


DCStartd::DCStartd( const char* name, const char* pool, const char* addr,
					const char* id )
	: Daemon( DT_STARTD, name, pool )
{
	if( addr ) {
		New_addr( strnewp(addr) );
	}
		// claim_id isn't initialized by Daemon's constructor, so we
		// have to treat it slightly differently 
	claim_id = NULL;
	if( id ) {
		claim_id = strnewp( id );
	}
}


DCStartd::~DCStartd( void )
{
	if( claim_id ) {
		delete [] claim_id;
	}
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
		MyString err = "DCStartd::deactivateClaim: ";
		err += "Failed to connect to startd (";
		err += _addr;
		err += ')';
		newError( CA_CONNECT_FAILED, err.Value() );
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
		MyString err = "DCStartd::deactivateClaim: ";
		err += "Failed to send command ";
		if( graceful ) {
			err += "DEACTIVATE_CLAIM";
		} else {
			err += "DEACTIVATE_CLAIM_FORCIBLY";
		}
		err += " to the startd";
		newError( CA_COMMUNICATION_ERROR, err.Value() );
		return false;
	}
		// Now, send the ClaimId
	if( ! reli_sock.code(claim_id) ) {
		MyString err = "DCStartd::deactivateClaim: ";
		err += "Failed to send ClaimId to the startd";
		newError( CA_COMMUNICATION_ERROR, err.Value() );
		return false;
	}
	if( ! reli_sock.eom() ) {
		MyString err = "DCStartd::deactivateClaim: ";
		err += "Failed to send EOM to the startd";
		newError( CA_COMMUNICATION_ERROR, err.Value() );
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
		MyString err = "DCStartd::activateClaim: ";
		err += "called with NULL claim_id, failing";
		newError( CA_INVALID_REQUEST, err.Value() );
		return CONDOR_ERROR;
	}

	Sock* tmp;
	tmp = startCommand( ACTIVATE_CLAIM, Stream::reli_sock, 20 ); 
	if( ! tmp ) {
		MyString err = "DCStartd::activateClaim: ";
		err += "Failed to send command ";
		err += "ACTIVATE_CLAIM";
		err += " to the startd";
		newError( CA_COMMUNICATION_ERROR, err.Value() );
		delete tmp;
		return CONDOR_ERROR;
	}
	if( ! tmp->code(claim_id) ) {
		MyString err = "DCStartd::activateClaim: ";
		err += "Failed to send ClaimId to the startd";
		newError( CA_COMMUNICATION_ERROR, err.Value() );
		delete tmp;
		return CONDOR_ERROR;
	}
	if( ! tmp->code(starter_version) ) {
		MyString err = "DCStartd::activateClaim: ";
		err += "Failed to send starter_version to the startd";
		newError( CA_COMMUNICATION_ERROR, err.Value() );
		delete tmp;
		return CONDOR_ERROR;
	}
	if( ! job_ad->put(*tmp) ) {
		MyString err = "DCStartd::activateClaim: ";
		err += "Failed to send job ClassAd to the startd";
		newError( CA_COMMUNICATION_ERROR, err.Value() );
		delete tmp;
		return CONDOR_ERROR;
	}
	if( ! tmp->end_of_message() ) {
		MyString err = "DCStartd::activateClaim: ";
		err += "Failed to send EOM to the startd";
		newError( CA_COMMUNICATION_ERROR, err.Value() );
		delete tmp;
		return CONDOR_ERROR;
	}

		// Now, try to get the reply
	tmp->decode();
	if( !tmp->code(reply) || !tmp->end_of_message()) {
		MyString err = "DCStartd::activateClaim: ";
		err += "Failed to receive reply from ";
		err += _addr;
		newError( CA_COMMUNICATION_ERROR, err.Value() );
		delete tmp;
		return CONDOR_ERROR;
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
		newError( CA_INVALID_REQUEST, err_msg.Value() );
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
DCStartd::locateStarter( const char* global_job_id, 
						 const char *claim_id,
						 ClassAd* reply,
						 int timeout )
{
	setCmdStr( "locateStarter" );

	ClassAd req;
	MyString line;

		// Add our own attributes to the request ad we're sending
	line = ATTR_COMMAND;
	line += "=\"";
	line += getCommandString( CA_LOCATE_STARTER );
	line += '"';
	req.Insert( line.Value() );

	line = ATTR_GLOBAL_JOB_ID;
	line += "=\"";
	line += global_job_id;
	line += '"';
	req.Insert( line.Value() );

	line = ATTR_CLAIM_ID;
	line += "=\"";
	line += claim_id;
	line += '"';
	req.Insert( line.Value() );


	return sendCACmd( &req, reply, false, timeout );
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
	newError( CA_INVALID_REQUEST, err_msg.Value() );
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
		newError( CA_INVALID_REQUEST, err_msg.Value() );
		return false;
	}
	return true;
}
