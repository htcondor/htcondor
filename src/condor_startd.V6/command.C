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
#include "startd.h"

/* XXX fix me */
#include "../condor_sysapi/sysapi.h"

extern "C" int tcp_accept_timeout( int, struct sockaddr*, int*, int );

int
command_handler( Service*, int cmd, Stream* stream )
{
	int rval = FALSE;
	Resource* rip;
	if( ! (rip = stream_to_rip(stream)) ) {
		return FALSE;
	}
	State s = rip->state();

		// RELEASE_CLAIM only makes sense in two states
	if( cmd == RELEASE_CLAIM ) {
		if( (s == claimed_state) || (s == matched_state) ) {
			rip->dprintf( D_ALWAYS, 
						  "State change: received RELEASE_CLAIM command\n" );
			return rip->release_claim();
		} else {
			rip->log_ignore( cmd, s );
			return FALSE;
		}
	}

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
		rval = rip->deactivate_claim();
		break;
	case DEACTIVATE_CLAIM_FORCIBLY:
		rval = rip->deactivate_claim_forcibly();
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
command_activate_claim( Service*, int cmd, Stream* stream )
{
	char* cap = NULL;
	Resource* rip;

	if( ! stream->code(cap) ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		free( cap );
		return FALSE;
	}
	rip = resmgr->get_by_cur_cap( cap );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		stream->end_of_message();
		reply( stream, NOT_OK );
		return FALSE;
	}
	free( cap );

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

	if( rip->is_deactivating() ) { 
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
		resmgr->walk( &Resource::release_claim );
		break;
	case VACATE_ALL_FAST:
		dprintf( D_ALWAYS, "State change: received VACATE_ALL_FAST command\n" );
		resmgr->walk( &Resource::kill_claim );
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
	resmgr->walk( &Resource::periodic_checkpoint );
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
	char* tmp;
	dprintf( D_FULLDEBUG, "command_give_state() called.\n" );
	stream->encode();
	tmp = strdup( state_to_string(resmgr->state()) );
	stream->code( tmp );
	stream->end_of_message();
	free( tmp );
	return TRUE;
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
	char* cap = NULL;
	Resource* rip;
	int rval;

	if( ! stream->code(cap) ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		if( cap ) { 
			free( cap );
		}
		refuse( stream );
		return FALSE;
	}

	rip = resmgr->get_by_any_cap( cap );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		refuse( stream );
		return FALSE;
	}

	if( resmgr->isShuttingDown() ) {
		rip->log_shutdown_ignore( cmd );
		free( cap );
		refuse( stream );
		return FALSE;
	}

	State s = rip->state();
	if( s == preempting_state ) {
		rip->log_ignore( REQUEST_CLAIM, s );
		refuse( stream );
		rval = FALSE;
	} else {
		rval = request_claim( rip, cap, stream );
	}
	free( cap );
	return rval;
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
		if( (s == claimed_state) || (s == matched_state) ) {
			rip->dprintf( D_ALWAYS, 
						  "State change: received VACATE_CLAIM command\n" );
			return rip->release_claim();
		} else {
			rip->log_ignore( cmd, s );
			return FALSE;
		}
		break;
	case VACATE_CLAIM_FAST:
		switch( s ) {
		case claimed_state:
		case matched_state:
		case preempting_state:
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
	char* cap = NULL;
	Resource* rip;
	int rval;

	if( ! stream->code(cap) ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		free( cap );
		return FALSE;
	}

		// Find Resource object for this capability
	rip = resmgr->get_by_any_cap( cap );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		return FALSE;
	}

	if( resmgr->isShuttingDown() ) {
		rip->log_shutdown_ignore( cmd );
		free( cap );
		return FALSE;
	}

		// Check resource state.  Ignore if we're preempting or
		// matched, otherwise process the command. 
	State s = rip->state();
	if( s == matched_state || s == preempting_state ) {
		rip->log_ignore( MATCH_INFO, s );
		rval = FALSE;
	} else {
		rval = match_info( rip, cap );
	}
	free( cap );
	return rval;
}


