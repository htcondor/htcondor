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
#include "condor_classad_util.h"
#include "condor_mkstemp.h"
#include "startd.h"
#include "vm_common.h"

/* XXX fix me */
#include "../condor_sysapi/sysapi.h"

#ifndef max
#define max(x,y) (((x) < (y)) ? (y) : (x))
#endif

extern "C" int tcp_accept_timeout( int, struct sockaddr*, int*, int );

static int deactivate_claim(Stream *stream, Resource *rip, bool graceful);

int
command_handler( Service*, int cmd, Stream* stream )
{
	int rval = FALSE;
	Resource* rip;
	if( ! (rip = stream_to_rip(stream)) ) {
		return FALSE;
	}
	State s = rip->state();

		// The rest of these only make sense in claimed state 
	if( s != claimed_state ) {
		rip->log_ignore( cmd, s );
		return FALSE;
	}
	switch( cmd ) {
	case ALIVE:
		rval = rip->got_alive();
		break;
	case DEACTIVATE_CLAIM:
	case DEACTIVATE_CLAIM_FORCIBLY:
		rval = deactivate_claim(stream,rip,cmd == DEACTIVATE_CLAIM);
		break;
	case PCKPT_FRGN_JOB:
		rval = rip->periodic_checkpoint();
		break;
	case REQ_NEW_PROC:
		if( resmgr->isShuttingDown() ) {
			rip->log_shutdown_ignore( cmd );
			rval = FALSE;
		} else {
			rval = rip->request_new_proc();
		}
		break;
	}
	return rval;
}

int
deactivate_claim(Stream *stream, Resource *rip, bool graceful)
{
	int rval;
	bool claim_is_closing = rip->curClaimIsClosing();

		// send response to shadow before killing starter to avoid a
		// 3-way deadlock (under windows) where startd blocks trying to
		// authenticate DC_RAISESIGNAL to starter while starter is
		// blocking trying to send update to shadow.
	stream->encode();

	ClassAd response_ad;
	response_ad.Assign(ATTR_START,!claim_is_closing);
	if( !response_ad.put(*stream) || !stream->eom() ) {
		dprintf(D_FULLDEBUG,"Failed to send response ClassAd in deactivate_claim.\n");
			// Prior to 7.0.5, no response ClassAd was expected.
			// Anyway, failure to send it is not (currently) critical
			// in any way.

		claim_is_closing = false;
	}

	if(graceful) {
		rval = rip->deactivate_claim();
	}
	else {
		rval = rip->deactivate_claim_forcibly();
	}

	if( claim_is_closing && rip->r_cur ) {
			// We told the submit-side this claim is closing, so there is
			// no need to exchange RELEASE_CLAIM messages.  Behave as
			// though the schedd has already sent us RELEASE_CLAIM.
		rip->r_cur->scheddClosedClaim();
		rip->void_release_claim();
	}

	return rval;
}

int
command_activate_claim( Service*, int cmd, Stream* stream )
{
	char* id = NULL;
	Resource* rip;

	if( ! stream->get_secret(id) ) {
		dprintf( D_ALWAYS, "Can't read ClaimId\n" );
		free( id );
		return FALSE;
	}
	rip = resmgr->get_by_cur_id( id );
	if( !rip ) {
		ClaimIdParser idp( id );
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with ClaimId (%s)\n", idp.publicClaimId() );
		free( id );
		stream->end_of_message();
		reply( stream, NOT_OK );
		return FALSE;
	}
	free( id );

	if ( resmgr->isShuttingDown() ) {
		rip->log_shutdown_ignore( cmd );
		stream->end_of_message();
		reply( stream, NOT_OK );
		return FALSE;
	}

	State s = rip->state();
	Activity a = rip->activity();

	if( s != claimed_state ) {
			// If we're not already claimed, any kind of
			// ACTIVATE_CLAIM is invalid.
		rip->log_ignore( ACTIVATE_CLAIM, s );
		stream->end_of_message();
		reply( stream, NOT_OK );
		return FALSE;
	}

	if( rip->isDeactivating() ) { 
			// We're in the middle of deactivating another claim, so
			// tell the shadow to try again.  Any shadow before 6.1.9
			// will just treat this like a "NOT_OK", which is what
			// we'd expect.  However, a 6.1.9 or later shadow will
			// honor the try again, sleep a little while, and try to
			// initiate a new ACTIVATE_CLAIM protocol.
		rip->dprintf( D_ALWAYS, 
					  "Got activate claim while starter is still alive.\n" );
		rip->dprintf( D_ALWAYS, 
					  "Telling shadow to try again later.\n" );
		stream->end_of_message();
		reply( stream, CONDOR_TRY_AGAIN );
		return FALSE;
	}
	
		// If we got this far and we're not in claimed/idle, there
		// really is a problem activating the claim.
	if( a != idle_act ) {
		rip->log_ignore( ACTIVATE_CLAIM, s, a );
		stream->end_of_message();
		reply( stream, NOT_OK );
		return FALSE;
	}

		// If we got to here, everything's cool.  Do the work. 
	return activate_claim( rip, stream );
}


int
command_vacate_all( Service*, int cmd, Stream* ) 
{
	dprintf( D_ALWAYS, "command_vacate_all() called.\n" );
	switch( cmd ) {
	case VACATE_ALL_CLAIMS:
		dprintf( D_ALWAYS, "State change: received VACATE_ALL_CLAIMS command\n" );
		resmgr->walk( &Resource::void_retire_claim );
		break;
	case VACATE_ALL_FAST:
		dprintf( D_ALWAYS, "State change: received VACATE_ALL_FAST command\n" );
		resmgr->walk( &Resource::void_kill_claim );
		break;
	default:
		EXCEPT( "Unknown command (%d) in command_vacate_all", cmd );
		break;
	}
	return TRUE;
}


int
command_pckpt_all( Service*, int, Stream* ) 
{
	dprintf( D_ALWAYS, "command_pckpt_all() called.\n" );
	resmgr->walk( &Resource::void_periodic_checkpoint );
	return TRUE;
}


int
command_x_event( Service*, int, Stream* s ) 
{
	dprintf( D_FULLDEBUG, "command_x_event() called.\n" );

	sysapi_last_xevent();

	if( s ) {
		s->end_of_message();
	}
	return TRUE;
}


int
command_give_state( Service*, int, Stream* stream ) 
{
	int rval = TRUE;
	char* tmp;
	dprintf( D_FULLDEBUG, "command_give_state() called.\n" );
	stream->encode();
	tmp = strdup( state_to_string(resmgr->state()) );
	if ( ! stream->code( tmp ) ||
		 ! stream->end_of_message() ) {
		dprintf( D_FULLDEBUG, "command_give_state(): failed to send state\n" );
		rval = FALSE;
	}
	free( tmp );
	return rval;
}

int
command_give_totals_classad( Service*, int, Stream* stream ) 
{
	int rval = FALSE;
	dprintf( D_FULLDEBUG, "command_give_totals_classad() called.\n" );
	stream->encode();
	if( resmgr->totals_classad ) {
		resmgr->totals_classad->put( *stream );
		rval = TRUE;
	}
	stream->end_of_message();
	return rval;
}


int
command_request_claim( Service*, int cmd, Stream* stream ) 
{
	char* id = NULL;
	Resource* rip;
	int rval;

	if( ! stream->get_secret(id) ) {
		dprintf( D_ALWAYS, "Can't read ClaimId\n" );
		if( id ) { 
			free( id );
		}
		refuse( stream );
		return FALSE;
	}

	rip = resmgr->get_by_any_id( id );
	if( !rip ) {
		ClaimIdParser idp( id );
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with ClaimId (%s)\n", idp.publicClaimId() );
		free( id );
		refuse( stream );
		return FALSE;
	}

	if( resmgr->isShuttingDown() ) {
		rip->log_shutdown_ignore( cmd );
		free( id );
		refuse( stream );
		return FALSE;
	}

	State s = rip->state();
	if( s == preempting_state ) {
		rip->log_ignore( REQUEST_CLAIM, s );
		free( id );
		refuse( stream );
		return FALSE;
	}

	Claim *claim = NULL;
	if( rip->state() == claimed_state ) {
		if( rip->r_pre_pre ) {
			claim = rip->r_pre_pre;
		}
		else {
			claim = rip->r_pre;
		}
	} else {
		claim = rip->r_cur;
	}
	ASSERT( claim );
	if( !claim->idMatches(id) ) {
			// This request doesn't match the right claim ID.  It must
			// match one of the other claim IDs associated with this
			// resource (e.g. because the negotiator matched the job
			// against a stale machine ClassAd).
		rip->log_ignore( REQUEST_CLAIM, s );
		free( id );
		refuse( stream );
		return FALSE;
	}

	rval = request_claim( rip, claim, id, stream );
	free( id );
	return rval;
}

