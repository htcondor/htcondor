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
#include "startd.h"
static char *_FileName_ = __FILE__;

int
command_handler( Service*, int cmd, Stream* stream )
{
	char* cap = NULL;
	Resource* rip;
	int rval = FALSE;

	if( ! stream->code(cap) ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		free( cap );
		return FALSE;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		free( cap );
		return FALSE;
	}
	rip = resmgr->get_by_cur_cap( cap );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		return FALSE;
	}
	free( cap );

	State s = rip->state();

		// RELEASE_CLAIM only makes sense in two states
	if( cmd == RELEASE_CLAIM ) {
		if( (s == claimed_state) || (s == matched_state) ) {
			return rip->release_claim();
		} else {
			log_ignore( cmd, s );
			return FALSE;
		}
	}

		// The rest of these only make sense in claimed state 
	if( s != claimed_state ) {
		log_ignore( cmd, s );
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
		rval = rip->request_new_proc();
		break;
	}
	return rval;
}


int
command_activate_claim( Service*, int, Stream* stream )
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

	State s = rip->state();
	Activity a = rip->activity();
	if( s != claimed_state || a != idle_act ) {
		log_ignore( ACTIVATE_CLAIM, s );
		stream->end_of_message();
		reply( stream, NOT_OK );
		return FALSE;
	}
	return activate_claim( rip, stream );
}


int
command_vacate( Service*, int, Stream* ) 
{
	dprintf( D_ALWAYS, "command_vacate() called.\n" );
	resmgr->walk( &Resource::release_claim );
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
	last_x_event = (int)time( NULL );
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
command_give_classad( Service*, int, Stream* stream ) 
{
	ClassAd* cp = resmgr->rip()->r_classad;
	dprintf( D_FULLDEBUG, "command_give_classad() called.\n" );
	stream->encode();
	cp->put( *stream );
	stream->end_of_message();
	return TRUE;
}


int
command_request_claim( Service*, int, Stream* stream ) 
{
	char* cap = NULL;
	Resource* rip;
	int rval;

	if( ! stream->code(cap) ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		free( cap );
		return FALSE;
	}

	rip = resmgr->get_by_any_cap( cap );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		return FALSE;
	}

	State s = rip->state();
	switch( s ) {
	case claimed_state:
	case matched_state:
	case unclaimed_state:
		rval = request_claim( rip, cap, stream );
		break;
	default:
		log_ignore( REQUEST_CLAIM, s );
		rval = FALSE;
		break;
	}
	free( cap );
	return rval;
}


