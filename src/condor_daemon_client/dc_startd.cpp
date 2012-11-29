/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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
#include "condor_string.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "command_strings.h"
#include "daemon.h"
#include "dc_startd.h"
#include "condor_claimid_parser.h"


DCStartd::DCStartd( const char* tName, const char* tPool ) 
	: Daemon( DT_STARTD, tName, tPool )
{
	claim_id = NULL;
}


DCStartd::DCStartd( const char* tName, const char* tPool, const char* tAddr,
					const char* tId )
	: Daemon( DT_STARTD, tName, tPool )
{
	if( tAddr ) {
		New_addr( strnewp(tAddr) );
	}
		// claim_id isn't initialized by Daemon's constructor, so we
		// have to treat it slightly differently 
	claim_id = NULL;
	if( tId ) {
		claim_id = strnewp( tId );
	}
}

DCStartd::DCStartd( const ClassAd *ad, const char *tPool )
	: Daemon(ad,DT_STARTD,tPool),
	  claim_id(NULL)
{
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


ClaimStartdMsg::ClaimStartdMsg( char const *the_claim_id, ClassAd const *job_ad, char const *the_description, char const *scheduler_addr, int alive_interval ):
 DCMsg(REQUEST_CLAIM)
{

	m_claim_id = the_claim_id;
	m_job_ad = *job_ad;
	m_description = the_description;
	m_scheduler_addr = scheduler_addr;
	m_alive_interval = alive_interval;
	m_reply = NOT_OK;
	m_have_leftovers = false;
}

void
ClaimStartdMsg::cancelMessage(char const *reason) {
	dprintf(D_ALWAYS,"Canceling request for claim %s %s\n", description(),reason ? reason : "");
	DCMsg::cancelMessage(reason);
}

bool
ClaimStartdMsg::writeMsg( DCMessenger * /*messenger*/, Sock *sock ) {
		// save startd fqu for hole punching
	m_startd_fqu = sock->getFullyQualifiedUser();
	m_startd_ip_addr = sock->peer_ip_str();

	std::string scheduler_addr_to_send = m_scheduler_addr;
	ConvertDefaultIPToSocketIP(ATTR_SCHEDD_IP_ADDR,scheduler_addr_to_send,*sock);

		// Insert an attribute in the request ad to inform the
		// startd that this schedd is capable of understanding 
		// the newer protocol where the claim response may send
		// over any leftover resources from a partitionable slot.
	m_job_ad.Assign("_condor_SEND_LEFTOVERS",
		param_boolean("CLAIM_PARTITIONABLE_LEFTOVERS",true));

	if( !sock->put_secret( m_claim_id.c_str() ) ||
	    !m_job_ad.put( *sock ) ||
	    !sock->put( scheduler_addr_to_send.c_str() ) ||
	    !sock->put( m_alive_interval ) )
	{
		dprintf(failureDebugLevel(),
				"Couldn't encode request claim to startd %s\n",
				description() );
		sockFailed( sock );
		return false;
	}
		// end_of_message() is done by caller
	return true;
}

DCMsg::MessageClosureEnum
ClaimStartdMsg::messageSent(DCMessenger *messenger, Sock *sock ) {
	messenger->startReceiveMsg( this, sock );
	return MESSAGE_CONTINUING;
}

bool
ClaimStartdMsg::readMsg( DCMessenger * /*messenger*/, Sock *sock ) {
	// Now, we set the timeout on the socket to 1 second.  Since we 
	// were called by as a Register_Socket callback, this should not 
	// block if things are working as expected.  
	// However, if the Startd wigged out and sent a 
	// partial int or some such, we cannot afford to block. -Todd 3/2000
	sock->timeout(1);

 	if( !sock->get(m_reply) ) {
		dprintf( failureDebugLevel(),
				 "Response problem from startd when requesting claim %s.\n",
				 description() );	
		sockFailed( sock );
		return false;
	}

	/* 
		Reply of 0 (OK) means claim accepted.
		Reply of 1 (NOT_OK) means claim rejected.
		Reply of 3 means claim accepted by a partitionable slot,
	      and the "leftovers" slot ad and claim id will be sent next.
	*/

	if( m_reply == OK ) {
			// no need to log success, because DCMsg::reportSuccess() will
	} else if( m_reply == NOT_OK ) {
		dprintf( failureDebugLevel(), "Request was NOT accepted for claim %s\n", description() );
	} else if( m_reply == 3 ) {
	 	if( !sock->get(m_leftover_claim_id) ||
			!m_leftover_startd_ad.initFromStream( *sock )  ) 
		{
			// failed to read leftover partitionable slot info
			dprintf( failureDebugLevel(),
				 "Failed to read paritionable slot leftover from startd - claim %s.\n",
				 description() );
			// treat this failure same as NOT_OK, since this startd is screwed
			m_reply = NOT_OK;
		} else {
			// successfully read leftover partitionable slot info
			m_have_leftovers = true;
			// change reply to OK cuz claim was a success
			m_reply = OK;
		}
	} else {
		dprintf( failureDebugLevel(), "Unknown reply from startd when requesting claim %s\n",description());
	}
		
	// end_of_message() is done by caller

	return true;
}


void
DCStartd::asyncRequestOpportunisticClaim( ClassAd const *req_ad, char const *description, char const *scheduler_addr, int alive_interval, int timeout, int deadline_timeout, classy_counted_ptr<DCMsgCallback> cb )
{
	dprintf(D_FULLDEBUG|D_PROTOCOL,"Requesting claim %s\n",description);

	setCmdStr( "requestClaim" );
	ASSERT( checkClaimId() );
	ASSERT( checkAddr() );

	classy_counted_ptr<ClaimStartdMsg> msg = new ClaimStartdMsg( claim_id, req_ad, description, scheduler_addr, alive_interval );

	ASSERT( msg.get() );
	msg->setCallback(cb);

	msg->setSuccessDebugLevel(D_ALWAYS|D_PROTOCOL);

		// if this claim is associated with a security session
	ClaimIdParser cid(claim_id);
	msg->setSecSessionId(cid.secSessionId());

	msg->setTimeout(timeout);
	msg->setDeadlineTimeout(deadline_timeout);
	sendMsg(msg.get());
}


bool 
DCStartd::deactivateClaim( bool graceful, bool *claim_is_closing )
{
	dprintf( D_FULLDEBUG, "Entering DCStartd::deactivateClaim(%s)\n",
			 graceful ? "graceful" : "forceful" );

	if( claim_is_closing ) {
		*claim_is_closing = false;
	}

	setCmdStr( "deactivateClaim" );
	if( ! checkClaimId() ) {
		return false;
	}
	if( ! checkAddr() ) {
		return false;
	}

		// if this claim is associated with a security session
	ClaimIdParser cidp(claim_id);
	char const *sec_session = cidp.secSessionId();

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr) ) {
		std::string err = "DCStartd::deactivateClaim: ";
		err += "Failed to connect to startd (";
		err += _addr;
		err += ')';
		newError( CA_CONNECT_FAILED, err.c_str() );
		return false;
	}
	int cmd;
	if( graceful ) {
		cmd = DEACTIVATE_CLAIM;
	} else {
		cmd = DEACTIVATE_CLAIM_FORCIBLY;
	}
	result = startCommand( cmd, (Sock*)&reli_sock, 20, NULL, NULL, false, sec_session ); 
	if( ! result ) {
		std::string err = "DCStartd::deactivateClaim: ";
		err += "Failed to send command ";
		if( graceful ) {
			err += "DEACTIVATE_CLAIM";
		} else {
			err += "DEACTIVATE_CLAIM_FORCIBLY";
		}
		err += " to the startd";
		newError( CA_COMMUNICATION_ERROR, err.c_str() );
		return false;
	}
		// Now, send the ClaimId
	if( ! reli_sock.put_secret(claim_id) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::deactivateClaim: Failed to send ClaimId to the startd" );
		return false;
	}
	if( ! reli_sock.end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::deactivateClaim: Failed to send EOM to the startd" );
		return false;
	}

	reli_sock.decode();
	ClassAd response_ad;
	if( !response_ad.initFromStream(reli_sock) || !reli_sock.end_of_message() ) {
		dprintf( D_FULLDEBUG, "DCStartd::deactivateClaim: failed to read response ad.\n");
			// The response ad is not critical and is expected to be missing
			// if the startd is from before 7.0.5.
	}
	else {
		bool start = true;
		response_ad.LookupBool(ATTR_START,start);
		if( claim_is_closing ) {
			*claim_is_closing = !start;
		}
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
		newError( CA_INVALID_REQUEST,
				  "DCStartd::activateClaim: called with NULL claim_id, failing" );
		return CONDOR_ERROR;
	}

		// if this claim is associated with a security session
	ClaimIdParser cidp(claim_id);
	char const *sec_session = cidp.secSessionId();

	Sock* tmp;
	tmp = startCommand( ACTIVATE_CLAIM, Stream::reli_sock, 20, NULL, NULL, false, sec_session ); 
	if( ! tmp ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::activateClaim: Failed to send command ACTIVATE_CLAIM to the startd" );
		return CONDOR_ERROR;
	}
	if( ! tmp->put_secret(claim_id) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::activateClaim: Failed to send ClaimId to the startd" );
		delete tmp;
		return CONDOR_ERROR;
	}
	if( ! tmp->code(starter_version) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::activateClaim: Failed to send starter_version to the startd" );
		delete tmp;
		return CONDOR_ERROR;
	}
	if( ! job_ad->put(*tmp) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::activateClaim: Failed to send job ClassAd to the startd" );
		delete tmp;
		return CONDOR_ERROR;
	}
	if( ! tmp->end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::activateClaim: Failed to send EOM to the startd" );
		delete tmp;
		return CONDOR_ERROR;
	}

		// Now, try to get the reply
	tmp->decode();
	if( !tmp->code(reply) || !tmp->end_of_message()) {
		std::string err = "DCStartd::activateClaim: ";
		err += "Failed to receive reply from ";
		err += _addr;
		newError( CA_COMMUNICATION_ERROR, err.c_str() );
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
DCStartd::requestClaim( ClaimType cType, const ClassAd* req_ad, 
						ClassAd* reply, int timeout )
{
	setCmdStr( "requestClaim" );

	std::string err_msg;
	switch( cType ) {
	case CLAIM_COD:
	case CLAIM_OPPORTUNISTIC:
		break;
	default:
		err_msg = "Invalid ClaimType (";
		err_msg += (int)cType;
		err_msg += ')';
		newError( CA_INVALID_REQUEST, err_msg.c_str() );
		return false;
	}

	ClassAd req( *req_ad );
	char buf[1024]; 

		// Add our own attributes to the request ad we're sending
	sprintf( buf, "%s = \"%s\"", ATTR_COMMAND,
			 getCommandString(CA_REQUEST_CLAIM) );
	req.Insert( buf );

	sprintf( buf, "%s = \"%s\"", ATTR_CLAIM_TYPE, getClaimTypeString(cType) );
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

		// Add our own attributes to the request ad we're sending
	req.Assign( ATTR_COMMAND, getCommandString(CA_ACTIVATE_CLAIM) );

	req.Assign( ATTR_CLAIM_ID, claim_id );

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

		// Add our own attributes to the request ad we're sending
	req.Assign( ATTR_COMMAND, getCommandString(CA_SUSPEND_CLAIM) );

	req.Assign( ATTR_CLAIM_ID, claim_id );

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

		// Add our own attributes to the request ad we're sending
	req.Assign( ATTR_COMMAND, getCommandString(CA_RESUME_CLAIM) );

	req.Assign( ATTR_CLAIM_ID, claim_id );

	return sendCACmd( &req, reply, true, timeout );
}


bool
DCStartd::deactivateClaim( VacateType vType, ClassAd* reply,
						   int timeout )
{
	setCmdStr( "deactivateClaim" );
	if( ! checkClaimId() ) {
		return false;
	}
	if( ! checkVacateType(vType) ) {
		return false;
	}

	ClassAd req;

		// Add our own attributes to the request ad we're sending
	req.Assign( ATTR_COMMAND, getCommandString(CA_DEACTIVATE_CLAIM) );

	req.Assign( ATTR_CLAIM_ID, claim_id );

	req.Assign( ATTR_VACATE_TYPE, getVacateTypeString(vType) ); 

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
DCStartd::releaseClaim( VacateType vType, ClassAd* reply, 
						int timeout ) 
{
	setCmdStr( "releaseClaim" );
	if( ! checkClaimId() ) {
		return false;
	}
	if( ! checkVacateType(vType) ) {
		return false;
	}

	ClassAd req;

		// Add our own attributes to the request ad we're sending
	req.Assign( ATTR_COMMAND, getCommandString(CA_RELEASE_CLAIM) );

	req.Assign( ATTR_CLAIM_ID, claim_id );

	req.Assign( ATTR_VACATE_TYPE, getVacateTypeString(vType) );

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
DCStartd::renewLeaseForClaim( ClassAd* reply, int timeout ) 
{
	setCmdStr( "renewLeaseForClaim" );
	if( ! checkClaimId() ) {
		return false;
	}

	ClassAd req;

		// Add our own attributes to the request ad we're sending
	req.Assign(ATTR_COMMAND,getCommandString(CA_RENEW_LEASE_FOR_CLAIM));

	req.Assign(ATTR_CLAIM_ID,claim_id);

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
						 const char *claimId,
						 const char *schedd_public_addr,
						 ClassAd* reply,
						 int timeout )
{
	setCmdStr( "locateStarter" );

	ClassAd req;

		// Add our own attributes to the request ad we're sending
	req.Assign(ATTR_COMMAND,getCommandString( CA_LOCATE_STARTER ));

	req.Assign(ATTR_GLOBAL_JOB_ID,global_job_id);

	req.Assign(ATTR_CLAIM_ID, claimId);

	if ( schedd_public_addr ) {
		req.Assign(ATTR_SCHEDD_IP_ADDR,schedd_public_addr);
	}

		// if this claim is associated with a security session
	ClaimIdParser cidp( claimId );

	return sendCACmd( &req, reply, false, timeout, cidp.secSessionId() );
}

int
DCStartd::delegateX509Proxy( const char* proxy, time_t expiration_time, time_t *result_expiration_time )
{
	dprintf( D_FULLDEBUG, "Entering DCStartd::delegateX509Proxy()\n" );

	setCmdStr( "delegateX509Proxy" );

	if( ! claim_id ) {
		newError( CA_INVALID_REQUEST,
				  "DCStartd::delegateX509Proxy: Called with NULL claim_id" );
		return CONDOR_ERROR;
	}

		// if this claim is associated with a security session
	ClaimIdParser cidp(claim_id);

	//
	// 1) begin the DELEGATE_GSI_CRED_STARTD command
	//
	ReliSock* tmp = (ReliSock*)startCommand( DELEGATE_GSI_CRED_STARTD,
	                                         Stream::reli_sock,
	                                         20, NULL, NULL, false,
											 cidp.secSessionId() ); 
	if( ! tmp ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::delegateX509Proxy: Failed to send command DELEGATE_GSI_CRED_STARTD to the startd" );
		return CONDOR_ERROR;
	}

	//
	// 2) get reply from startd - OK means continue, NOT_OK means
	//    don't bother (the startd doesn't require a delegated
	//    proxy
	//
	tmp->decode();
	int reply;
	if( !tmp->code(reply) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::delegateX509Proxy: failed to receive reply from startd (1)" );
		delete tmp;
		return CONDOR_ERROR;
	}
	if ( !tmp->end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::delegateX509Proxy: end of message error from startd (1)" );
		delete tmp;
		return CONDOR_ERROR;
	}
	if( reply == NOT_OK ) {
		delete tmp;
		return NOT_OK;
	}
		
	//
	// 3) send over the claim id and delegate (or copy) the given proxy
	//
	tmp->encode();
	int use_delegation =
		param_boolean( "DELEGATE_JOB_GSI_CREDENTIALS", true ) ? 1 : 0;
	if( !tmp->code( claim_id ) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::delegateX509Proxy: Failed to send claim id to the startd" );
		delete tmp;
		return CONDOR_ERROR;
	}
	if ( !tmp->code( use_delegation ) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::delegateX509Proxy: Failed to send use_delegation flag to the startd" );
		delete tmp;
		return CONDOR_ERROR;
	}
	int rv;
	filesize_t dont_care;
	if( use_delegation ) {
		rv = tmp->put_x509_delegation( &dont_care, proxy, expiration_time, result_expiration_time );
	}
	else {
		dprintf( D_FULLDEBUG,
		         "DELEGATE_JOB_GSI_CREDENTIALS is False; using direct copy\n");
		if( ! tmp->get_encryption() ) {
			newError( CA_COMMUNICATION_ERROR,
					  "DCStartd::delegateX509Proxy: Cannot copy: channel does not have encryption enabled" );
			delete tmp;
			return CONDOR_ERROR;
		}
		rv = tmp->put_file( &dont_care, proxy );
	}
	if( rv == -1 ) {
		newError( CA_FAILURE,
				  "DCStartd::delegateX509Proxy: Failed to delegate proxy" );
		delete tmp;
		return CONDOR_ERROR;
	}
	if ( !tmp->end_of_message() ) {
		newError( CA_FAILURE,
				  "DCStartd::delegateX509Proxy: end of message error to startd" );
		delete tmp;
		return CONDOR_ERROR;
	}

	// command successfully sent; now get the reply
	tmp->decode();
	if( !tmp->code(reply) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::delegateX509Proxy: failed to receive reply from startd (2)" );
		delete tmp;
		return CONDOR_ERROR;
	}
	if ( !tmp->end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::delegateX509Proxy: end of message error from startd (2)" );
		delete tmp;
		return CONDOR_ERROR;
	}
	delete tmp;

	dprintf( D_FULLDEBUG,
	         "DCStartd::delegateX509Proxy: successfully sent command, reply is: %d\n",
	         reply );

	return reply;
}

bool 
DCStartd::vacateClaim( const char* name_vacate )
{
	setCmdStr( "vacateClaim" );

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr) ) {
		std::string err = "DCStartd::vacateClaim: ";
		err += "Failed to connect to startd (";
		err += _addr;
		err += ')';
		newError( CA_CONNECT_FAILED, err.c_str() );
		return false;
	}

	int cmd = VACATE_CLAIM;

	result = startCommand( cmd, (Sock*)&reli_sock ); 
	if( ! result ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::vacateClaim: Failed to send command PCKPT_JOB to the startd" );
		return false;
	}

	if( ! reli_sock.code((unsigned char *)const_cast<char*>(name_vacate)) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::vacateClaim: Failed to send Name to the startd" );
		return false;
	}
	if( ! reli_sock.end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::vacateClaim: Failed to send EOM to the startd" );
		return false;
	}
		
	return true;
}

bool 
DCStartd::_suspendClaim( )
{
	setCmdStr( "suspendClaim" );
	
	if( ! checkClaimId() ) {
		return false;
	}
	if( ! checkAddr() ) {
		return false;
	}

	// if this claim is associated with a security session
	ClaimIdParser cidp(claim_id);
	char const *sec_session = cidp.secSessionId();
	
	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr) ) {
		std::string err = "DCStartd::_suspendClaim: ";
		err += "Failed to connect to startd (";
		err += _addr;
		err += ')';
		newError( CA_CONNECT_FAILED, err.c_str() );
		return false;
	}

	int cmd = SUSPEND_CLAIM;

	result = startCommand( cmd, (Sock*)&reli_sock, 20, NULL, NULL, false, sec_session ); 
	if( ! result ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::_suspendClaim: Failed to send command " );
		return false;
	}
	
	// Now, send the ClaimId
	if( ! reli_sock.put_secret(claim_id) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::_suspendClaim: Failed to send ClaimId to the startd" );
		return false;
	}

	if( ! reli_sock.end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::_suspendClaim: Failed to send EOM to the startd" );
		return false;
	}
	
	return true;
}