int
command_release_claim( Service*, int cmd, Stream* stream ) 
{
	char* id = NULL;
	Resource* rip;

	if( ! stream->get_secret(id) ) {
		dprintf( D_ALWAYS, "Can't read ClaimId\n" );
		if( id ) { 
			free( id );
		}
		refuse( stream );
		return FALSE;
	}

	rip = resmgr->get_by_any_id( id );
	if( !rip ) {
		ClaimIdParser idp( id );
		dprintf( D_FULLDEBUG, 
				 "Can't find resource with ClaimId (%s); perhaps this claim was removed already.\n", idp.publicClaimId() );
		free( id );
		refuse( stream );
		return FALSE;
	}

	if( !stream->end_of_message() ) {
		ClaimIdParser idp( id );
		dprintf( D_ALWAYS,
				 "Error: can't read end of message for RELEASE_CLAIM %s.\n",
				 idp.publicClaimId() );
		return FALSE;
	}

	State s = rip->state();

	//There are two cases: claim id is the current or the preempting claim
	if( rip->r_pre && rip->r_pre->idMatches(id) ) {
		// preempting claim is being canceled by schedd
		rip->dprintf( D_ALWAYS, 
		              "State change: received RELEASE_CLAIM command from preempting claim\n" );
		rip->r_pre->scheddClosedClaim();
		rip->removeClaim(rip->r_pre);
		free(id);
		return TRUE;
	}
	else if( rip->r_pre_pre && rip->r_pre_pre->idMatches(id) ) {
		// preempting preempting claim is being canceled by schedd
		rip->dprintf( D_ALWAYS, 
		              "State change: received RELEASE_CLAIM command from preempting preempting claim\n" );
		rip->r_pre_pre->scheddClosedClaim();
		rip->removeClaim(rip->r_pre_pre);
		free(id);
		return TRUE;
	}
	else if( rip->r_cur && rip->r_cur->idMatches(id) ) {
		if( (s == claimed_state) || (s == matched_state) ) {
			rip->dprintf( D_ALWAYS, 
						  "State change: received RELEASE_CLAIM command\n" );
			free(id);
			rip->r_cur->scheddClosedClaim();
			return rip->release_claim();
		} else {
			rip->log_ignore( cmd, s );
			free(id);
			return FALSE;
		}
	}

	// This should never happen unless get_by_any_id() changes.
	EXCEPT("Neither pre nor cur claim matches claim id: %s",id);
	return FALSE;
}

int
command_name_handler( Service*, int cmd, Stream* stream ) 
{
	char* name = NULL;
	Resource* rip;

	if( ! stream->code(name) ) {
		dprintf( D_ALWAYS, "Can't read name\n" );
		free( name );
		return FALSE;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		free( name );
		return FALSE;
	}
	rip = resmgr->get_by_name( name );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with name (%s)\n",
				 name );
		free( name );
		return FALSE;
	}
	free( name );

	State s = rip->state();
	switch( cmd ) {
	case VACATE_CLAIM:
		switch( s ) {
		case claimed_state:
		case matched_state:
#if HAVE_BACKFILL
		case backfill_state:
#endif /* HAVE_BACKFILL */
			rip->dprintf( D_ALWAYS, 
						  "State change: received VACATE_CLAIM command\n" );
			return rip->retire_claim();
			break;

		default:
			rip->log_ignore( cmd, s );
			return FALSE;
			break;
		}
		break;
	case VACATE_CLAIM_FAST:
		switch( s ) {
		case claimed_state:
		case matched_state:
		case preempting_state:
#if HAVE_BACKFILL
		case backfill_state:
#endif /* HAVE_BACKFILL */
			rip->dprintf( D_ALWAYS, 
						  "State change: received VACATE_CLAIM_FAST command\n" );
			return rip->kill_claim();
			break;
		default:
			rip->log_ignore( cmd, s );
			return FALSE;
			break;
		}
		break;
	case PCKPT_JOB:
		if( s == claimed_state ) {
			return rip->periodic_checkpoint();
		} else {
			rip->log_ignore( cmd, s );
			return FALSE;
		}
		break;
	default:
		EXCEPT( "Unknown command (%d) in command_name_handler", cmd );
	}
	return FALSE;
}


int
command_match_info( Service*, int cmd, Stream* stream ) 
{
	char* id = NULL;
	Resource* rip;
	int rval;

	if( ! stream->get_secret(id) ) {
		dprintf( D_ALWAYS, "Can't read ClaimId\n" );
		free( id );
		return FALSE;
	}
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error: can't read end of message for MATCH_INFO.\n" );
		return FALSE;
	}

		// Find Resource object for this ClaimId
	rip = resmgr->get_by_any_id( id );
	if( !rip ) {
		ClaimIdParser idp( id );
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with ClaimId (%s)\n", idp.publicClaimId() );
		free( id );
		return FALSE;
	}

	if( resmgr->isShuttingDown() ) {
		rip->log_shutdown_ignore( cmd );
		free( id );
		return FALSE;
	}

		// Check resource state.  Ignore if we're preempting or
		// matched, otherwise process the command. 
	State s = rip->state();
	if( s == matched_state || s == preempting_state ) {
		rip->log_ignore( MATCH_INFO, s );
		rval = FALSE;
	} else {
		rval = match_info( rip, id );
	}
	free( id );
	return rval;
}


int
command_give_request_ad( Service*, int, Stream* stream) 
{
	int pid = -1;  // Starter sends it's pid so we know what
				   // resource's request ad to send 
	Claim*		claim;
	ClassAd*	cp;

	if( ! stream->code(pid) ) {
		dprintf( D_ALWAYS, "give_request_ad: Can't read pid\n" );
		stream->encode();
		stream->end_of_message();
		return FALSE;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "give_request_ad: Can't read eom\n" );
		stream->encode();
		stream->end_of_message();
		return FALSE;
	}
	claim = resmgr->getClaimByPid( pid );
	if( !claim ) {
		dprintf( D_ALWAYS, 
				 "give_request_ad: Can't find starter with pid %d\n",
				 pid ); 
		stream->encode();
		stream->end_of_message();
		return FALSE;
	}
	cp = claim->ad();
	if( !cp ) {
		claim->dprintf( D_ALWAYS, 
						"give_request_ad: current claim has NULL classad.\n" );
		stream->encode();
		stream->end_of_message();
		return FALSE;
	}
	stream->encode();
	cp->put( *stream );
	stream->end_of_message();
	return TRUE;
}


int
command_query_ads( Service*, int, Stream* stream) 
{
	ClassAd queryAd;
	ClassAd *ad;
	ClassAdList ads;
	int more = 1, num_ads = 0;
   
	dprintf( D_FULLDEBUG, "In command_query_ads\n" );

	stream->decode();
    stream->timeout(15);
	if( !queryAd.initFromStream(*stream) || !stream->end_of_message()) {
        dprintf( D_ALWAYS, "Failed to receive query on TCP: aborting\n" );
		return FALSE;
	}

		// Construct a list of all our ClassAds:
	resmgr->makeAdList( &ads );
	
		// Now, find the ClassAds that match.
	stream->encode();
	ads.Open();
	while( (ad = ads.Next()) ) {
		if( IsAHalfMatch( &queryAd, ad ) ) {
			if( !stream->code(more) || !ad->put(*stream) ) {
				dprintf (D_ALWAYS, 
						 "Error sending query result to client -- aborting\n");
				return FALSE;
			}
			num_ads++;
        }
	}

		// Finally, close up shop.  We have to send NO_MORE.
	more = 0;
	if( !stream->code(more) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending EndOfResponse (0) to client\n" );
		return FALSE;
	}
    dprintf( D_FULLDEBUG, "Sent %d ads in response to query\n", num_ads ); 
	return TRUE;
}

int
command_vm_register( Service*, int, Stream* s )
{
	char *raddr = NULL;

	s->decode();
    	s->timeout(5);
	if( !s->code(raddr) ) {
		dprintf( D_ALWAYS, "command_vm_register: Can't read register IP\n");
		free(raddr);
		return FALSE;
	}

	dprintf( D_FULLDEBUG, "command_vm_register() is called with IP(%s).\n", raddr );

	if( !s->end_of_message() ){
		dprintf( D_ALWAYS, "command_vm_register: Can't read end_of_message\n");
		free(raddr);
		return FALSE;
	}

	int permission = 0;

	if( vmapi_register_cmd_handler(raddr, &permission) == TRUE ) {
		s->encode();
		s->code(permission);
		s->end_of_message();
	}else{
		free(raddr);
		return FALSE;
	}

	free(raddr);
	return TRUE;
}