int
command_match_info( Service*, int, Stream* stream ) 
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

		// Check resource state.  If we're in claimed or unclaimed,
		// process the command.  Otherwise, ignore it.  
	State s = rip->state();
	if( s == claimed_state || s == unclaimed_state ) {
		rval = match_info( rip, cap );
	} else {
		log_ignore( MATCH_INFO, s );
		rval = FALSE;
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
		dprintf( D_ALWAYS, 
				 "give_request_ad: rip doesn't have a current match.\n" );
		stream->encode();
		stream->end_of_message();
		return FALSE;
	}
	cp = rip->r_cur->ad();
	if( !cp ) {
		dprintf( D_ALWAYS, 
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

//////////////////////////////////////////////////////////////////////
// Protocol helper functions
//////////////////////////////////////////////////////////////////////
#define REFUSE \
stream->end_of_message();	\
stream->encode();			\
stream->put(NOT_OK);		\
stream->end_of_message();			

#define ABORT \
delete requestAd;						\
if( s == claimed_state ) {				\
	delete rip->r_pre;					\
	rip->r_pre = new Match;				\
} else {								\
	rip->change_state( owner_state );	\
}										\
return FALSE

int
request_claim( Resource* rip, char* cap, Stream* stream )
{
		// Formerly known as "reqservice"

	ClassAd *requestAd = new ClassAd;
	int cmd;
	float rank = -1;
	float oldrank = -1;
	State s = rip->state();

	if( !rip->r_cur ) {
		EXCEPT( "request_claim: no current match object." );
	}

		// Get the classad of the request.
	if( !requestAd->get(*stream) ) {
		dprintf( D_ALWAYS, "Can't receive classad from schedd-agent\n" );
		ABORT;
	}

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't receive eom from schedd-agent\n" );
		ABORT;
	}

	dprintf(D_ALWAYS,
			"Received capability from schedd agent (%s)\n", cap );

	rank = compute_rank( rip->r_classad, requestAd );

	dprintf( D_FULLDEBUG, "Rank of this match is: %f\n", rank );

	if( rip->state() == claimed_state ) {
			// We're currently claimed.  We might want to preempt the
			// current claim to make way for this new one...
		if( !rip->r_pre ) {
			dprintf( D_ALWAYS, 
					 "In CLAIMED state without preempting match object, aborting.\n" );
			REFUSE;
			ABORT;
		}
		
			// Now that we're being contacted by the schedd agent, we
			// can cancel the match timer on the preempting match.
		rip->r_pre->cancel_match_timer();

		if( rip->r_pre->cap()->matches(cap) ) {
			dprintf( D_ALWAYS, "Preempting match has correct capability.\n" );
			
				// Check rank, decided to preempt, if needed, do so.
			if( rank < rip->r_cur->rank() ) {
				dprintf( D_ALWAYS, 
						 "Preempting match doesn't have sufficient rank, refusing.\n" );
				cmd = NOT_OK;
			} else {
				dprintf( D_ALWAYS, 
						 "New match has sufficient rank, preempting current match.\n" );

					// We're going to preempt.  Save everything we
					// need to know into r_pre.
				rip->r_pre->setagentstream( stream );
				rip->r_pre->setad( requestAd );
				rip->r_pre->setrank( rank );
				rip->r_pre->setoldrank( rip->r_cur->rank() );

					// Get rid of the current claim.
				rip->release_claim();

					// Tell daemon core not to do anything to the stream.
				return KEEP_STREAM;
			}					
		} else {
			dprintf( D_ALWAYS,
					 "Capability from schedd agent (%s) doesn't match (%s)\n",
					 cap, rip->r_pre->capab() );
			cmd = NOT_OK;
		}
	} else {
			// We're not claimed

			// Now that we're being contacted by the schedd agent, we
			// can cancel the match timer on the current match.
		rip->r_cur->cancel_match_timer();

		if( rip->r_cur->cap()->matches(cap) ) {
			dprintf(D_ALWAYS, "Request accepted.\n");
			cmd = OK;
		} else {
			dprintf(D_ALWAYS,
					"Capability from schedd agent (%s) doesn't match (%s)\n",
					cap, rip->r_cur->capab() );
			cmd = NOT_OK;
		}		
	}	

	if( cmd == OK ) {
			// We decided to accept the request, save the agent's
			// stream, the rank and the classad of this request.
		rip->r_cur->setagentstream( stream );
		rip->r_cur->setad( requestAd );
		rip->r_cur->setrank( rank );
		rip->r_cur->setoldrank( oldrank );

			// Call this other function to actually reply to the agent
			// and perform the last half of the protocol.  We use the
			// same function after the preemption has completed when
			// the startd is finally ready to reply to the agent and
			// finish the claiming process.
		return accept_request_claim( rip );
	} else {
		REFUSE;
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
rip->change_state( owner_state );	\
return KEEP_STREAM

int
accept_request_claim( Resource* rip )
{
	int interval;
	char *client_addr = NULL, *client_host, *full_client_host, *tmp;
	char RemoteUser[512];
	RemoteUser[0] = '\0';

		// There should not be a pre match object now.
	assert( rip->r_pre == NULL );

	Stream* stream = rip->r_cur->agentstream();
	assert( stream );

	stream->encode();
	if( !stream->put( OK ) ) {
		dprintf( D_ALWAYS, "Can't to send cmd to agent.\n" );
		ABORT;
	}
	if( !stream->eom() ) {
		dprintf( D_ALWAYS, "Can't to send eom to agent.\n" );
		ABORT;
	}

		// Grab the schedd addr and alive interval.
	stream->decode();
	if( ! stream->code(client_addr) ) {
		dprintf( D_ALWAYS, "Can't receive schedd addr.\n" );
		ABORT;
	} else {
		dprintf( D_FULLDEBUG, "Schedd addr = %s\n", client_addr );
	}
	if( !stream->code(interval) ) {
		dprintf( D_ALWAYS, "Can't receive alive interval\n" );
		ABORT;
	} else {
		dprintf( D_FULLDEBUG, "Alive interval = %d\n", interval );
	}
	stream->end_of_message();

		// Now, store them into r_cur
	rip->r_cur->setaliveint( interval );
	rip->r_cur->client()->setaddr( client_addr );
	free( client_addr );

		// Figure out the hostname of our client.
	if( ! (tmp = sin_to_hostname(stream->endpoint(), NULL)) ) {
		dprintf( D_ALWAYS, "Can't find hostname of client machine\n");
		ABORT;
	} 
	client_host = strdup( tmp );

		// Try to make sure we've got a fully-qualified hostname.
	full_client_host = get_full_hostname( client_host );
	if( ! full_client_host ) {
		dprintf( D_ALWAYS, "Error finding full hostname of %s\n", 
				 client_host );
		rip->r_cur->client()->sethost( client_host );
	} else {
		rip->r_cur->client()->sethost( full_client_host );
	}
	free( client_host );

		// Get the owner of this claim out of the request classad.
	if( (rip->r_cur->ad())->
		EvalString( ATTR_USER, rip->r_cur->ad(), RemoteUser ) == 0 ) { 
		dprintf( D_ALWAYS, 
				 "Can't evaluate attribute %s in request ad.\n", 
				 ATTR_USER );
		RemoteUser[0] = '\0';
	}
	if( RemoteUser ) {
		rip->r_cur->client()->setname( RemoteUser );
		dprintf( D_ALWAYS, "Remote user is %s\n", RemoteUser );
	} else {
		dprintf( D_ALWAYS, "Remote user is NULL\n" );
			// What else should we do here???
	}		
		// Since we're done talking to this schedd agent, delete the stream.
	rip->r_cur->setagentstream( NULL );

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

#define CLOSE \
(void)close(sock_1);	\
(void)close(sock_2);


int
activate_claim( Resource* rip, Stream* stream ) 
{
		// Formerly known as "startjob"
	int mach_requirements = 1, req_requirements = 1;
	ClassAd	*req_classad = NULL, *mach_classad = rip->r_classad;

	int sock_1, sock_2;
	int fd_1, fd_2;
	struct sockaddr_in frm;
	int len = sizeof frm;
	char official_name[MAXHOSTLEN];
	StartdRec stRec;
	start_info_t ji;	/* XXXX */
	int universe, job_cluster, job_proc, starter;
	char* shadow_addr = strdup( sin_to_string( stream->endpoint() ));

	if( rip->state() != claimed_state ) {
		dprintf(D_ALWAYS, "Not in claimed state, aborting.\n");
		REFUSE;
		ABORT;
	}

	dprintf( D_ALWAYS,
			 "Got activate_claim request from shadow (%s)\n", 
			 shadow_addr );

		// Find out what version of the starter to use for the activation.
	if( ! stream->code( starter ) ) {
		dprintf( D_ALWAYS, "Can't read starter command from %s\n",
				 shadow_addr );
		REFUSE;
		ABORT;
	}
	if( starter >= MAX_STARTERS ) {
	    dprintf( D_ALWAYS, "Requested alternate starter is out of range.\n" );
		REFUSE;
	    ABORT;
	}

	if( starter ) {
		RealStarter = AlternateStarter[starter];
	} else {
		RealStarter = PrimaryStarter;
	}

	/* We could have been asked to run an alternate version of the
	 * starter which is not listed in our config file.  If so, Starter
	 * would show up as NULL here.  */
	if( RealStarter == NULL ) {
	    dprintf( D_ALWAYS, "Alternate starter not found in config file\n" );
		REFUSE;
	    ABORT;
	}
	
		// Grab request class ad 
	req_classad = new ClassAd;
	if( !req_classad->get(*stream) ) {
		dprintf( D_ALWAYS, "Can't receive request classad from shadow.\n" );
		ABORT;
	}
	if (!stream->end_of_message()) {
		dprintf( D_ALWAYS, "Can't receive eom() from shadow.\n" );
		ABORT;
	}

	dprintf( D_FULLDEBUG, "Read request ad and starter from shadow.\n" );
	if( starter ) {
		dprintf( D_FULLDEBUG, "Using alternate starter #%d.\n", starter );
	} else {
		dprintf( D_FULLDEBUG, "Using default starter.\n" );
	}

		// This recomputes all attributes and fills in the machine classad 
	rip->update_classad();
	
		// Possibly print out the ads we just got to the logs.
	dprintf( D_JOB, "REQ_CLASSAD:\n" );
	if( DebugFlags & D_JOB ) {
		req_classad->fPrint(stdout);
		fprintf( stdout, "\n" );
	}

	  
	dprintf( D_MACHINE, "MACHINE_CLASSAD:\n" );
	if( DebugFlags & D_MACHINE ) {
		mach_classad->fPrint(stdout);
		fprintf( stdout, "\n" );
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
	    dprintf( D_ALWAYS,"mach_requirements = %d, request_requirements = %d\n",
				 mach_requirements, req_requirements );
		REFUSE;
	    ABORT;
	}

		// If we're here, we've decided to activate the claim.  Tell
		// the shadow we're ok.
	stream->encode();
	if( !stream->put( OK ) ) {
		dprintf( D_ALWAYS, "Can't send OK to shadow.\n" );
		ABORT;
	}
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't send eom to shadow.\n" );
		ABORT;
	}

		// Set up the two starter ports and send them to the shadow
	stRec.version_num = VERSION_FOR_FLOCK;
	stRec.ports.port1 = create_port(&sock_1);
	stRec.ports.port2 = create_port(&sock_2);
	
	/* send name of server machine: dhruba */ 
	if (get_machine_name(official_name) == -1) {
		dprintf(D_ALWAYS,"Error in get_machine_name\n");
		CLOSE;
		ABORT;
	}
	/* fvdl XXX fails for machines with multiple interfaces */
	stRec.server_name = (char *)strdup( official_name );
	
	/* send ip_address of server machine: dhruba */ 
		// This is very whacky and wrong... needs fixing.
	if( get_inet_address((in_addr*)&stRec.ip_addr) == -1) {
		dprintf(D_ALWAYS,"Error in get_machine_name\n");
		CLOSE;
		ABORT;
	}
	
	stream->encode();
	if (!stream->code(stRec)) {
		CLOSE;
		ABORT;
	}

	if (!stream->eom()) {
		CLOSE;
		ABORT;
	}

	/* now that we sent stRec, free stRec.server_name which we strduped */
	free( stRec.server_name );

#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	/* Wait for connection to port1 */
	len = sizeof frm;
	memset( (char *)&frm,0, sizeof frm );
	while( (fd_1=tcp_accept_timeout(sock_1, (struct sockaddr *)&frm,
									&len, 150)) < 0 ) {
		if( fd_1 != -3 ) {  /* tcp_accept_timeout returns -3 on EINTR */
			if( fd_1 == -2 ) {
				dprintf( D_ALWAYS, "accept timed out\n" );
				CLOSE;
				ABORT;
			} else {
				dprintf( D_ALWAYS, "tcp_accept_timeout returns %d, errno=%d\n",
						 fd_1, errno );
				CLOSE;
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
				dprintf( D_ALWAYS, "accept timed out\n" );
				CLOSE;
				ABORT;
			} else {
				dprintf( D_ALWAYS, "tcp_accept_timeout returns %d, errno=%d\n",
						 fd_2, errno );
				CLOSE;
				ABORT;
			}
		}
	}
	CLOSE;

	ji.ji_hname = rip->r_cur->client()->host();
	ji.ji_sock1 = fd_1;
	ji.ji_sock2 = fd_2;

		// Get a bunch of info out of the request ad that is now
		// relevant, and store it in the machine ad and cur Match object

	req_classad->EvalInteger( ATTR_CLUSTER_ID, req_classad, job_cluster );
	req_classad->EvalInteger( ATTR_PROC_ID, req_classad, job_proc );
	rip->r_cur->setproc( job_proc );
	rip->r_cur->setcluster( job_cluster );
	dprintf( D_ALWAYS, "Remote job ID is %d.%d\n", job_cluster, job_proc );

	rip->r_cur->setad( req_classad );

	if( !rip->r_cur->ad() ||
		((rip->r_cur->ad())->EvalInteger( ATTR_JOB_UNIVERSE,
										  rip->r_classad,universe)==0) ) {
		universe = STANDARD;
		dprintf( D_ALWAYS,
				 "Default universe (%d) since not in classad \n", 
				 universe );
	} else {
		dprintf( D_ALWAYS, "Got universe (%d) from request classad\n",
				 universe );
	}
	if( universe == VANILLA ) {
		dprintf( D_ALWAYS, "Startd using *_VANILLA control params.\n" );
	} else {
		dprintf( D_ALWAYS, "Startd using standard control params.\n" );
	}
	rip->r_cur->setuniverse(universe);

		// Actually spawn the starter
	rip->r_starter->setname( RealStarter );
	rip->r_starter->spawn( &ji );

	int now = (int)time( NULL );
	rip->r_cur->setjobstart(now);	
	rip->r_cur->setlastpckpt(now);	

		// Finally, update all these things into the resource classad.
	rip->r_cur->update( rip->r_classad );

	rip->change_state( busy_act );

	free( shadow_addr );
#endif
	return TRUE;
}
#undef ABORT
#undef CLOSE


int
match_info( Resource* rip, char* cap )
{
	int rval = FALSE;
	dprintf(D_ALWAYS, "match_info called\n");

	if( rip->state() == claimed_state ) {
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
	} else {
		assert( rip->state() == unclaimed_state );
		if( rip->r_cur->cap()->matches(cap) ) {
			dprintf( D_ALWAYS, "Received match %s\n", cap );

				// Start a timer to prevent staying in matched state
				// too long. 
			rip->r_cur->start_match_timer();

				// Entering matched state sets our reqexp to unavail
				// and updates CM.
			rip->change_state( matched_state );
			rval = TRUE;
		} else {
			dprintf( D_ALWAYS, 
					 "Capability from negotiator (%s) doesn't match.\n",  
					 cap );			
			rval = FALSE;
		}
	}
	return rval;
}