bool 
DCStartd::_continueClaim( )
{
	setCmdStr( "continueClaim" );

	if( ! checkClaimId() ) {
		return false;
	}
	if( ! checkAddr() ) {
		return false;
	}

	// if this claim is associated with a security session
	ClaimIdParser cidp(claim_id);
	char const *sec_session = cidp.secSessionId();
	
	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr) ) {
		std::string err = "DCStartd::_continueClaim: ";
		err += "Failed to connect to startd (";
		err += _addr;
		err += ')';
		newError( CA_CONNECT_FAILED, err.c_str() );
		return false;
	}

	int cmd = CONTINUE_CLAIM;

	result = startCommand( cmd, (Sock*)&reli_sock, 20, NULL, NULL, false, sec_session ); 
	if( ! result ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::_continueClaim: Failed to send command " );
		return false;
	}
	
	// Now, send the ClaimId
	if( ! reli_sock.put_secret(claim_id) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::_suspendClaim: Failed to send ClaimId to the startd" );
		return false;
	}

	if( ! reli_sock.end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::_continueClaim: Failed to send EOM to the startd" );
		return false;
	}
		
	return true;
}


bool 
DCStartd::checkpointJob( const char* name_ckpt )
{
	dprintf( D_FULLDEBUG, "Entering DCStartd::checkpointJob(%s)\n",
			 name_ckpt );

	setCmdStr( "checkpointJob" );

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr) ) {
		std::string err = "DCStartd::checkpointJob: ";
		err += "Failed to connect to startd (";
		err += _addr;
		err += ')';
		newError( CA_CONNECT_FAILED, err.c_str() );
		return false;
	}

	int cmd = PCKPT_JOB;

	result = startCommand( cmd, (Sock*)&reli_sock ); 
	if( ! result ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::checkpointJob: Failed to send command PCKPT_JOB to the startd" );
		return false;
	}

		// Now, send the name
	if( ! reli_sock.code((unsigned char *)const_cast<char*>(name_ckpt)) ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::checkpointJob: Failed to send Name to the startd" );
		return false;
	}
	if( ! reli_sock.end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "DCStartd::checkpointJob: Failed to send EOM to the startd" );
		return false;
	}
		// we're done
	dprintf( D_FULLDEBUG, "DCStartd::checkpointJob: "
			 "successfully sent command\n" );
	return true;
}