int
command_vm_universe( Service*, int cmd, Stream* stream )
{
	char *value = NULL; // Pid of Starter
	int starter_pid = 0;
	int vm_pid = 0;

	stream->decode();
	stream->timeout(5);
	if( !stream->code(value) ) {
		dprintf( D_ALWAYS, "command_vm_universe: "
							"Can't read PID of local starter\n");
		if( value ) {
			free(value);
		}
		return FALSE;
	}
	starter_pid = (int)strtol(value, (char **)NULL, 10);
	if( starter_pid <= 0 ) {
		dprintf( D_ALWAYS, "command_vm_universe: "
							"Invalid PID of starter\n");
		free(value);
		return FALSE;
	}
	free(value);
	value = NULL;

	switch( cmd ) {
		case VM_UNIV_GAHP_ERROR:
			resmgr->m_vmuniverse_mgr.checkVMUniverse();
			break;
		case VM_UNIV_VMPID:
			// Read vm pid
			value = NULL;
			if( !stream->code(value) ) {
				dprintf( D_ALWAYS, "command_vm_universe: "
						"Can't read PID of process for a VM\n");
				if( value ) {
					free(value);
				}
				return FALSE;
			}
			vm_pid = (int)strtol(value, (char **)NULL, 10);
			if( vm_pid < 0 ) {
				dprintf( D_ALWAYS, "command_vm_universe: "
						"Invalid PID(%s) of process for a VM\n", value);
				free(value);
				return FALSE;
			}
			free(value);
			value = NULL;

			resmgr->m_vmuniverse_mgr.addProcessForVM(starter_pid, vm_pid);
			break;
		case VM_UNIV_GUEST_IP:
			value = NULL;
			if( !stream->code(value) ) {
				dprintf( D_ALWAYS, "command_vm_universe: "
						"Can't read IP of VM\n");
				if( value ) {
					free(value);
				}
				return FALSE;
			}
			resmgr->m_vmuniverse_mgr.addIPForVM(starter_pid, value);
			free(value);
			value = NULL;
			break;
		case VM_UNIV_GUEST_MAC:
			value = NULL;
			if( !stream->code(value) ) {
				dprintf( D_ALWAYS, "command_vm_universe: "
						"Can't read MAC of VM\n");
				if( value ) {
					free(value);
				}
				return FALSE;
			}
			resmgr->m_vmuniverse_mgr.addMACForVM(starter_pid, value);
			free(value);
			value = NULL;
			break;
		default:
			dprintf( D_ALWAYS, "command_vm_universe(command=%d) is "
							"not supported\n", cmd);
	}

	dprintf( D_FULLDEBUG, "command_vm_universe(%s) is called from "
			"local starter(pid=%d).\n", getCommandString(cmd), starter_pid);

	if( !stream->end_of_message() ){
		dprintf( D_ALWAYS, "command_vm_universe: Can't read end_of_message\n");
		return FALSE;
	}

	return TRUE;
}

#if !defined(WIN32)
int
command_delegate_gsi_cred( Service*, int, Stream* stream )
{
	// The shadow is trying to delegate its user proxy, in case
	// we plan to use glexec to spawn the starter (which we will
	// if GLEXEC_STARTER is set in config)

	if( stream->type() != Stream::reli_sock ) {
		dprintf( D_ALWAYS, "request to delegate proxy via UDP denied\n");
		return FALSE;
	}
	ReliSock* sock = (ReliSock*)stream;
 
	//
	// 1) send OK if we need delegation (GLEXEC_STARTER is set),
	//    NOT_OK otherwise
	//
	if( ! sock->end_of_message() ) {
		dprintf( D_ALWAYS, "end of message error (1)\n" );
		return FALSE;
	}
	if ( ! param_boolean( "GLEXEC_STARTER", false ) ) {
		dprintf( D_ALWAYS,
		         "GLEXEC_STARTER is false, cancelling delegation\n" );
		if( ! reply( sock, NOT_OK ) ) {
			dprintf( D_ALWAYS,
			         "error sending NOT_OK to cancel delegation\n" );
			return FALSE;
		}
		return TRUE;
	}
	if( ! reply( sock, OK ) ) {
		dprintf( D_ALWAYS, "error sending OK to begin delegation\n");
		return FALSE;
	}

	//
	// 2) get claim id and delegated proxy off the wire; send OK
	//    if all secceeds
	//
	sock->decode();			 
	char* id = NULL;
    if( ! sock->code(id) ) {
        dprintf( D_ALWAYS, "error reading claim id\n" );
			// If we couldn't read it, no sense trying to reply ERROR.
        return FALSE;
    }

    Claim* claim = resmgr->getClaimById( id );
    if( !claim ) {
        dprintf( D_ALWAYS,
                 "error finding resource with claim id (%s)\n", id );
        free( id );
		reply( sock, CONDOR_ERROR );
        return FALSE;
    }
    free( id );

	// Make sure the claim is idle
	if (claim->state() != CLAIM_IDLE) {
		Resource* rip = claim->rip();
		rip->dprintf(D_ALWAYS,
					 "Got %s for a %s claim (not idle), ignoring.\n",
					 getCommandString(DELEGATE_GSI_CRED_STARTD),
					 getClaimStateString(claim->state()));
		reply( sock, CONDOR_ERROR );
		return FALSE;
	}

	// create a temporary file to hold the proxy and set it
	// to mode 600
	MyString proxy_file;
	char* glexec_user_dir = param("GLEXEC_USER_DIR");
	if (glexec_user_dir != NULL) {
		proxy_file = glexec_user_dir;
		free(glexec_user_dir);
	}
	else {
		proxy_file = "/tmp";
	}
	proxy_file += "/startd-tmp-proxy-XXXXXX";
	char* proxy_file_tmp = strdup(proxy_file.Value());
	ASSERT(proxy_file_tmp != NULL);
	int fd = condor_mkstemp( proxy_file_tmp );
	proxy_file = proxy_file_tmp;
	free( proxy_file_tmp );
	if( fd == -1 ) {
		dprintf( D_ALWAYS,
		         "error creating temp file for proxy: %s (%d)\n",
		         strerror( errno ), errno );
		sock->end_of_message();
		reply( sock, CONDOR_ERROR );
		return FALSE;
	}
	close( fd );

	dprintf( D_FULLDEBUG,
	         "writing temporary proxy to: %s\n",
	         proxy_file.Value() );

	// sender decides whether to use delegation or simply copy
	int use_delegation;
	if( ! sock->code(use_delegation) ) {
		dprintf( D_ALWAYS, "error reading delegation request\n" );
		return FALSE;
	}

	int rv;
	filesize_t dont_care;
	if( use_delegation ) {
		rv = sock->get_x509_delegation( &dont_care, proxy_file.Value() );
	}
	else {
		dprintf( D_FULLDEBUG,
		         "DELEGATE_JOB_GSI_CREDENTIALS is False; using direct copy\n");
		if( ! sock->get_encryption() ) {
			dprintf( D_ALWAYS,
			         "cannot copy: encryption not enabled on channel\n" );
			sock->end_of_message();
			reply( sock, CONDOR_ERROR );
			return FALSE;
		}
		rv = sock->get_file( &dont_care, proxy_file.Value() );
	}
	if( rv == -1 ) {
		dprintf( D_ALWAYS, "Error: couldn't get proxy\n");
		sock->end_of_message();
		reply( sock, NOT_OK );
		return FALSE;
	}
	if ( !sock->end_of_message() ) {
		dprintf( D_ALWAYS, "end of message error (2)\n" );
		reply( sock, NOT_OK );
		return FALSE;
	}
	
	// that's it - return success
	reply( sock, OK );

	// if the claim already has an associated proxy, delete it
	// before replacing it with the one we just got
	char* old_proxy = claim->client()->proxyFile();
	if (old_proxy != NULL) {
		if (unlink(old_proxy) == -1) {
			dprintf(D_ALWAYS,
			        "error deleting old proxy %s before updating: %s (%d)\n",
			        old_proxy,
			        strerror(errno),
			        errno);
		}
	}

	// we have the proxy - now stash its location in the Claim's
	// Client object so we can get at it when we launch the
	// starter
	claim->client()->setProxyFile( proxy_file.Value() );

	return TRUE;
}
#endif

//////////////////////////////////////////////////////////////////////
// Protocol helper functions
//////////////////////////////////////////////////////////////////////

#define ABORT \
delete req_classad;						\
if (client_addr) free(client_addr);		\
return_code = abort_claim(rip);		\
if( new_dynamic_slot ) rip->change_state( delete_state );	\
return return_code;

