#include "startd.h"
static char *_FileName_ = __FILE__;

int
timeout_and_update()
{
	Resource* rip = (Resource*) daemonCore->GetDataPtr();
	return rip->eval_and_update();
}


int
poll_resource()
{
	Resource* rip = (Resource*) daemonCore->GetDataPtr();
	return rip->eval_state();
}


int
alive( Resource* rip )
{
	assert( rip->state() == claimed_state );
	if( !rip->r_cur ) {
		dprintf( D_ALWAYS, "Got keep alive with no current match object.\n" );
		return FALSE;
	}

	if( !rip->r_cur->client() ) {
		dprintf( D_ALWAYS, "Got keep alive with no current client object.\n" );
		return FALSE;
	}
	rip->r_cur->alive();
	return TRUE;
}


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
	ClassAd templateCA;
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

	if( !stream->eom() ) {
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
				release_claim( rip );

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
		oldrank = compute_rank( rip->r_classad, &templateCA );
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
rip->change_state( owner_state );	\
return FALSE

int
accept_request_claim( Resource* rip )
{
	int interval;
	char* client_addr = NULL;
	char tmp[128];
	char RemoteUser[512];
	RemoteUser[0] = '\0';
	struct hostent *hp;
	struct sockaddr_in* from;

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
	from = stream->endpoint();
	if( (hp = gethostbyaddr((char *)&from->sin_addr,
							sizeof(struct in_addr),
							from->sin_family)) == NULL ) {
		dprintf( D_FULLDEBUG, "gethostbyaddr failed. errno= %d\n", errno );
		EXCEPT("Can't find host name");
	}
	rip->r_cur->client()->sethost( hp->h_name );
	sprintf( tmp, "%s=\"%s\"", ATTR_CLIENT_MACHINE, hp->h_name );
	(rip->r_classad)->Insert( tmp );

		// Get the owner of this claim out of the request classad and
		// put it into our resource ad.
	if( (rip->r_cur->ad())->
		EvalString( ATTR_USER, rip->r_cur->ad(), RemoteUser ) == 0 ) { 
		dprintf( D_ALWAYS, 
				 "Can't evaluate attribute %s in request ad.\n", 
				 ATTR_USER );
		RemoteUser[0] = '\0';
	}
	if( RemoteUser ) {
		rip->r_cur->client()->setname( RemoteUser );
		sprintf(tmp, "%s=\"%s\"", ATTR_REMOTE_USER, RemoteUser );
		(rip->r_classad)->Insert( tmp );
		dprintf( D_ALWAYS, "Remote user is %s\n", RemoteUser );
	} else {
		dprintf( D_ALWAYS, "Remote user is NULL\n" );
			// What else should we do here???
	}		

		// Generate a preempting match object
	rip->r_pre = new Match;

	rip->change_state( claimed_state );

	return TRUE;
}
#undef ABORT


int
release_claim( Resource* rip )
{
		// Formerly known as "vacate service"
	dprintf(D_ALWAYS, "Called release_claim()\n");
	if( rip->state() == claimed_state) {
		rip->change_state( preempting_state );
	}
	return TRUE;
}	


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

	char tmp[80];
	int sock_1, sock_2;
	int fd_1, fd_2;
	struct sockaddr_in frm;
	int len = sizeof frm;
	char official_name[MAXHOSTLEN];
	StartdRec stRec;
	start_info_t ji;	/* XXXX */
	char JobId[80];
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

	RealStarter = AlternateStarter[starter];

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

	dprintf( D_FULLDEBUG, "About to reply ok to the shadow.\n" );

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

	dprintf( D_FULLDEBUG, "Finished replying ok to the shadow.\n" );

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
	sprintf( JobId, "%d.%d", job_cluster, job_proc );
	sprintf(tmp, "%s=\"%s\"", ATTR_JOB_ID, JobId );
	(rip->r_classad)->Insert( tmp );
	dprintf( D_ALWAYS, "Remote job ID is %s\n", JobId );

	rip->r_cur->setad( req_classad );
	if( !rip->r_cur->ad() ||
		((rip->r_cur->ad())->EvalInteger("UNIVERSE",
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
	/*
	 * Put the JobUniverse into the machine classad so that the negotiator
	 * can take it into account for preemption (i.e. dont preempt vanilla
	 * jobs).
	 */
	sprintf( tmp, "%s=%d", ATTR_JOB_UNIVERSE, universe );
	(rip->r_classad)->Insert( tmp );

		// Actually spawn the starter
	rip->r_starter->setname( RealStarter );
	rip->r_starter->spawn( &ji );

	rip->change_state( busy_act );

	int now = (int)time( NULL );

	sprintf(tmp, "%s=%d", ATTR_JOB_START, now );
	(rip->r_classad)->Insert( tmp );

	sprintf(tmp, "%s=%d", ATTR_LAST_PERIODIC_CHECKPOINT, now );
	(rip->r_classad)->Insert( tmp );

	free( shadow_addr );
#endif
	return TRUE;
}
#undef ABORT
#undef CLOSE

int
deactivate_claim( Resource* rip )
{
#if defined(WIN32) /* NEED TO PORT TO WIN32 */
	return TRUE;
#else
	dprintf(D_ALWAYS, "Called deactivate_claim()\n");
	if( (rip->state() == claimed_state) &&
		(rip->r_starter->active()) ) {
		return rip->r_starter->kill( SIGTSTP );
	} else {
		return FALSE;
	}
#endif
}


int
deactivate_claim_forcibly( Resource* rip )
{
#if defined(WIN32) /* NEED TO PORT TO WIN32 */
	return TRUE;
#else
	dprintf(D_ALWAYS, "Called deactivate_claim_forcibly()\n");
	if( (rip->state() == claimed_state) &&
		(rip->r_starter->active()) ) {
		return rip->r_starter->kill( SIGINT );
	} else {
		return FALSE;
	}
#endif
}


int
vacate_claim( Resource* rip )
{
	dprintf(D_ALWAYS, "Called vacate_claim()\n");
	switch( rip->state() ) {
	case claimed_state:
		rip->change_state( preempting_state );
		return TRUE;
	case matched_state:
	case owner_state:
	case unclaimed_state:
		return TRUE;
	case preempting_state:
		if( rip->r_starter->active() ) {
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
			if( rip->r_starter->kill( SIGTSTP ) < 0 ) {
				return FALSE;
			} else {
				return rip->change_state( vacating_act );
			}
#endif
		} else {
			return rip->change_state( idle_act );
		}
	default:
		EXCEPT( "Unknown state (%d) in vacate_claim",
				(int)rip->state() );
	}
	return FALSE;
}


int
kill_claim( Resource* rip )
{
	dprintf(D_ALWAYS, "Called kill_claim()\n");
	switch( rip->state() ) {
	case claimed_state:
		rip->change_state( preempting_state );
		return TRUE;
	case matched_state:
	case owner_state:
	case unclaimed_state:
		return TRUE;
	case preempting_state:
		if( rip->r_starter->active() ) {
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
			if( rip->r_starter->kill( SIGINT ) < 0 ) {
				return FALSE;
			} else {
				return rip->change_state( killing_act );
			}
#endif
		} else {
			return rip->change_state( idle_act );
		}
	default:
		EXCEPT( "Unknown state (%d) in kill_claim",
				(int)rip->state() );
	}
	return FALSE;
}

int
suspend_claim( Resource* rip )
{
#if defined(WIN32) /* NEED TO PORT TO WIN32 */
	return TRUE;
#else
	dprintf(D_ALWAYS, "Called suspend_claim()\n");
	if( rip->state() == claimed_state ) {
		if( rip->r_starter->kill( SIGUSR1 ) < 0 ) {
			return FALSE;
		} else {
			return rip->change_state( suspended_act );
		}
	} else {
		return FALSE;
	}
#endif
}


int
continue_claim( Resource* rip )
{
#if defined(WIN32) /* NEED TO PORT TO WIN32 */
	return TRUE;
#else
	dprintf(D_ALWAYS, "Called continue_claim()\n");
	if( rip->state() == claimed_state ) {
		if( rip->r_starter->kill( SIGCONT ) < 0 ) {
			return FALSE;
		} else {
			return rip->change_state( busy_act );
		}
	} else {
		return FALSE;
	}
#endif
}


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


int
match_timed_out( Service* )
{
	Match* match = (Match*) daemonCore->GetDataPtr();
	Resource* rip = resmgr->get_by_any_cap( match->capab() );

	if( !rip ) {
		EXCEPT( "Can't find resource of expired match." );
	}

	if( rip->r_cur->cap()->matches( match->capab() ) ) {
		if( rip->state() != matched_state ) {
			EXCEPT( "Match timed out but not in matched state." );
		}
		
		delete rip->r_cur;
		rip->r_cur = new Match;
		rip->change_state( owner_state );
	} else {
			// The match that timed out was the preempting match.
		assert( rip->r_pre->cap()->matches( match->capab() ) );
			// We need to generate a new preempting match object,
			// restore our reqexp, and update the CM. 
		delete rip->r_pre;
		rip->r_pre = new Match;
		rip->r_reqexp->pub();
		rip->update();
	}		
	return TRUE;
}

int
claim_timed_out( Service* )
{
	Match* match = (Match*) daemonCore->GetDataPtr();
	Resource* rip = resmgr->get_by_cur_cap( match->capab() );

	if( !rip ) {
		EXCEPT( "Can't find resource of expired claim." );
	}

		// Note that this claim timed out so we don't try to send a
		// command to our client.
	match->claim_timed_out();

		// Release the claim.
	release_claim( rip );

	return TRUE;
}


int
periodic_checkpoint( Resource* rip )
{
	char tmp[80];

	if( rip->state() != claimed_state ) {
		return FALSE;
	}
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	if( rip->r_starter->kill( SIGUSR2 ) < 0 ) {
		return FALSE;
	}
#endif
	
	sprintf( tmp, "%s=%d", ATTR_LAST_PERIODIC_CHECKPOINT, (int)time(NULL) );
	(rip->r_classad)->Insert(tmp);

	return TRUE;
}


int
request_new_proc( Resource* rip )
{
#if defined(WIN32) /* NEED TO PORT TO WIN32 */
	return TRUE;
#else
	if( rip->state() != claimed_state ) {
		return FALSE;
	}
	if( rip->r_starter->active() ) {
		if( rip->r_starter->kill( SIGHUP ) < 0 ) {
			return FALSE;
		} else {
			return TRUE;
		}
	}
	return FALSE;
#endif
}	

