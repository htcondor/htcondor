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
	MatchClassAd mad;
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
	mad.ReplaceRightAd( &queryAd );
	bool match = false;
	stream->encode();
	ads.Open();
	while( (ad = ads.Next()) ) {
//		if( (*ad) >= queryAd ) {
		mad.RemoveLeftAd( );
		mad.ReplaceLeftAd( ad );
		if( mad.EvaluateAttrBool( "leftMatchesRight", match ) && match ) {
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
	rip->r_pre = new Claim( rip );		\
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
	if( rip->state() == claimed_state ) {
		rip->r_pre->cancel_match_timer();
	} else {
		rip->r_cur->cancel_match_timer();
	}

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
				  "Received capability from schedd (%s)\n", cap );

		// Make sure we're willing to run this job at all.  Verify that 
		// the machine and job meet each other's requirements.
	rip->r_reqexp->restore();
	if( mach_classad->EvalBool( ATTR_REQUIREMENTS, 
								req_classad, mach_requirements ) == 0 ) {
		mach_requirements = 0;
	}

	Starter* tmp_starter;
	tmp_starter = resmgr->starter_mgr.findStarter( req_classad,
												   mach_classad );
	if( ! tmp_starter ) {
		req_requirements = 0;
	} else {
		delete( tmp_starter );
		req_requirements = 1;
	}

	if( !mach_requirements || !req_requirements ) {
	    rip->dprintf( D_ALWAYS, "Request to claim resource refused.\n" );
		if( !mach_requirements ) {
			rip->dprintf( D_FAILURE|D_ALWAYS, 
						  "Machine requirements not satisfied.\n" );
		}
		if( !req_requirements ) {
			rip->dprintf( D_FAILURE|D_ALWAYS, 
						  "Job requirements not satisfied.\n" );
		}
		refuse( stream );
		ABORT;
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

		// Now, make sure it's got a high enough rank to preempt us.
	rank = compute_rank( mach_classad, req_classad );

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
		if( rip->r_pre->cap()->matches(cap) ) {
			rip->dprintf( D_ALWAYS, 
						  "Preempting claim has correct capability.\n" );
			
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
					 "Capability from schedd (%s) doesn't match (%s)\n",
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
					"Capability from schedd (%s) doesn't match (%s)\n",
					cap, rip->r_cur->capab() );
			cmd = NOT_OK;
		}		
	}	

	if( cmd == OK ) {
			// We decided to accept the request, save the schedd's
			// stream, the rank and the classad of this request.
		rip->r_cur->setRequestStream( stream );
		rip->r_cur->setad( req_classad );
		rip->r_cur->setrank( rank );
		rip->r_cur->setoldrank( oldrank );

			// Call this other function to actually reply to the
			// schedd and perform the last half of the protocol.  We
			// use the same function after the preemption has
			// completed when the startd is finally ready to reply to
			// the and finish the claiming process.
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
rip->r_cur->setRequestStream( NULL );	\
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

		// There should not be a pre claim object now.
	assert( rip->r_pre == NULL );

	Stream* stream = rip->r_cur->requestStream();
	assert( stream );
	Sock* sock = (Sock*)stream;

	stream->encode();
	if( !stream->put( OK ) ) {
		rip->dprintf( D_ALWAYS, "Can't to send cmd to schedd.\n" );
		ABORT;
	}
	if( !stream->eom() ) {
		rip->dprintf( D_ALWAYS, "Can't to send eom to schedd.\n" );
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
		rip->dprintf( D_FULLDEBUG,
					  "Can't find hostname of client machine\n" );
		rip->r_cur->client()->sethost( sin_to_string(sock->endpoint()) );
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
		// Since we're done talking to this schedd, delete the stream.
	rip->r_cur->setRequestStream( NULL );

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
	int mach_requirements = 1;
	ClassAd	*req_classad = NULL, *mach_classad = rip->r_classad;

	int sock_1, sock_2;
	ReliSock rsock_1, rsock_2;
	int fd_1, fd_2;
	struct sockaddr_in frm;
	int len = sizeof frm;
	StartdRec stRec;
	int starter;
	Sock* sock = (Sock*)stream;
	char* shadow_addr = strdup( sin_to_string( sock->endpoint() ));

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

	time_t now = time( NULL );

		// now that we've gotten this far, we're really going to try
		// to spawn the starter.  set it in our Claim object. 
	rip->r_cur->setStarter( tmp_starter );
		// Grab the job ID, so we've got it...
	rip->r_cur->saveJobInfo( req_classad );

		// Actually spawn the starter
	if( ! rip->r_cur->spawnStarter(now, shadow_sock) ) {
			// Error spawning starter!
		delete( tmp_starter );
		rip->r_cur->setStarter( NULL );
		ABORT;
	}

		// Grab everything we need/want out of the request and store
		// it in our current claim 
	rip->r_cur->beginActivation( now );

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
				// current claim, and we're already claimed.  There's
				// nothing to do here.
			rval = TRUE;
		} else if( rip->r_pre && rip->r_pre->cap()->matches(cap) ) {
				// The capability we got matches the preempting
				// capability we've been advertising.  Advertise
				// ourself as unavailable for future claims, update
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
						  "Invalid capability from negotiator (%s)\n",  
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


int
caRequestCODClaim( Stream *s, char* cmd_str, ClassAd* req_ad )
{
	char* requirements_str = NULL;
	Resource* rip;
	Claim* claim;
	MyString err_msg;
	ExprTree *tree;
	ReliSock* rsock = (ReliSock*)s;
	const char* owner = rsock->getOwner();
	ClassAdUnParser unp;
	string bufString;

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
	tree = req_ad->Lookup( ATTR_REQUIREMENTS );
	if( ! tree ) {
		dprintf( D_FULLDEBUG, 
				 "Request did not contain %s, assuming TRUE\n",
				 ATTR_REQUIREMENTS );
		requirements_str = strdup( "TRUE" );
		req_ad->Insert( "Requirements = TRUE" );
	} else {
		unp.Unparse( bufString, tree );
		const char *bufCString = bufString.c_str( );
		requirements_str = (char *) malloc( strlen( bufCString ) + 1 );
		strcpy( requirements_str, bufCString );
	}

		// Find the right resource for this claim
	rip = resmgr->findRipForNewCOD( req_ad );

	if( ! rip ) {
		err_msg = "Can't find Resource matching ";
		err_msg += ATTR_REQUIREMENTS;
		err_msg += " (";
		err_msg += requirements_str;
		err_msg += ')';
		free( requirements_str );
		return sendErrorReply( s, cmd_str, CA_FAILURE, err_msg.Value() );
	}

		// done with this now, so don't leak it
	free( requirements_str );

		// try to create the new claim
	claim = rip->newCODClaim();
	if( ! claim ) {
		return sendErrorReply( s, cmd_str, CA_FAILURE, 
							   "Can't create new COD claim" );
	}

		// Stash some info about who made this request in the Claim  
	claim->client()->setuser( owner );
	claim->client()->setowner(owner );
	claim->client()->sethost( rsock->endpoint_ip_str() );

		// now, we just fill in the reply ad appropriately.  publish
		// a complete resource ad (like what we'd send to the
		// collector), and include the ClaimID  
	ClassAd reply;
	rip->publish( &reply, A_PUBLIC | A_ALL | A_EVALUATED );

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
command_classad_handler( Service*, int, Stream* s )
{
	int rval;
	ClassAd ad;
	ReliSock* rsock = (ReliSock*)s;

        // make sure this connection is authenticated, and we know who
        // the user is.  also, set a timeout, since we don't want to
        // block long trying to read from our client.   
    rsock->timeout( 10 );  
    rsock->decode();
    if( ! rsock->isAuthenticated() ) {
		char* p = SecMan::getSecSetting( "SEC_%s_AUTHENTICATION_METHODS",
										 "WRITE" );
        MyString methods;
        if( p ) {
            methods = p;
            free( p );
        } else {
            methods = SecMan::getDefaultAuthenticationMethods();
        }
		CondorError errstack;
        if( ! rsock->authenticate(methods.Value(), &errstack) ) {
                // we failed to authenticate, we should bail out now
                // since we don't know what user is trying to perform
                // this action.
			sendErrorReply( s, "CA_CMD", CA_NOT_AUTHENTICATED,
							"Server: client failed to authenticate" );
			dprintf (D_ALWAYS, "STARTD: authenticate failed\n");
			dprintf (D_ALWAYS, "%s", errstack.get_full_text());
			return FALSE;
        }
    }
	
	if( ! ad.initFromStream(*s) ) { 
		dprintf( D_ALWAYS, 
				 "Failed to read ClassAd from network, aborting\n" ); 
		return FALSE;
	}
	if( ! s->eom() ) { 
		dprintf( D_ALWAYS, 
				 "Error, more data on stream after ClassAd, aborting\n" ); 
		return FALSE;
	}

	if( DebugFlags & D_FULLDEBUG && DebugFlags & D_COMMAND ) {
		dprintf( D_COMMAND, "Command ClassAd:\n" );
		ad.dPrint( D_COMMAND );
		dprintf( D_COMMAND, "*** End of Command ClassAd***\n" );
	}

	char* cmd_str = NULL;
	int cmd;
	if( ! ad.LookupString(ATTR_COMMAND, &cmd_str) ) {
		dprintf( D_ALWAYS, "Failed to read %s from ClassAd, aborting\n", 
				 ATTR_COMMAND );
		return FALSE;
	}		
	cmd = getCommandNum( cmd_str );
	if( cmd < 0 ) {
		unknownCmd( s, cmd_str );
		free( cmd_str );
		return FALSE;
	}

	if( cmd == CA_REQUEST_CLAIM ) { 
			// this one's a special case, since we're creating a new
			// claim... 
		rval = caRequestClaim( s, cmd_str, &ad );
		free( cmd_str );
		return rval;
	}

		// for all the rest, we need to read the ClaimId out of the
		// ad, find the right claim, and call the appropriate method
		// on it.  

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