int
abort_claim( Resource* rip )
{
	switch( rip->state() ) {
	case claimed_state:
		if (rip->r_pre_pre) {
			rip->removeClaim( rip->r_pre_pre );
		}
		else {
			rip->removeClaim( rip->r_pre );
		}
		break;

	case owner_state:
			// no state change or other logic required
		break;

#if HAVE_BACKFILL
	case backfill_state:
			// For clarity, print this before changing the destination state.
		rip->dprintf( D_ALWAYS, "Claiming protocol failed\n" );
		if (rip->destination_state() == matched_state ) { 
			/*
			  We got the match notification, so we've already started
			  evicting the backfill job.  In this case, if we have to
			  abort the claim, the best we can do is change the
			  destination so we go back to owner instead of matched.
			  We still have to wait for the starter to exit. -Derek 
			*/
			rip->set_destination_state( owner_state );
		} else {
			/*
			  Otherwise, we were running normally when the claim
			  request came in we can just let it happily continue.  In
			  this case, all we've got to do here is delete our
			  current claim object and generate a new one. -Derek
			*/
			rip->removeClaim( rip->r_cur );
		}
		return FALSE;
		break;
#endif /* HAVE_BACKFILL */

	default:
		rip->dprintf( D_ALWAYS, "State change: claiming protocol failed\n" );
		rip->change_state( owner_state );
		return FALSE;
		break;
	}
	rip->dprintf( D_ALWAYS, "Claiming protocol failed\n" );
	return FALSE;
}


int
request_claim( Resource* rip, Claim *claim, char* id, Stream* stream )
{
		// Formerly known as "reqservice"

	ClassAd	*req_classad = new ClassAd, *mach_classad = rip->r_classad;
	int cmd, mach_requirements = 1;
	float rank = 0;
	float oldrank = 0;
	char *client_addr = NULL;
	int interval;
	ClaimIdParser idp(id);

		// Used in ABORT macro, yuck
	bool new_dynamic_slot = false;
	int return_code;

	if( !rip->r_cur ) {
		EXCEPT( "request_claim: no current claim object." );
	}

		/* 
		   Now that we've been contacted by the schedd, we can
		   cancel the match timer on either the current or the
		   preempting claim, depending on what state we're in.  We
		   want to do this right away so we don't abort this function
		   for some reason and leave that timer in place, since we'll
		   EXCEPT later on if it goes off and we're not in the matched
		   state anymore (and it's the current claim at that point).
		   -Derek Wright 3/11/99 
		*/
	claim->cancel_match_timer();

		// Get the classad of the request.
	if( !req_classad->initFromStream(*stream) ) {
		rip->dprintf( D_ALWAYS, "Can't receive classad from schedd\n" );
		ABORT;
	}

		// Try now to read the schedd addr and aline interval.
		// Do _not_ abort if we fail, since older (pre v6.1.11) schedds do 
		// not send this information until after we accept the request, and we
		// want to make an attempt to be backwards-compatibile.
	if ( stream->code(client_addr) ) {
		// We got the schedd addr, we must be talking to a post 6.1.11 schedd
		rip->dprintf( D_FULLDEBUG, "Schedd addr = %s\n", client_addr );
		if( !stream->code(interval) ) {
			rip->dprintf( D_ALWAYS, "Can't receive alive interval\n" );
			ABORT;
		} else {
			rip->dprintf( D_FULLDEBUG, "Alive interval = %d\n", interval );
		}
			// Now, store them into r_cur or r_pre, as appropiate
		claim->setaliveint( interval );
		claim->client()->setaddr( client_addr );
		free( client_addr );
		client_addr = NULL;
	} else {
		rip->dprintf(D_FULLDEBUG, "Schedd using pre-v6.1.11 claim protocol\n");
	}

	if( !stream->end_of_message() ) {
		rip->dprintf( D_ALWAYS, "Can't receive eom from schedd\n" );
		ABORT;
	}

		// At this point, the schedd has registered this socket (stream)
		// and likely has gone off to service other requests.  Thus, 
		// we need change the timeout on the socket to the schedd to be
		// very patient.  By default, daemon core put a timeout of ~20
		// seconds on this socket.  This is not at all long enough.
		// since we should only hold this claim for match_timeout,
		// set the timeout to be 10 seconds less than that. -Todd
	if ( match_timeout > 10 ) {
		stream->timeout( match_timeout - 10 );
	}
		
	rip->dprintf( D_FULLDEBUG,
				  "Received ClaimId from schedd (%s)\n", idp.publicClaimId() );

	if( Resource::PARTITIONABLE_SLOT == rip->get_feature() ) {
		Resource *new_rip;
		CpuAttributes *cpu_attrs;
		MyString type;
		StringList type_list;
		int cpus, memory, disk;

			// Make sure the partitionable slot itself is satisfied by
			// the job. If not there is no point in trying to
			// partition it. This check also prevents
			// over-partitioning. The acceptability of the dynamic
			// slot and job will be checked later, in the normal
			// course of accepting the claim.
		rip->r_reqexp->restore();
		if( mach_classad->EvalBool( ATTR_REQUIREMENTS, 
									req_classad, mach_requirements ) == 0 ) {
			rip->dprintf( D_FAILURE|D_ALWAYS, 
						  "Machine requirements not satisfied.\n" );
			refuse( stream );
			ABORT;
		}

		if( !req_classad->EvalInteger( ATTR_REQUEST_CPUS, mach_classad, cpus ) ) {
			cpus = 1; // reasonable default, for sure
		}
		type.sprintf_cat( "cpus=%d ", cpus );

		if( req_classad->EvalInteger( ATTR_REQUEST_MEMORY, mach_classad, memory ) ) {
			type.sprintf_cat( "memory=%d ", memory );
		} else {
				// some memory size must be available else we cannot
				// match, plus a job ad without ATTR_MEMORY is sketchy
			rip->dprintf( D_ALWAYS,
						  "No memory request in incoming ad, aborting...\n" );
			ABORT;
		}

		if( req_classad->EvalInteger( ATTR_REQUEST_DISK, mach_classad, disk ) ) {
				// XXX: HPUX does not appear to have ceilf, and
				// Solaris doesn't make it readily available
			type.sprintf_cat( "disk=%d%%",
							  max((int) ceil((disk / (double) rip->r_attr->get_total_disk()) * 100), 1) );
		} else {
				// some disk size must be available else we cannot
				// match, plus a job ad without ATTR_DISK is sketchy
			rip->dprintf( D_FULLDEBUG,
						  "No disk request in incoming ad, aborting...\n" );
			ABORT;
		}

		rip->dprintf( D_FULLDEBUG,
					  "Match requesting resources: %s\n", type.Value() );

		type_list.initializeFromString( type.Value() );
		cpu_attrs = resmgr->buildSlot( rip->r_id, &type_list, -1, false );
		if( ! cpu_attrs ) {
			rip->dprintf( D_ALWAYS,
						  "Failed to parse attributes for request, aborting\n" );
			ABORT;
		}

		new_rip = new Resource( cpu_attrs, rip->r_id, rip );
		if( ! new_rip ) {
			rip->dprintf( D_ALWAYS,
						  "Failed to build new resource for request, aborting\n" );
			ABORT;
		}

			// Initialize the rest of the Resource
		new_rip->compute( A_ALL );
		new_rip->compute( A_TIMEOUT | A_UPDATE ); // Compute disk space
		new_rip->init_classad();
		new_rip->refresh_classad( A_EVALUATED ); 
		new_rip->refresh_classad( A_SHARED_SLOT ); 

			// The new resource needs the claim from its
			// parititionable parent
		delete new_rip->r_cur;
		new_rip->r_cur = rip->r_cur;
		new_rip->r_cur->setResource( new_rip );

			// And the partitionable parent needs a new claim
		rip->r_cur = new Claim( rip );

			// Recompute the partitionable slot's resources
		rip->change_state( unclaimed_state );
		rip->update(); // in case we were never matched, i.e. no state change

		resmgr->addResource( new_rip );

			// XXX: This is overkill, but the best way, right now, to
			// get many of the new_rip's attributes calculated.
		resmgr->compute( A_ALL );
		resmgr->compute( A_TIMEOUT | A_UPDATE );

			// Now we continue on with the newly spawned Resource
			// getting claimed
		rip = new_rip;

			// This is, unfortunately, part of the ABORT macro. The
			// idea is that if we are aborting a claim at the same
			// time that we are creating a dynamic slot for that
			// claim, we should already remove the dynamic slot. We
			// don't want to do this everytime an ABORT happens on a
			// dynamic slot because it may be useful to other jobs.
		new_dynamic_slot = true;
	}

		// Make sure we're willing to run this job at all.
	if (!rip->willingToRun(req_classad)) {
	    rip->dprintf(D_ALWAYS, "Request to claim resource refused.\n");
		refuse(stream);
		ABORT;
	}

		// Now, make sure it's got a high enough rank to preempt us.
	rank = compute_rank(rip->r_classad, req_classad);
	rip->dprintf( D_FULLDEBUG, "Rank of this claim is: %f\n", rank );

	if( rip->state() == claimed_state ) {
			// We're currently claimed.  We might want to preempt the
			// current claim to make way for this new one...
		if( !rip->r_pre ) {
			rip->dprintf( D_ALWAYS, 
			   "In CLAIMED state without preempting claim object, aborting.\n" );
			refuse( stream );
			ABORT;
		}
		if( rip->r_pre_pre ) {
			if(!rip->r_pre_pre->idMatches(id)) {
				rip->dprintf( D_ALWAYS,
							  "ClaimId from schedd (%s) doesn't match (%s)\n",
							  idp.publicClaimId(), rip->r_pre_pre->publicClaimId() );
				refuse( stream );
				ABORT;
			}
			rip->dprintf(
					 D_ALWAYS,
					"Preempting preempting claim has correct ClaimId.\n");
			if( rank < rip->r_pre->rank() ) {
				rip->dprintf( D_ALWAYS, 
							  "Preempting claim doesn't have sufficient "
							  "rank to replace existing preempting claim; "
							  "refusing.\n" );
				refuse( stream );
				ABORT;
			}

			if( rank > rip->r_pre->rank() ) {
				rip->dprintf(D_ALWAYS,
							 "Replacing existing preempting claim with "
							 "new preempting claim based on machine rank.\n");
			}
			else {
				rip->dprintf(D_ALWAYS,
							 "Replacing existing preempting claim with "
							 "new preempting claim based on user priority.\n");
			}

			Claim *pre_pre = rip->r_pre_pre;
			rip->r_pre_pre = NULL;
			rip->removeClaim(rip->r_pre); // cancel existing preempting claim
			rip->r_pre = pre_pre;         // replace with new one
		}

		if( rip->r_pre->idMatches(id) ) {
			rip->dprintf( D_ALWAYS, 
						  "Preempting claim has correct ClaimId.\n" );
			
				// Check rank, decided to preempt, if needed, do so.
			if( rank < rip->r_cur->rank() ) {
				rip->dprintf( D_ALWAYS, 
				  "Preempting claim doesn't have sufficient rank, refusing.\n" );
				cmd = NOT_OK;
			} else {
				rip->dprintf( D_ALWAYS, 
				  "New claim has sufficient rank, preempting current claim.\n" );

					// We're going to preempt.  Save everything we
					// need to know into r_pre.
				rip->r_pre->setRequestStream( stream );
				rip->r_pre->setad( req_classad );
				rip->r_pre->loadAccountingInfo();
				rip->r_pre->setrank( rank );
				rip->r_pre->setoldrank( rip->r_cur->rank() );
				rip->r_pre->loadRequestInfo();

					// Create a new claim id for preempting this preempting
					// claim (in case of long retirement).
				rip->r_pre_pre = new Claim( rip );

					// Get rid of the current claim.
				if( rank > rip->r_cur->rank() ) {
					rip->dprintf( D_ALWAYS, 
					 "State change: preempting claim based on machine rank\n" );
				} else {
					ASSERT( rank == rip->r_cur->rank() );
					rip->dprintf( D_ALWAYS, 
					 "State change: preempting claim based on user priority\n" );
				}

				    // Force resource to take note of the preempting claim.
				    // This results in a reversible transition to the
				    // retiring activity.  If the preempting claim goes
				    // away before the current claim retires, the current
				    // claim can unretire and continue without any disturbance.
				rip->eval_state();

					// Tell daemon core not to do anything to the stream.
				return KEEP_STREAM;
			}					
		} else {
			rip->dprintf( D_ALWAYS,
					 "ClaimId from schedd (%s) doesn't match (%s)\n",
					 idp.publicClaimId(), rip->r_pre->publicClaimId() );
			cmd = NOT_OK;
		}
	} else {
			// We're not claimed
		if( rip->r_cur->idMatches(id) ) {
			rip->dprintf( D_ALWAYS, "Request accepted.\n" );
			cmd = OK;
		} else {
			rip->dprintf( D_ALWAYS,
					"ClaimId from schedd (%s) doesn't match (%s)\n",
					idp.publicClaimId(), rip->r_cur->publicClaimId() );
			cmd = NOT_OK;
		}		
	}	

	if( cmd != OK ) {
		refuse( stream );
		ABORT;
	}

		// We decided to accept the request, save the schedd's
		// stream, the rank and the classad of this request.
	rip->r_cur->setRequestStream( stream );
	rip->r_cur->setad( req_classad );
	rip->r_cur->setrank( rank );
	rip->r_cur->setoldrank( oldrank );

#if HAVE_BACKFILL
	if( rip->state() == backfill_state ) {
			// we're currently in Backfill, so we can't just accept
			// the request immediately, we have to first kill our
			// backfill job on this resource.  so, we'll set the
			// destination state to claimed, and allow that to finish
			// the claiming protocol once the backfill is gone...
		rip->set_destination_state( claimed_state );
		return KEEP_STREAM;
	}
#endif /* HAVE_BACKFILL */

		// If we're still here, we're ready to accpet the claim now.
		// Call this other function to actually reply to the schedd
		// and perform the last half of the protocol.  We use the same
		// function after the preemption has completed when the startd
		// is finally ready to reply to the and finish the claiming
		// process.
	accept_request_claim( rip );

		// We always need to return KEEP_STREAM so that daemon core
		// doesn't try to delete the stream we've already deleted.
	return KEEP_STREAM;

}
#undef ABORT


