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
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "command_strings.h"
#include "daemon.h"
#include "dc_startd.h"
#include "condor_claimid_parser.h"
#include "my_username.h"


DCStartd::DCStartd( const char* tName, const char* tPool ) 
	: Daemon( DT_STARTD, tName, tPool )
{
	claim_id = NULL;
	extra_ids = NULL;
}


DCStartd::DCStartd( const char* tName, const char* tPool, const char* tAddr,
					const char* tId , const char *ids)
	: Daemon( DT_STARTD, tName, tPool )
{
	if( tAddr ) {
		Set_addr(tAddr);
	}
		// claim_id isn't initialized by Daemon's constructor, so we
		// have to treat it slightly differently 
	claim_id = NULL;
	if( tId ) {
		claim_id = strdup( tId );
	}

	extra_ids = NULL;
	if( ids && (strlen(ids) > 0)) {
		extra_ids = strdup( ids );
	}
}

DCStartd::DCStartd( const ClassAd *ad, const char *tPool )
	: Daemon(ad,DT_STARTD,tPool),
	  claim_id(NULL), extra_ids(NULL)
{
}

DCStartd::~DCStartd( void )
{
	if( claim_id ) {
		free(claim_id);
	}
	if( extra_ids ) {
		free(extra_ids);
	}
}


bool
DCStartd::setClaimId( const char* id ) 
{
	if( ! id ) {
		return false;
	}
	if( claim_id ) {
		free(claim_id);
		claim_id = NULL;
	}
	claim_id = strdup( id );
	return true;
}