bool 
DCStartd::getAds( ClassAdList &adsList )
{
	CondorError errstack;
	// fetch the query
	QueryResult q;
	CondorQuery* query;
	char* ad_addr;

	// instantiate query object
	if (!(query = new CondorQuery (STARTD_AD))) {
		dprintf( D_ALWAYS, "Error:  Out of memory\n");
		return(false);
	}

	if( this->locate() ){
		ad_addr = this->addr();
		q = query->fetchAds(adsList, ad_addr, &errstack);
		if (q != Q_OK) {
        	if (q == Q_COMMUNICATION_ERROR) {
            	dprintf( D_ALWAYS, "%s\n", errstack.getFullText(true).c_str() );
        	}
        	else {
            	dprintf (D_ALWAYS, "Error:  Could not fetch ads --- %s\n",
                     	getStrQueryResult(q));
        	}
			delete query;
        	return (false);
		}
	} else {
		delete query;
		return(false);
	}

	delete query;
	return(true);
}

bool
DCStartd::checkClaimId( void )
{
	if( claim_id ) {
		return true;
	}
	std::string err_msg;
	if( _cmd_str ) {
		err_msg += _cmd_str;
		err_msg += ": ";
	}
	err_msg += "called with no ClaimId";
	newError( CA_INVALID_REQUEST, err_msg.c_str() );
	return false;
}