void
abort_accept_claim( Resource* rip, Stream* stream )
{
	stream->encode();
	stream->end_of_message();
	rip->r_cur->setRequestStream( NULL );
#if HAVE_BACKFILL
	if (rip->state() == backfill_state &&
		rip->destination_state() != owner_state)
	{
			/*
			  If we're still in backfill when this happens, we can't
			  go directly to owner state.  We've got to just set our
			  destination and then wait for the starter to exit before
			  we actually change states...
			*/
		rip->dprintf( D_ALWAYS, "Claiming protocol failed\n" );
		rip->set_destination_state( owner_state );
		return;
	}
#endif /* HAVE_BACKFILL */

	rip->dprintf( D_ALWAYS, "State change: claiming protocol failed\n" );
	rip->change_state( owner_state );
	return;
}


bool
accept_request_claim( Resource* rip )
{
	int interval = -1;
	char *client_addr = NULL, *client_host, *full_client_host, *tmp;
	char RemoteOwner[512];
	RemoteOwner[0] = '\0';

		// There should not be a pre claim object now.
	ASSERT( rip->r_pre == NULL );

	Stream* stream = rip->r_cur->requestStream();
	ASSERT( stream );
	Sock* sock = (Sock*)stream;

	stream->encode();
	if( !stream->put( OK ) ) {
		rip->dprintf( D_ALWAYS, "Can't to send cmd to schedd.\n" );
		abort_accept_claim( rip, stream );
		return false;
	}
	if( !stream->eom() ) {
		rip->dprintf( D_ALWAYS, "Can't to send eom to schedd.\n" );
		abort_accept_claim( rip, stream );
		return false;
	}

		// Grab the schedd addr and alive interval if the alive interval is still
		// unitialized (-1) which means we are talking to an old (pre v6.1.11) schedd.
		// Normally, we want to get this information from the schedd when the claim request
		// first arrives.  This prevents a deadlock in the claiming protocol between the
		// schedd and startd when the startd is running on an SMP.  -Todd 1/2000
	if ( rip->r_cur->getaliveint() == -1 ) {
		stream->decode();
		if( ! stream->code(client_addr) ) {
			rip->dprintf( D_ALWAYS, "Can't receive schedd addr.\n" );
			abort_accept_claim( rip, stream );
			return false;
		} else {
			rip->dprintf( D_FULLDEBUG, "Schedd addr = %s\n", client_addr );
		}
		if( !stream->code(interval) ) {
			rip->dprintf( D_ALWAYS, "Can't receive alive interval\n" );
			free( client_addr );
			client_addr = NULL;
			abort_accept_claim( rip, stream );
			return false;
		} else {
			rip->dprintf( D_FULLDEBUG, "Alive interval = %d\n", interval );
		}
		stream->end_of_message();

			// Now, store them into r_cur
		rip->r_cur->setaliveint( interval );
		rip->r_cur->client()->setaddr( client_addr );
		free( client_addr );
		client_addr = NULL;
	}

		// Figure out the hostname of our client.
	if( ! (tmp = sin_to_hostname(sock->peer_addr(), NULL)) ) {
		char *sinful = sin_to_string(sock->peer_addr());
		char *ip = string_to_ipstr(sinful);
		rip->dprintf( D_FULLDEBUG,
					  "Can't find hostname of client machine %s\n", ip );
		rip->r_cur->client()->sethost( ip );
	} else {
		client_host = strdup( tmp );
			// Try to make sure we've got a fully-qualified hostname.
		full_client_host = get_full_hostname( (const char *) client_host );
		if( ! full_client_host ) {
			rip->dprintf( D_ALWAYS, "Error finding full hostname of %s\n", 
						  client_host );
			rip->r_cur->client()->sethost( client_host );
		} else {
			rip->r_cur->client()->sethost( full_client_host );
			delete [] full_client_host;
		}
		free( client_host );
	}

		// Get the owner of this claim out of the request classad.
	if( (rip->r_cur->ad())->
		EvalString( ATTR_USER, rip->r_cur->ad(), RemoteOwner ) == 0 ) { 
		rip->dprintf( D_ALWAYS, 
				 "Can't evaluate attribute %s in request ad.\n", 
				 ATTR_USER );
		RemoteOwner[0] = '\0';
	}
	if( '\0' != RemoteOwner[0] ) {
		rip->r_cur->client()->setowner( RemoteOwner );
			// For now, we say the remote user is the same as the
			// remote owner.  In the future, we might decide to leave
			// RemoteUser undefined until the resource is busy...
		rip->r_cur->client()->setuser( RemoteOwner );
		rip->dprintf( D_ALWAYS, "Remote owner is %s\n", RemoteOwner );
	} else {
		rip->dprintf( D_ALWAYS, "Remote owner is NULL\n" );
			// TODO: What else should we do here???
	}		
		// Also look for ATTR_ACCOUNTING_GROUP and stash that
	char* acct_grp = NULL;
	rip->r_cur->ad()->LookupString( ATTR_ACCOUNTING_GROUP, &acct_grp );
	if( acct_grp ) {
		rip->r_cur->client()->setAccountingGroup( acct_grp );
		free( acct_grp );
		acct_grp = NULL;
	}

	rip->r_cur->loadRequestInfo();

		// Since we're done talking to this schedd, delete the stream.
	rip->r_cur->setRequestStream( NULL );

	rip->dprintf( D_FAILURE|D_ALWAYS, "State change: claiming protocol successful\n" );
	rip->change_state( claimed_state );
	return true;
}