ClaimStartdMsg::ClaimStartdMsg( char const *the_claim_id, char const *extra_claims, ClassAd const *job_ad, char const *the_description, char const *scheduler_addr, int alive_interval ):
 DCMsg(REQUEST_CLAIM)
{

	m_claim_id = the_claim_id;
	if (extra_claims) {
		m_extra_claims = extra_claims;
	}
	m_job_ad = *job_ad;
	m_description = the_description;
	m_scheduler_addr = scheduler_addr;
	m_alive_interval = alive_interval;
	m_num_dslots = 1;
	m_pslot_claim_lease = 0;
	m_claim_pslot = false;
	m_reply = NOT_OK;
	m_have_leftovers = false;
	m_have_claimed_slot_info = false;
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

		// Insert an attribute in the request ad to inform the
		// startd that this schedd is capable of understanding 
		// the newer protocol where the claim response may send
		// over any leftover resources from a partitionable slot.
	m_job_ad.Assign("_condor_SEND_LEFTOVERS",
		param_boolean("CLAIM_PARTITIONABLE_LEFTOVERS",true));

		// Insert an attribute in the request ad to inform the
		// startd that this schedd is capable of understanding
		// the newer protocol where any claim id in the response
		// is encrypted.
	m_job_ad.Assign("_condor_SECURE_CLAIM_ID", true);

		// Insert an attribute requesting the startd to send the
		// claimed slot ad in its response.
	m_job_ad.Assign("_condor_SEND_CLAIMED_AD", true);

		// Tell the startd whether we want the pslot to become Claimed
	m_job_ad.Assign("_condor_CLAIM_PARTITIONABLE_SLOT", m_claim_pslot);
	if (m_claim_pslot) {
		m_job_ad.Assign("_condor_PARTITIONABLE_SLOT_CLAIM_TIME", m_pslot_claim_lease);
		m_job_ad.Assign("_condor_WANT_MATCHING", true);
	}

		// Tell the startd how many dslots we want created off of a pslot
		// for this request. 0 is a reasonable answer when claiming the
		// pslot.
	m_job_ad.Assign("_condor_NUM_DYNAMIC_SLOTS", m_num_dslots);
	if (m_num_dslots > 0) m_claimed_slots.reserve(m_num_dslots);

	if( !sock->put_secret( m_claim_id.c_str() ) ||
	    !putClassAd( sock, m_job_ad ) ||
	    !sock->put( m_scheduler_addr.c_str() ) ||
	    !sock->put( m_alive_interval ) ||
		!this->putExtraClaims(sock))
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

bool
ClaimStartdMsg::putExtraClaims(Sock *sock) {

	const CondorVersionInfo *cvi = sock->get_peer_version();

		// Older versions of Condor don't know about extra claim ids.
		// But with SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION=True,
		// the schedd can't get the startd's version from the ReliSock.
		// In that case, use the old protocol if there are no extra
		// claim ids. Otherwise, assume the startd is new enough.
		// If it isn't, the claim request will probably fail anyway,
		// because the single claim won't have enough resources for
		// the request.
	if (!cvi && m_extra_claims.length() == 0) {
		return true;
	}

	if (cvi && !cvi->built_since_version(8,2,3)) {
		return true;
	}

	if (m_extra_claims.length() == 0) {
		return sock->put(0);
	}

	size_t begin = 0;
	size_t end = 0;
	std::list<std::string> claims;

	while ((end = m_extra_claims.find(' ', begin)) != std::string::npos) {
		std::string claim = m_extra_claims.substr(begin, end - begin);
		claims.push_back(claim);
		begin = end + 1;
	}
	
	int num_extra_claims = claims.size();
	if (!sock->put(num_extra_claims)) {
		return false;
	}

	while (num_extra_claims--) {
		if (!sock->put_secret(claims.front().c_str())) {
			return false;
		}
		
		claims.pop_front();
	}
	
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
		Reply of 0 (NOT_OK) means claim rejected.
		Reply of 1 (OK) means claim accepted.
		Reply of 3 (REQUEST_CLAIM_LEFTOVERS) means claim accepted by a
		  partitionable slot, and the "leftovers" slot ad and claim id
		  will be sent next.
		Reply of 4 (REQUEST_CLAIM_PAIR) is no longer used
		Reply of 5 (REQUEST_CLAIM_LEFTOVERS_2) is the same as 3, but
		  the claim id is encrypted.
		Reply of 6 (REQUEST_CLAIM_PAIR_2) is no longer used
		Reply of 7 (REQUEST_CLAIM_SLOT_AD) means claim accepted, the
		  claimed slot ad will be sent next, and either OK or p-slot
		  leftovers will be sent after that.
	*/

	while (m_reply == REQUEST_CLAIM_SLOT_AD) {
		_slotClaimInfo & info = m_claimed_slots.emplace_back();
		if (!sock->get_secret(info.claim_id) || !getClassAd(sock, info.slot_ad) || !sock->get(m_reply)) {
			dprintf(failureDebugLevel(),
			        "Response problem from startd when requesting claim %s.\n",
			        description());
			sockFailed(sock);
			return false;
		}
		// the claim id on the wire can have an explicit trailing null (or more?) at the end
		// that will mess up comparisons so remove it them here
		while ( ! info.claim_id.empty() && info.claim_id.back() == 0) info.claim_id.pop_back(); 
		m_have_claimed_slot_info = true;
	}

	if( m_reply == OK ) {
			// no need to log success, because DCMsg::reportSuccess() will
	} else if( m_reply == NOT_OK ) {
		dprintf( failureDebugLevel(), "Request was NOT accepted for claim %s\n", description() );
	} else if( m_reply == REQUEST_CLAIM_LEFTOVERS || m_reply == REQUEST_CLAIM_LEFTOVERS_2 ) {
		bool recv_ok = false;
		if ( m_reply == REQUEST_CLAIM_LEFTOVERS_2 ) {
			char *val = NULL;
			recv_ok = sock->get_secret(val);
			if ( recv_ok ) {
				m_leftover_claim_id = val;
				free(val);
			}
		} else {
			recv_ok = sock->get(m_leftover_claim_id);
		}
		if( !recv_ok ||
			!getClassAd( sock, m_leftover_startd_ad )  ) 
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
DCStartd::asyncRequestOpportunisticClaim( ClassAd const *req_ad, char const *description, char const *scheduler_addr, int alive_interval, bool claim_pslot, int timeout, int deadline_timeout, classy_counted_ptr<DCMsgCallback> cb )
{
	dprintf(D_FULLDEBUG|D_PROTOCOL,"Requesting claim %s\n",description);

	setCmdStr( "requestClaim" );
	ASSERT( checkClaimId() );
	ASSERT( checkAddr() );

	classy_counted_ptr<ClaimStartdMsg> msg = new ClaimStartdMsg( claim_id, extra_ids, req_ad, description, scheduler_addr, alive_interval );

	ASSERT( msg.get() );
	msg->setCallback(cb);

	if (claim_pslot) {
		// TODO Currently, we always request the pslot's max lease time
		//   (msg->m_pslot_claim_lease=0).
		//   Consider adding option to let client request shorter lease time.
		msg->m_claim_pslot = true;
	}

	// For now, requesting a WorkingCM means we don't want a dslot
	// (and our req_ad isn't a full job ad)
	std::string working_cm;
	req_ad->LookupString("WorkingCM", working_cm);
	if (!working_cm.empty()) {
		msg->m_num_dslots = 0;
	}

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

	if (IsDebugLevel(D_COMMAND)) {
		int cmd = graceful ? DEACTIVATE_CLAIM : DEACTIVATE_CLAIM_FORCIBLY;
		dprintf (D_COMMAND, "DCStartd::deactivateClaim(%s,...) making connection to %s\n", getCommandStringSafe(cmd), _addr.c_str());
	}

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr.c_str()) ) {
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
	if( !getClassAd(&reli_sock, response_ad) || !reli_sock.end_of_message() ) {
		newError(CA_COMMUNICATION_ERROR,
		         "DCStartd::deactivateClaim: failed to read response ad.");
		return false;
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
	if( ! putClassAd(tmp, *job_ad) ) {
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
		err += _addr.c_str();
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

		// Add our own attributes to the request ad we're sending
	req.Assign( ATTR_COMMAND, getCommandString(CA_REQUEST_CLAIM) );

	req.Assign( ATTR_CLAIM_TYPE, getClaimTypeString(cType) );

	return sendCACmd( &req, reply, true, timeout );
}


bool
DCStartd::updateMachineAd( const ClassAd * update, ClassAd * reply, int timeout ) {
	setCmdStr( "updateMachineAd" );

	ClassAd u( * update );
	u.Assign( ATTR_COMMAND, getCommandString( CA_UPDATE_MACHINE_AD ) );

	return sendCACmd( & u, reply, true, timeout );
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

	if (IsDebugLevel(D_COMMAND)) {
		int cmd = VACATE_CLAIM;
		dprintf (D_COMMAND, "DCStartd::vacateClaim(%s,...) making connection to %s\n", getCommandStringSafe(cmd), _addr.c_str());
	}

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr.c_str()) ) {
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

	if( ! reli_sock.put(name_vacate) ) {
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
	
	if (IsDebugLevel(D_COMMAND)) {
		int cmd = SUSPEND_CLAIM;
		dprintf (D_COMMAND, "DCStartd::_suspendClaim(%s,...) making connection to %s\n", getCommandStringSafe(cmd), _addr.c_str());
	}

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr.c_str()) ) {
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
	
	if (IsDebugLevel(D_COMMAND)) {
		int cmd = CONTINUE_CLAIM;
		dprintf (D_COMMAND, "DCStartd::_continueClaim(%s,...) making connection to %s\n", getCommandStringSafe(cmd), _addr.c_str());
	}

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr.c_str()) ) {
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

	if (IsDebugLevel(D_COMMAND)) {
		int cmd = PCKPT_JOB;
		dprintf (D_COMMAND, "DCStartd::checkpointJob(%s,...) making connection to %s\n", getCommandStringSafe(cmd), _addr.c_str());
	}

	bool  result;
	ReliSock reli_sock;
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr.c_str()) ) {
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
	if( ! reli_sock.put(name_ckpt) ) {
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
	const char* ad_addr;

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
	if( ! _cmd_str.empty() ) {
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
DCStartd::drainJobs(int how_fast,const char * reason,int on_completion,char const *check_expr,char const *start_expr,std::string &request_id)
{
	std::string error_msg;
	ClassAd request_ad;
	Sock *sock = startCommand( DRAIN_JOBS, Sock::reli_sock, 20 );
	if( !sock ) {
		formatstr(error_msg,"Failed to start DRAIN_JOBS command to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		return false;
	}

	if (reason) {
		request_ad.Assign(ATTR_DRAIN_REASON, reason);
	} else {
		auto_free_ptr username(my_username());
		if (! username) username.set(strdup("command"));
		std::string reason("by "); reason += username.ptr();
		request_ad.Assign(ATTR_DRAIN_REASON, reason);
	}
	request_ad.Assign(ATTR_HOW_FAST,how_fast);
	request_ad.Assign(ATTR_RESUME_ON_COMPLETION,on_completion);
	if( check_expr ) {
		request_ad.AssignExpr(ATTR_CHECK_EXPR,check_expr);
	}
	if( start_expr ) {
		request_ad.AssignExpr(ATTR_START_EXPR,start_expr);
	}

	if( !putClassAd(sock, request_ad) || !sock->end_of_message() ) {
		formatstr(error_msg,"Failed to compose DRAIN_JOBS request to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		delete sock;
		return false;
	}

	sock->decode();
	ClassAd response_ad;
	if( !getClassAd(sock, response_ad) || !sock->end_of_message() ) {
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

	if( !putClassAd(sock, request_ad) || !sock->end_of_message() ) {
		formatstr(error_msg,"Failed to compose CANCEL_DRAIN_JOBS request to %s",name());
		newError(CA_FAILURE,error_msg.c_str());
		return false;
	}

	sock->decode();
	ClassAd response_ad;
	if( !getClassAd(sock, response_ad) || !sock->end_of_message() ) {
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