int
command_give_request_ad( Service*, int, Stream* stream) 
{
	int pid = -1;  // Starter sends it's pid so we know what
				   // resource's request ad to send 
	Resource*	rip;
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
	rip = resmgr->get_by_pid( pid );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "give_request_ad: Can't find starter with pid %d\n",
				 pid ); 
		stream->encode();
		stream->end_of_message();
		return FALSE;
	}
	if( !rip->r_cur ) {
		rip->dprintf( D_ALWAYS, 
					  "give_request_ad: resource doesn't have a current match.\n" );
		stream->encode();
		stream->end_of_message();
		return FALSE;
	}
	cp = rip->r_cur->ad();
	if( !cp ) {
		rip->dprintf( D_ALWAYS, 
					  "give_request_ad: current match has NULL classad.\n" );
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
   
	dprintf( D_ALWAYS, "In command_query_ads\n" );

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
		if( (*ad) >= queryAd ) {
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


//////////////////////////////////////////////////////////////////////
// Protocol helper functions
//////////////////////////////////////////////////////////////////////

#define ABORT \
delete req_classad;						\
if (client_addr) free(client_addr);		\
if( s == claimed_state ) {				\
	delete rip->r_pre;					\
	rip->r_pre = new Match( rip );		\
} else {								\
    if( s != owner_state ) {			\
        rip->dprintf( D_ALWAYS, "State change: claiming protocol failed\n" ); \
    }									\
	rip->change_state( owner_state );	\
}										\
return FALSE

int
request_claim( Resource* rip, char* cap, Stream* stream )
{
		// Formerly known as "reqservice"

	ClassAd	*req_classad = new ClassAd, *mach_classad = rip->r_classad;
	int cmd, mach_requirements = 1, req_requirements = 1;
	float rank = 0;
	float oldrank = 0;
	char *client_addr = NULL;
	int interval;
	State s = rip->state();

	if( !rip->r_cur ) {
		EXCEPT( "request_claim: no current match object." );
	}

		/* 
		   Now that we've been contacted by the schedd agent, we can
		   cancel the match timer on either the current or the
		   preempting match, depending on what state we're in.  We
		   want to do this right away so we don't abort this function
		   for some reason and leave that timer in place, since we'll
		   EXCEPT later on if it goes off and we're not in the matched
		   state anymore (and it's the current match at that point).
		   -Derek Wright 3/11/99 
		*/
	if( rip->state() == claimed_state ) {
		rip->r_pre->cancel_match_timer();
	} else {
		rip->r_cur->cancel_match_timer();
	}

		// Get the classad of the request.
	if( !req_classad->initFromStream(*stream) ) {
		rip->dprintf( D_ALWAYS, "Can't receive classad from schedd-agent\n" );
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
		if ( rip->state() == claimed_state ) {
			rip->r_pre->setaliveint( interval );
			rip->r_pre->client()->setaddr( client_addr );
		} else {
			rip->r_cur->setaliveint( interval );
			rip->r_cur->client()->setaddr( client_addr );
		}
		free( client_addr );
		client_addr = NULL;
	} else {
		rip->dprintf(D_FULLDEBUG, "Schedd using pre-v6.1.11 claim protocol\n");
	}

	if( !stream->end_of_message() ) {
		rip->dprintf( D_ALWAYS, "Can't receive eom from schedd-agent\n" );
		ABORT;
	}

		// At this point, the schedd has registered this socket (stream)
		// and likely has gone off to service other requests.  Thus, 
		// we need change the timeout on the socket to the schedd to be
		// very patient.  By default, daemon core put a timeout of ~20
		// seconds on this socket.  This is not at all long enough.
		// since we should only hold this match for match_timeout,
		// set the timeout to be 10 seconds less than that. -Todd
	if ( match_timeout > 10 ) {
		stream->timeout( match_timeout - 10 );
	}
		
	rip->dprintf( D_FULLDEBUG,
				  "Received capability from schedd agent (%s)\n", cap );

		// Make sure we're willing to run this job at all.  Verify that 
		// the machine and job meet each other's requirements.
	rip->r_reqexp->restore();
	if( mach_classad->EvalBool( ATTR_REQUIREMENTS, 
								req_classad, mach_requirements ) == 0 ) {
		mach_requirements = 0;
	}
	if( req_classad->EvalBool( ATTR_REQUIREMENTS, 
							   mach_classad, req_requirements ) == 0 ) {
		req_requirements = 0;
	}
	if( !mach_requirements || !req_requirements ) {
	    rip->dprintf( D_ALWAYS, "Request to claim resource refused.\n" );
		if( !mach_requirements ) {
			rip->dprintf( D_FAILURE|D_ALWAYS, "Machine requirements not satisfied.\n" );
		}
		if( !req_requirements ) {
			rip->dprintf( D_FAILURE|D_ALWAYS, "Job requirements not satisfied.\n" );
		}
		// Possibly print out the ads we just got to the logs.
	rip->dprintf( D_JOB, "REQ_CLASSAD:\n" );
	if( DebugFlags & D_JOB ) {
		req_classad->dPrint( D_JOB );
	}
	  
	rip->dprintf( D_MACHINE, "MACHINE_CLASSAD:\n" );
	if( DebugFlags & D_MACHINE ) {
		mach_classad->dPrint( D_MACHINE );
	}

		refuse( stream );
		ABORT;
	}

		// Now, make sure it's got a high enough rank to preempt us.
	rank = compute_rank( mach_classad, req_classad );

	rip->dprintf( D_FULLDEBUG, "Rank of this match is: %f\n", rank );

	if( rip->state() == claimed_state ) {
			// We're currently claimed.  We might want to preempt the
			// current claim to make way for this new one...
		if( !rip->r_pre ) {
			rip->dprintf( D_ALWAYS, 
			   "In CLAIMED state without preempting match object, aborting.\n" );
			refuse( stream );
			ABORT;
		}
		if( rip->r_pre->cap()->matches(cap) ) {
			rip->dprintf( D_ALWAYS, 
						  "Preempting match has correct capability.\n" );
			
				// Check rank, decided to preempt, if needed, do so.
			if( rank < rip->r_cur->rank() ) {
				rip->dprintf( D_ALWAYS, 
				  "Preempting match doesn't have sufficient rank, refusing.\n" );
				cmd = NOT_OK;
			} else {
				rip->dprintf( D_ALWAYS, 
				  "New match has sufficient rank, preempting current match.\n" );

					// We're going to preempt.  Save everything we
					// need to know into r_pre.
				rip->r_pre->setagentstream( stream );
				rip->r_pre->setad( req_classad );
				rip->r_pre->setrank( rank );
				rip->r_pre->setoldrank( rip->r_cur->rank() );

					// Get rid of the current claim.
				if( rank > rip->r_cur->rank() ) {
					rip->dprintf( D_ALWAYS, 
					 "State change: preempting claim based on machine rank\n" );
				} else {
					ASSERT( rank == rip->r_cur->rank() );
					rip->dprintf( D_ALWAYS, 
					 "State change: preempting claim based on user priority\n" );
				}
				rip->release_claim();

					// Tell daemon core not to do anything to the stream.
				return KEEP_STREAM;
			}					
		} else {
			rip->dprintf( D_ALWAYS,
					 "Capability from schedd agent (%s) doesn't match (%s)\n",
					 cap, rip->r_pre->capab() );
			cmd = NOT_OK;
		}
	} else {
			// We're not claimed
		if( rip->r_cur->cap()->matches(cap) ) {
			rip->dprintf( D_ALWAYS, "Request accepted.\n" );
			cmd = OK;
		} else {
			rip->dprintf( D_ALWAYS,
					"Capability from schedd agent (%s) doesn't match (%s)\n",
					cap, rip->r_cur->capab() );
			cmd = NOT_OK;
		}		
	}	

	if( cmd == OK ) {
			// We decided to accept the request, save the agent's
			// stream, the rank and the classad of this request.
		rip->r_cur->setagentstream( stream );
		rip->r_cur->setad( req_classad );
		rip->r_cur->setrank( rank );
		rip->r_cur->setoldrank( oldrank );

			// Call this other function to actually reply to the agent
			// and perform the last half of the protocol.  We use the
			// same function after the preemption has completed when
			// the startd is finally ready to reply to the agent and
			// finish the claiming process.
		return accept_request_claim( rip );
	} else {
		refuse( stream );
		ABORT;
	}
}
#undef ABORT

#define ABORT 						\
if( client_addr )					\
    free( client_addr );			\
stream->encode();					\
stream->end_of_message();			\
rip->r_cur->setagentstream( NULL );	\
rip->dprintf( D_ALWAYS, "State change: claiming protocol failed\n" ); \
rip->change_state( owner_state );	\
return KEEP_STREAM

int
accept_request_claim( Resource* rip )
{
	int interval;
	char *client_addr = NULL, *client_host, *full_client_host, *tmp;
	char RemoteOwner[512];
	RemoteOwner[0] = '\0';

		// There should not be a pre match object now.
	assert( rip->r_pre == NULL );

	Stream* stream = rip->r_cur->agentstream();
	assert( stream );
	Sock* sock = (Sock*)stream;

	stream->encode();
	if( !stream->put( OK ) ) {
		rip->dprintf( D_ALWAYS, "Can't to send cmd to agent.\n" );
		ABORT;
	}
	if( !stream->eom() ) {
		rip->dprintf( D_ALWAYS, "Can't to send eom to agent.\n" );
		ABORT;
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
			ABORT;
		} else {
			rip->dprintf( D_FULLDEBUG, "Schedd addr = %s\n", client_addr );
		}
		if( !stream->code(interval) ) {
			rip->dprintf( D_ALWAYS, "Can't receive alive interval\n" );
			ABORT;
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
	if( ! (tmp = sin_to_hostname(sock->endpoint(), NULL)) ) {
		rip->dprintf( D_ALWAYS, "Can't find hostname of client machine\n");
		ABORT;
	} 
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

		// Get the owner of this claim out of the request classad.
	if( (rip->r_cur->ad())->
		EvalString( ATTR_USER, rip->r_cur->ad(), RemoteOwner ) == 0 ) { 
		rip->dprintf( D_ALWAYS, 
				 "Can't evaluate attribute %s in request ad.\n", 
				 ATTR_USER );
		RemoteOwner[0] = '\0';
	}
	if( RemoteOwner ) {
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
		// Since we're done talking to this schedd agent, delete the stream.
	rip->r_cur->setagentstream( NULL );

	rip->dprintf( D_FAILURE|D_ALWAYS, "State change: claiming protocol successful\n" );
	rip->change_state( claimed_state );

		// Want to return KEEP_STREAM so that daemon core doesn't try
		// to delete the stream we've already deleted.
	return KEEP_STREAM;
}
#undef ABORT


#define ABORT \
if( req_classad ) 	 					\
	delete( req_classad );				\
free( shadow_addr );					\
return FALSE

int
activate_claim( Resource* rip, Stream* stream ) 
{
		// Formerly known as "startjob"
	int mach_requirements = 1, req_requirements = 1;
	ClassAd	*req_classad = NULL, *mach_classad = rip->r_classad;

	int sock_1, sock_2;
	ReliSock rsock_1, rsock_2;
	int fd_1, fd_2;
	struct sockaddr_in frm;
	int len = sizeof frm;
	StartdRec stRec;
	start_info_t ji;	/* XXXX */
	int universe, job_cluster, job_proc, starter;
	Sock* sock = (Sock*)stream;
	char* shadow_addr = strdup( sin_to_string( sock->endpoint() ));
	bool found_attr_user = false;	// did we find ATTR_USER in the ad?

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

	if( ! (rip->r_starter->settype(starter)) ) {
	    rip->dprintf( D_ALWAYS, "Requested starter is invalid.\n" );
		refuse( stream );
	    ABORT;
	}

	if( rip->r_starter->is_dc() ) {
		ji.shadowCommandSock = stream;
	} else {
		ji.shadowCommandSock = NULL;
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
	if( req_classad->EvalBool( ATTR_REQUIREMENTS, 
							   mach_classad, req_requirements ) == 0 ) {
		req_requirements = 0;
	}
	if( !mach_requirements || !req_requirements ) {
	    rip->dprintf( D_ALWAYS, "Requirements check failed! "
					  "resource = %d, request = %d\n",
					  mach_requirements, req_requirements );
		refuse( stream );
	    ABORT;
	}

		// Make sure the classad we got includes an ATTR_USER field,
		// so we know who to charge for our services.  If it's not
		// there, refuse to run the job.
	char remote_user[256];
	remote_user[0] = '\0';
	if( req_classad->EvalString(ATTR_USER, rip->r_classad, 
								remote_user) == 0 ) {
		rip->dprintf( D_FULLDEBUG, "WARNING: %s not defined in request "
					  "classad!  Using old value (%s)\n", ATTR_USER,
					  rip->r_cur->client()->user() );
	} else {
		rip->dprintf( D_FULLDEBUG, 
					  "Got RemoteUser (%s) from request classad\n",	
					  remote_user );
		found_attr_user = true;
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

#ifndef WIN32

	if( ! (rip->r_starter->is_dc()) ) {

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
		ji.ji_sock1 = fd_1;
		ji.ji_sock2 = fd_2;
	}

#endif	// of ifndef WIN32

	ji.ji_hname = rip->r_cur->client()->host();

	int now = (int)time( NULL );

		// Actually spawn the starter
	if( ! rip->spawn_starter(&ji, now) ) {
			// Error spawning starter!
		ABORT;
	}

		// Get a bunch of info out of the request ad that is now
		// relevant, and store it in the machine ad and cur Match object

	req_classad->EvalInteger( ATTR_CLUSTER_ID, req_classad, job_cluster );
	req_classad->EvalInteger( ATTR_PROC_ID, req_classad, job_proc );

	rip->dprintf( D_ALWAYS, "Remote job ID is %d.%d\n", job_cluster,
				  job_proc );

	if( req_classad->EvalInteger(ATTR_JOB_UNIVERSE,
								 rip->r_classad, universe) == 0 ) {
		universe = CONDOR_UNIVERSE_STANDARD;
		rip->dprintf( D_ALWAYS,
					  "Default universe (%d) since not in classad \n", 
					  universe );
	} else {
		rip->dprintf( D_ALWAYS, "Got universe (%d) from request classad\n",
					  universe );
	}
	if( universe == CONDOR_UNIVERSE_VANILLA ) {
		rip->dprintf( D_ALWAYS, 
					  "Startd using *_VANILLA control expressions.\n" );
	} else {
		rip->dprintf( D_ALWAYS, 
					  "Startd using standard control expressions.\n" );
	}

	if( found_attr_user ) {
		rip->r_cur->client()->setuser( remote_user );
	}
	rip->r_cur->setproc( job_proc );
	rip->r_cur->setcluster( job_cluster );
	rip->r_cur->setad( req_classad );
	rip->r_cur->setuniverse(universe);

	rip->r_cur->setjobstart(now);	
	rip->r_cur->setlastpckpt(now);	

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
match_info( Resource* rip, char* cap )
{
	int rval = FALSE;
	rip->dprintf(D_ALWAYS, "match_info called\n");

	switch( rip->state() ) {
	case claimed_state:
		if( rip->r_cur->cap()->matches(cap) ) {
				// The capability we got matches the one for the
				// current match, and we're already claimed.  There's
				// nothing to do here.
			rval = TRUE;
		} else if( rip->r_pre && rip->r_pre->cap()->matches(cap) ) {
				// The capability we got matches the preempting
				// capability we've been advertising.  Advertise
				// ourself as unavailable for future matches, update
				// the CM, and set the timer for this match.
			rip->r_reqexp->unavail();
			rip->update();
			rip->r_pre->start_match_timer();
			rval = TRUE;
		} else {
				// The capability doesn't match any of our
				// capabilities.  
				// Should never get here, since we found the rip by
				// some cap in the first place.
			EXCEPT( "Should never get here" );
		}
		break;
	case unclaimed_state:
	case owner_state:
		if( rip->r_cur->cap()->matches(cap) ) {
			rip->dprintf( D_ALWAYS, "Received match %s\n", cap );

				// Start a timer to prevent staying in matched state
				// too long. 
			rip->r_cur->start_match_timer();

				// Entering matched state sets our reqexp to unavail
				// and updates CM.
			rip->dprintf( D_FAILURE|D_ALWAYS, 
						  "State change: match notification protocol successful\n" );
			rip->change_state( matched_state );
			rval = TRUE;
		} else {
			rip->dprintf( D_ALWAYS, 
					 "Capability from negotiator (%s) doesn't match.\n",  
					 cap );			
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