#define ABORT \
if( req_classad ) 	 					\
	delete( req_classad );				\
free( shadow_addr );					\
return FALSE

int
activate_claim( Resource* rip, Stream* stream ) 
{
		// Formerly known as "startjob"
	int mach_requirements = 1;
	ClassAd	*req_classad = NULL, *mach_classad = rip->r_classad;
	ReliSock rsock_1, rsock_2;
#ifndef WIN32
	int sock_1, sock_2;
	int fd_1, fd_2;
	struct sockaddr_in frm;
	int len = sizeof frm;	
	StartdRec stRec;
#endif
	int starter = MAX_STARTERS;
	Sock* sock = (Sock*)stream;
	char* shadow_addr = strdup( sin_to_string( sock->peer_addr() ));

	if( rip->state() != claimed_state ) {
		rip->dprintf( D_ALWAYS, "Not in claimed state, aborting.\n" );
		refuse( stream );
		ABORT;
	}

	rip->dprintf( D_ALWAYS,
			 "Got activate_claim request from shadow (%s)\n", 
			 shadow_addr );

		// Find out what version of the starter to use for the activation.
	if( ! stream->code( starter ) ) {
		rip->dprintf( D_ALWAYS, "Can't read starter type from %s\n",
				 shadow_addr );
		refuse( stream );
		ABORT;
	}
	if( starter >= MAX_STARTERS ) {
	    rip->dprintf( D_ALWAYS, "Requested starter is out of range.\n" );
		refuse( stream );
	    ABORT;
	}

		// Grab request class ad 
	req_classad = new ClassAd;
	if( !req_classad->initFromStream(*stream) ) {
		rip->dprintf( D_ALWAYS, "Can't receive request classad from shadow.\n" );
		ABORT;
	}
	if (!stream->end_of_message()) {
		rip->dprintf( D_ALWAYS, "Can't receive eom() from shadow.\n" );
		ABORT;
	}

	rip->dprintf( D_FULLDEBUG, "Read request ad and starter from shadow.\n" );

		// Now, ask the ResMgr to recompute so we have totally
		// up-to-date values for everything in our classad.
		// Unfortunately, this happens to all the resources in an SMP
		// at once, but that's the only way to compute anything... 
	resmgr->compute( A_TIMEOUT | A_UPDATE );

		// Possibly print out the ads we just got to the logs.
	rip->dprintf( D_JOB, "REQ_CLASSAD:\n" );
	if( DebugFlags & D_JOB ) {
		req_classad->dPrint( D_JOB );
	}
	  
	rip->dprintf( D_MACHINE, "MACHINE_CLASSAD:\n" );
	if( DebugFlags & D_MACHINE ) {
		mach_classad->dPrint( D_MACHINE );
	}

		// See if machine and job meet each other's requirements, if
		// so start the job and tell shadow, otherwise refuse and
		// clean up.  
	rip->r_reqexp->restore();
	if( mach_classad->EvalBool( ATTR_REQUIREMENTS, 
								req_classad, mach_requirements ) == 0 ) {
		mach_requirements = 0;
	}
	if( !mach_requirements ) {
		rip->dprintf( D_ALWAYS, "Machine Requirements check failed!\n" );
		refuse( stream );
	    ABORT;
	}

	int job_univ = 0;
	if( req_classad->LookupInteger(ATTR_JOB_UNIVERSE, job_univ) != 1 ) {
		rip->dprintf(D_ALWAYS, "Can't find Job Universe in Job ClassAd\n");
		refuse(stream);
		ABORT;
	}

	ClassAd vm_classad = *req_classad;
	if( job_univ == CONDOR_UNIVERSE_VM ) {
		if( resmgr->m_vmuniverse_mgr.canCreateVM(&vm_classad) == false ) {
			// Not enough memory or reaches to max number of VMs
			rip->dprintf( D_ALWAYS, "Cannot execute a VM universe job "
					"due to insufficient resource\n");
			refuse(stream);
			ABORT;
		}
	}

		// now, try to satisfy the job.  while we're at it, we'll
		// figure out what starter they want to use
	Starter* tmp_starter;
	tmp_starter = resmgr->starter_mgr.findStarter( req_classad,
												   mach_classad,
												   starter );
	if( ! tmp_starter ) {
		rip->dprintf( D_ALWAYS, "Job Requirements check failed!\n" );
		refuse( stream );
	    ABORT;
	}

		// If we're here, we've decided to activate the claim.  Tell
		// the shadow we're ok.
	stream->encode();
	if( !stream->put( OK ) ) {
		rip->dprintf( D_ALWAYS, "Can't send OK to shadow.\n" );
		ABORT;
	}
	if( !stream->end_of_message() ) {
		rip->dprintf( D_ALWAYS, "Can't send eom to shadow.\n" );
		ABORT;
	}

		// if we're daemonCore, hold onto the sock the shadow used for
		// this command, and we'll use that for the shadow RSC sock.
		// otherwise, if we're not windoze, setup our two ports, tell
		// the shadow about them, and wait for it to connect.
	Stream* shadow_sock = NULL;
	if( tmp_starter->is_dc() ) {
		shadow_sock = stream;
	} 
#ifndef WIN32
	else {

			// Set up the two starter ports and send them to the shadow
		stRec.version_num = VERSION_FOR_FLOCK;
		sock_1 = create_port(&rsock_1);
		sock_2 = create_port(&rsock_2);
		stRec.ports.port1 = rsock_1.get_port();
		stRec.ports.port2 = rsock_2.get_port();

		stRec.server_name = strdup( my_full_hostname() );
	
			// Send our local IP address, too.
		memcpy( &stRec.ip_addr, my_sin_addr(), sizeof(struct in_addr) );
		
		stream->encode();
		if (!stream->code(stRec)) {
			ABORT;
		}

		if (!stream->eom()) {
			ABORT;
		}

			/* now that we sent stRec, free stRec.server_name which we strduped */
		free( stRec.server_name );

			/* Wait for connection to port1 */
		len = sizeof frm;
		memset( (char *)&frm,0, sizeof frm );
		while( (fd_1=tcp_accept_timeout(sock_1, (struct sockaddr *)&frm,
										&len, 150)) < 0 ) {
			if( fd_1 != -3 ) {  /* tcp_accept_timeout returns -3 on EINTR */
				if( fd_1 == -2 ) {
					rip->dprintf( D_ALWAYS, "accept timed out\n" );
					ABORT;
				} else {
					rip->dprintf( D_ALWAYS, 
								  "tcp_accept_timeout returns %d, errno=%d\n",
								  fd_1, errno );
					ABORT;
				}
			}
		}
	
			/* Wait for connection to port2 */
		len = sizeof frm;
		memset( (char *)&frm,0,sizeof frm );
		while( (fd_2 = tcp_accept_timeout(sock_2, (struct sockaddr *)&frm,
										  &len, 150)) < 0 ) {
			if( fd_2 != -3 ) {  /* tcp_accept_timeout returns -3 on EINTR */
				if( fd_2 == -2 ) {
					rip->dprintf( D_ALWAYS, "accept timed out\n" );
					ABORT;
				} else {
					rip->dprintf( D_ALWAYS, 
								  "tcp_accept_timeout returns %d, errno=%d\n",
								  fd_2, errno );
					ABORT;
				}
			}
		}
		tmp_starter->setPorts( fd_1, fd_2 );
	}
#endif	// of ifdef WIN32

		// now that we've gotten this far, we're really going to try
		// to spawn the starter.  set it in our Claim object.  Once
		// it's there, we no longer control this memory so we should
		// clear out our pointer to avoid confusion/problems.
	rip->r_cur->setStarter( tmp_starter );
	// this variable will be used to know the IP of Starter later.
	Starter* vm_starter = tmp_starter;
	tmp_starter = NULL;

		// update the current rank on this claim
	float rank = compute_rank( mach_classad, req_classad ); 
	rip->r_cur->setrank( rank );

		// Grab the job ID, so we've got it.  Once we give the
		// req_classad to the Claim object, we no longer control it. 
	rip->r_cur->saveJobInfo( req_classad );
	req_classad = NULL;

		// Actually spawn the starter
	if( ! rip->r_cur->spawnStarter(shadow_sock) ) {
			// if Claim::spawnStarter fails, it resets the Claim
			// object to clear out all the info we just stashed above
			// with setStarter() and saveJobInfo().  it's safe to just
			// abort now, and all the state will be happy.
		ABORT;
	}

	if( job_univ == CONDOR_UNIVERSE_VM ) {
		if( resmgr->m_vmuniverse_mgr.allocVM(vm_starter->pid(), vm_classad, rip->executeDir()) 
				== false ) {
			ABORT;
		}
		vm_starter = NULL;

		// update VM related info
		resmgr->walk( &Resource::update);
	}

		// Finally, update all these things into the resource classad.
	rip->r_cur->publish( rip->r_classad, A_PUBLIC );

	rip->dprintf( D_FAILURE|D_ALWAYS, 
				  "State change: claim-activation protocol successful\n" );
	rip->change_state( busy_act );

	free( shadow_addr );
	return TRUE;
}
#undef ABORT