bool
DCStartd::checkVacateType( VacateType t )
{
	std::string err_msg;
	switch( t ) {
	case VACATE_GRACEFUL:
	case VACATE_FAST:
		break;
	default:
		formatstr(err_msg, "Invalid VacateType (%d)", (int)t);
		newError( CA_INVALID_REQUEST, err_msg.c_str() );
		return false;
	}
	return true;
}

DCClaimIdMsg::DCClaimIdMsg( int cmd, char const *claim_id ):
	DCMsg( cmd )
{
	m_claim_id = claim_id;
}

bool DCClaimIdMsg::writeMsg( DCMessenger *, Sock *sock )
{
	if( !sock->put_secret( m_claim_id.c_str() ) ) {
		sockFailed( sock );
		return false;
	}
	return true;
}

bool DCClaimIdMsg::readMsg( DCMessenger *, Sock *sock )
{
	char *str = NULL;
	if( !sock->get_secret( str ) ){
		sockFailed( sock );
		return false;
	}
	m_claim_id = str;
	free(str);

	return true;
}

bool
DCStartd::drainJobs(int how_fast,bool resume_on_completion,char const *check_expr,std::string &request_id)
{
	std::string error_msg;
	ClassAd request_ad;
	Sock *sock = startCommand( DRAIN_JOBS, Sock::reli_sock, 20 );
	if( !sock ) {
		formatstr(error_msg,"Failed to start DRAIN_JOBS command to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		return false;
	}

	request_ad.Assign(ATTR_HOW_FAST,how_fast);
	request_ad.Assign(ATTR_RESUME_ON_COMPLETION,resume_on_completion);
	if( check_expr ) {
		request_ad.AssignExpr(ATTR_CHECK_EXPR,check_expr);
	}

	if( !request_ad.put(*sock) || !sock->end_of_message() ) {
		formatstr(error_msg,"Failed to compose DRAIN_JOBS request to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		delete sock;
		return false;
	}

	sock->decode();
	ClassAd response_ad;
	if( !response_ad.initFromStream(*sock) || !sock->end_of_message() ) {
		formatstr(error_msg,"Failed to get response to DRAIN_JOBS request to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		delete sock;
		return false;
	}

	response_ad.LookupString(ATTR_REQUEST_ID,request_id);

	bool result = false;
	int error_code = 0;
	response_ad.LookupBool(ATTR_RESULT,result);
	if( !result ) {
		std::string remote_error_msg;
		response_ad.LookupString(ATTR_ERROR_STRING,remote_error_msg);
		response_ad.LookupInteger(ATTR_ERROR_CODE,error_code);
		formatstr(error_msg,
				"Received failure from %s in response to DRAIN_JOBS request: error code %d: %s",
				name(),error_code,remote_error_msg.c_str());
		newError(CA_FAILURE,error_msg.c_str());
		delete sock;
		return false;
	}

	delete sock;
	return true;
}

bool
DCStartd::cancelDrainJobs(char const *request_id)
{
	std::string error_msg;
	ClassAd request_ad;
	Sock *sock = startCommand( CANCEL_DRAIN_JOBS, Sock::reli_sock, 20 );
	if( !sock ) {
		formatstr(error_msg,"Failed to start CANCEL_DRAIN_JOBS command to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		return false;
	}

	if( request_id ) {
		request_ad.Assign(ATTR_REQUEST_ID,request_id);
	}

	if( !request_ad.put(*sock) || !sock->end_of_message() ) {
		formatstr(error_msg,"Failed to compose CANCEL_DRAIN_JOBS request to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		return false;
	}

	sock->decode();
	ClassAd response_ad;
	if( !response_ad.initFromStream(*sock) || !sock->end_of_message() ) {
		formatstr(error_msg,"Failed to get response to CANCEL_DRAIN_JOBS request to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		delete sock;
		return false;
	}

	bool result = false;
	int error_code = 0;
	response_ad.LookupBool(ATTR_RESULT,result);
	if( !result ) {
		std::string remote_error_msg;
		response_ad.LookupString(ATTR_ERROR_STRING,remote_error_msg);
		response_ad.LookupInteger(ATTR_ERROR_CODE,error_code);
		formatstr(error_msg,
				"Received failure from %s in response to CANCEL_DRAIN_JOBS request: error code %d: %s",
				name(),error_code,remote_error_msg.c_str());
		newError(CA_FAILURE,error_msg.c_str());
		delete sock;
		return false;
	}

	delete sock;
	return true;
}