int
match_info( Resource* rip, char* id )
{
	int rval = FALSE;
	ClaimIdParser idp(id);

	rip->dprintf(D_ALWAYS, "match_info called\n");

	switch( rip->state() ) {
	case claimed_state:
		if( rip->r_cur->idMatches(id) ) {
				// The ClaimId we got matches the one for the
				// current claim, and we're already claimed.  There's
				// nothing to do here.
			rval = TRUE;
		} else if( rip->r_pre && rip->r_pre->idMatches(id) ) {
				// The ClaimId we got matches the preempting
				// ClaimId we've been advertising.  Advertise
				// ourself as unavailable for future claims, update
				// the CM, and set the timer for this match.
			rip->r_reqexp->unavail();
			rip->update();
			rip->r_pre->start_match_timer();
			rval = TRUE;
		} else if( rip->r_pre_pre && rip->r_pre_pre->idMatches(id) ) {
				// The ClaimId we got matches the preempting preempting
				// ClaimId we've been advertising.  Advertise
				// ourself as unavailable for future claims, update
				// the CM, and set the timer for this match.
			rip->r_reqexp->unavail();
			rip->update();
			rip->r_pre_pre->start_match_timer();
			rval = TRUE;
		} else {
				// The ClaimId doesn't match any of our ClaimIds.
				// Should never get here, since we found the rip by
				// some id in the first place.
			EXCEPT( "Should never get here" );
		}
		break;

#if HAVE_BACKFILL
	case backfill_state:
#endif /* HAVE_BACKFILL */
	case unclaimed_state:
	case owner_state:
		if( rip->r_cur->idMatches(id) ) {
			rip->dprintf( D_ALWAYS, "Received match %s\n", idp.publicClaimId() );

			if( rip->destination_state() != no_state ) {
					// we've already got a destination state.
					// therefore, we don't want to act on this match
					// notification, we want to allow our destination
					// logic to run its course and just return.
				dprintf( D_ALWAYS, "Got match while destination state "
						 "set to %s, ignoring\n",
						 state_to_string(rip->destination_state()) );
				return TRUE;
			}

				// Start a timer to prevent staying in matched state
				// too long. 
			rip->r_cur->start_match_timer();

			rip->dprintf( D_FAILURE|D_ALWAYS, "State change: "
						  "match notification protocol successful\n" );

#if HAVE_BACKFILL
			if( rip->state() == backfill_state ) {
					// if we're currently in backfill, we can't go
					// immediately to matched, since we have to kill
					// our existing backfill client/starter, first.
				rip->set_destination_state( matched_state );
				return TRUE;
			}
#endif /* HAVE_BACKFILL */

				// Entering matched state sets our reqexp to unavail
				// and updates CM.
			rip->change_state( matched_state );
			rval = TRUE;
		} else {
			rip->dprintf( D_ALWAYS, 
						  "Invalid ClaimId from negotiator (%s)\n", id );
			rval = FALSE;
		}
		break;
	default:
		EXCEPT( "match_info() called with unexpected state (%s)", 
				state_to_string(rip->state()) );
		break;
	}
	return rval;
}


int
caRequestCODClaim( Stream *s, char* cmd_str, ClassAd* req_ad )
{
	const char* requirements_str = NULL;
	Resource* rip;
	Claim* claim;
	MyString err_msg;
	ExprTree *tree, *rhs;
	ReliSock* rsock = (ReliSock*)s;
	int lease_duration = 0;
	const char* owner = rsock->getOwner();

	if( ! authorizedForCOD(owner) ) {
		err_msg = "User '";
		err_msg += owner;
		err_msg += "' is not authorized for using COD at this machine"; 
		return sendErrorReply( s, cmd_str, CA_NOT_AUTHORIZED,
							   err_msg.Value() );
	}
	dprintf( D_COMMAND, 
			 "Serving request for a new COD claim by user '%s'\n", 
			 owner );

		// Make sure the ad's got a requirements expression at all.
	tree = req_ad->LookupExpr( ATTR_REQUIREMENTS );
	if( ! tree ) {
		dprintf( D_FULLDEBUG, 
				 "Request did not contain %s, assuming TRUE\n",
				 ATTR_REQUIREMENTS );
		requirements_str = "TRUE";
		req_ad->Insert( "Requirements = TRUE" );
	} else {
		requirements_str = ExprTreeToString( tree );
	}

		// Find the right resource for this claim
	rip = resmgr->findRipForNewCOD( req_ad );

	if( ! rip ) {
		err_msg = "Can't find Resource matching ";
		err_msg += ATTR_REQUIREMENTS;
		err_msg += " (";
		err_msg += requirements_str;
		err_msg += ')';
		return sendErrorReply( s, cmd_str, CA_FAILURE, err_msg.Value() );
	}

	if( !req_ad->LookupInteger(ATTR_JOB_LEASE_DURATION, lease_duration) ) {
		lease_duration = -1;
	}
		// try to create the new claim
	claim = rip->newCODClaim( lease_duration );
	if( ! claim ) {
		return sendErrorReply( s, cmd_str, CA_FAILURE, 
							   "Can't create new COD claim" );
	}

		// Stash some info about who made this request in the Claim  
	claim->client()->setuser( owner );
	claim->client()->setowner(owner );
	claim->client()->sethost( rsock->peer_ip_str() );

		// now, we just fill in the reply ad appropriately.  publish
		// a complete resource ad (like what we'd send to the
		// collector), and include the ClaimID  
	ClassAd reply;
	rip->publish( &reply, A_ALL_PUB );

	MyString line;
	line = ATTR_CLAIM_ID;
	line += " = \"";
	line += claim->id();
	line += '"';
	reply.Insert( line.Value() );
	
	line = ATTR_RESULT;
	line += " = \"";
	line += getCAResultString( CA_SUCCESS );
	line += '"';
	reply.Insert( line.Value() );

	int rval = sendCAReply( s, cmd_str, &reply );
	if( ! rval ) {
		dprintf( D_ALWAYS, 
				 "Failed to reply to request, removing new claim\n" );
		rip->r_cod_mgr->removeClaim( claim );
			// removeClaim() deletes the object, so don't have a
			// dangling pointer...
		claim = NULL;
	}
	return rval;
}


int
caRequestClaim( Stream *s, char* cmd_str, ClassAd* req_ad )
{
	ClaimType claim_type;
	char* ct_str = NULL;
	MyString err_msg; 

		// Now, depending on what kind of claim they're asking for, do
		// the right thing
	if( ! req_ad->LookupString(ATTR_CLAIM_TYPE, &ct_str) ) {
		err_msg = "No ";
		err_msg += ATTR_CLAIM_TYPE;
		err_msg += " in ClassAd";
		return sendErrorReply( s, cmd_str, CA_INVALID_REQUEST, 
							   err_msg.Value() );
	}
	claim_type = getClaimTypeNum( ct_str );
	free( ct_str ); 
	switch( claim_type ) {
	case CLAIM_COD:
		return caRequestCODClaim( s, cmd_str, req_ad );
		break;
	case CLAIM_OPPORTUNISTIC:
		err_msg = ATTR_CLAIM_TYPE;
		err_msg += " (";
		err_msg += getClaimTypeString( claim_type );
		err_msg += ") not supported by this startd";
		return sendErrorReply( s, cmd_str, CA_INVALID_REQUEST,
							   err_msg.Value() );
		break;
	default:
		err_msg = "Unrecognized ";
		err_msg += ATTR_CLAIM_TYPE;
		err_msg += " (";
		err_msg += getClaimTypeString( claim_type );
		err_msg += ") in request ClassAd";
		return sendErrorReply( s, cmd_str, CA_INVALID_REQUEST,
							   err_msg.Value() );
		break;
	}
	return FALSE;
}


int
caLocateStarter( Stream *s, char* cmd_str, ClassAd* req_ad )
{
	char* global_job_id = NULL;
	char* claimid = NULL;
	char* schedd_addr = NULL;
	Claim* claim = NULL;
	int rval = TRUE;
	ClassAd reply;

	req_ad->LookupString(ATTR_CLAIM_ID, &claimid);
	req_ad->LookupString(ATTR_GLOBAL_JOB_ID, &global_job_id);
	req_ad->LookupString(ATTR_SCHEDD_IP_ADDR, &schedd_addr);
	claim = resmgr->getClaimByGlobalJobIdAndId(global_job_id, claimid);

	if( ! claim ) {
		MyString err_msg = ATTR_CLAIM_ID;
		err_msg += " (";
		err_msg += claimid;
		err_msg += ") and ";
		err_msg += ATTR_GLOBAL_JOB_ID;
		err_msg += " ( ";
		err_msg += global_job_id;
		err_msg += " ) not found";
		sendErrorReply( s, cmd_str, CA_FAILURE, err_msg.Value() );
		rval = FALSE;
		goto cleanup;
	}

		// if startd is sending keepalives to the schedd,
		// then we _must_ be passed the address of the schedd
		// since it likely changed.
	if ( (!schedd_addr) && 
		 (param_boolean("STARTD_SENDS_ALIVES",false)) )
	{
		MyString err_msg;
		err_msg.sprintf("Required %s, not found in request",
						ATTR_SCHEDD_IP_ADDR);
		sendErrorReply( s, cmd_str, CA_FAILURE, err_msg.Value() );
		rval = FALSE;
		goto cleanup;
	}

	if( ! claim->publishStarterAd(&reply) ) {
		MyString err_msg = "No starter found for ";
		err_msg += ATTR_GLOBAL_JOB_ID;
		err_msg += " (";
		err_msg += global_job_id;
		err_msg += ")";
		sendErrorReply( s, cmd_str, CA_FAILURE, err_msg.Value() );
		rval = FALSE;
		goto cleanup;
	}
	
		// if we are passed an updated schedd addr, stash it
	if ( schedd_addr ) {
		Client *client = claim->client();
		if ( client ) {
			client->setaddr(schedd_addr);
		}
	}

		// if we're still here, everything worked, so we can reply
		// with success...
	reply.Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );

	rval = sendCAReply( s, cmd_str, &reply );
	if( ! rval ) {
		dprintf( D_ALWAYS, "Failed to reply to request\n" );
	}

cleanup:
	if ( global_job_id ) free( global_job_id );
	if ( claimid ) free( claimid );
	if ( schedd_addr ) free( schedd_addr );

	return rval;
}


int
command_classad_handler( Service*, int dc_cmd, Stream* s )
{
	int rval;
	ClassAd ad;
	ReliSock* rsock = (ReliSock*)s;
	int cmd = 0;
	char* cmd_str = NULL;


	if( dc_cmd == CA_AUTH_CMD ) {
		cmd = getCmdFromReliSock( rsock, &ad, true );
	} else {
		cmd = getCmdFromReliSock( rsock, &ad, false );
	}
		// since we really care about the command string for a lot of
		// things, let's just grab it out of the classad once right
		// here.
	if( ! ad.LookupString(ATTR_COMMAND, &cmd_str) ) {
		cmd_str = strdup( "UNKNOWN" );
	}

	switch( cmd ) {
	case CA_LOCATE_STARTER:
			// special case, since it's not really about claims at
			// all, but instead are worrying about trying to find a
			// starter given a global job id.  this is used for
			// shadow/starter reconnect mode, so the shadow can find
			// the right ip/port to reconnect to (since it already has
			// our address in the job classad).
		rval = caLocateStarter( s, cmd_str, &ad );
		free( cmd_str );
		return rval;
		break;

	case CA_REQUEST_CLAIM:
			// this one's a special case, since we're creating a new
			// claim... 
		rval = caRequestClaim( s, cmd_str, &ad );
		free( cmd_str );
		return rval;
		break;

	default:
			// for all the rest, we need to read the ClaimId out of
			// the ad, find the right claim, and call the appropriate
			// method on it.  nothing more to do here...
		break;
	}

	char* claim_id = NULL;
	Claim* claim = NULL;

	if( ! ad.LookupString(ATTR_CLAIM_ID, &claim_id) ) {
		dprintf( D_ALWAYS, "Failed to read %s from ClassAd "
				 "for cmd %s, aborting\n", ATTR_CLAIM_ID, cmd_str );
		free( cmd_str );
		return FALSE;
	}
	claim = resmgr->getClaimById( claim_id );
	if( ! claim ) {
		MyString err_msg = "ClaimID (";
		err_msg += claim_id;
		err_msg += ") not found";
		sendErrorReply( s, cmd_str, CA_FAILURE, err_msg.Value() );
		free( claim_id );
		free( cmd_str );
		return FALSE;
	}

		// make sure the user attempting this action (whatever it
		// might be) is the same as the owner of the claim
	const char* owner = rsock->getOwner();
	if( ! claim->ownerMatches(owner) ) {
		MyString err_msg = "User '";
		err_msg += owner;
		err_msg += "' does not match the owner of this claim";
		sendErrorReply( s, cmd_str, CA_NOT_AUTHORIZED, err_msg.Value() ); 
		free( claim_id );
		free( cmd_str );
		return FALSE;
	}
	dprintf( D_COMMAND, "Serving request for %s by user '%s'\n", 
			 cmd_str, owner );

	char* tmp = NULL;
	ad.LookupString( ATTR_OWNER, &tmp );
	if( tmp ) {
		if( strcmp(tmp, owner) ) {
				// they're different!
			MyString err_msg = ATTR_OWNER;
			err_msg += " specified in ClassAd as '";
			err_msg += tmp;
			err_msg += "' yet request sent by user '";
			err_msg += owner;
			err_msg += "', possible security attack, request refused!";
			sendErrorReply( s, cmd_str, CA_FAILURE, err_msg.Value() );
			free( claim_id );
			free( cmd_str );
			free( tmp );
			return FALSE;
		} 
		free( tmp );
	} else {
			// ATTR_OWNER not defined, set it ourselves...
		MyString line = ATTR_OWNER;
		line += "=\"";
		line += owner;
		line += '"';
		ad.Insert( line.Value() );
	}
 

		// now, find the CODMgr managing this Claim, and call the
		// appropriate method for the given command
	CODMgr* cod_mgr = claim->getCODMgr();
	if( ! cod_mgr ) {
		EXCEPT( "Resource does not have a CODMgr!" );
	}

	switch( cmd ) {
	case CA_RELEASE_CLAIM:
		rval = cod_mgr->release( s, &ad, claim );
		break;
	case CA_ACTIVATE_CLAIM:
		rval = cod_mgr->activate( s, &ad, claim );
		break;
	case CA_DEACTIVATE_CLAIM:
		rval = cod_mgr->deactivate( s, &ad, claim );
		break;
	case CA_SUSPEND_CLAIM:
		rval = cod_mgr->suspend( s, &ad, claim );
		break;
	case CA_RESUME_CLAIM:
		rval = cod_mgr->resume( s, &ad, claim );
		break;
	case CA_REQUEST_CLAIM:
		EXCEPT( "Already handled CA_REQUEST_CLAIM, shouldn't be here\n" );
		break;
	case CA_RENEW_LEASE_FOR_CLAIM:
		rval = cod_mgr->renew( s, &ad, claim );
		break;
	default:
		unknownCmd( s, cmd_str );
		free( claim_id );
		free( cmd_str );
		return FALSE;
	}
	free( claim_id );
	free( cmd_str );
	return rval;
}
