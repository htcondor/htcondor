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

/*  
  	This file implements the classes defined in claim.h.  See that
	file for comments and documentation on what it's about.

	Originally written 9/29/97 by Derek Wright <wright@cs.wisc.edu>

	Decided the Match object should really be called "Claim" (the
	files were renamed in cvs from Match.[Ch] to claim.[Ch], and
	renamed everything on 1/10/03 - Derek Wright
*/

#include "condor_common.h"
#include "startd.h"

///////////////////////////////////////////////////////////////////////////
// Claim
///////////////////////////////////////////////////////////////////////////

Claim::Claim( Resource* rip, bool is_cod )
{
	c_client = new Client;
	c_id = new ClaimId( is_cod );
	if( ! is_cod ) {
		c_id->dropFile( rip->r_id );
	}
	c_ad = NULL;
	c_starter = NULL;
	c_rank = 0;
	c_oldrank = 0;
	c_universe = -1;
	c_request_stream = NULL;
	c_match_tid = -1;
	c_lease_tid = -1;
	c_aliveint = -1;
	c_lease_duration = -1;
	c_cluster = -1;
	c_proc = -1;
	c_global_job_id = NULL;
	c_job_start = -1;
	c_last_pckpt = -1;
	c_rip = rip;
	c_is_cod = is_cod;
	c_cod_keyword = NULL;
	c_has_job_ad = 0;
	c_pending_cmd = -1;
	c_wants_remove = false;
	c_claim_started = 0;
		// to make purify happy, we want to initialize this to
		// something.  however, we immediately set it to UNCLAIMED
		// (which is what it should really be) by using changeState()
		// so we get all the nice functionality that method provides.
	c_state = CLAIM_IDLE;
	changeState( CLAIM_UNCLAIMED );
	c_job_total_run_time = 0;
	c_job_total_suspend_time = 0;
	c_claim_total_run_time = 0;
	c_claim_total_suspend_time = 0;
	c_may_unretire = true;
	c_retire_peacefully = false;
	c_preempt_was_true = false;
}


Claim::~Claim()
{	
	if( c_is_cod ) {
		dprintf( D_FULLDEBUG, "Deleted claim %s (owner '%s')\n", 
				 c_id->id(), 
				 c_client->owner() ? c_client->owner() : "unknown" );  
	}

		// Cancel any timers associated with this claim
	this->cancel_match_timer();
	this->cancelLeaseTimer();

		// Free up memory that's been allocated
	if( c_ad ) {
		delete( c_ad );
	}
	delete( c_id );
	if( c_client ) {
		delete( c_client );
	}
	if( c_request_stream ) {
		delete( c_request_stream );
	}
	if( c_starter ) {
		delete( c_starter );
	}
	if( c_global_job_id ) { 
		free( c_global_job_id );
	}
	if( c_cod_keyword ) {
		free( c_cod_keyword );
	}
}	
	

void
Claim::vacate() 
{
	assert( c_id );
		// warn the client of this claim that it's being vacated
	if( c_client && c_client->addr() ) {
		c_client->vacate( c_id->id() );
	}
}


void
Claim::publish( ClassAd* ad, amask_t how_much )
{
	MyString line;
	char* tmp;
	char *remoteUser;

	if( IS_PRIVATE(how_much) ) {
			// None of this belongs in private ads
		return;
	}

		/*
		  NOTE: currently, we publish all of the following regardless
		  of the mask (e.g. UPDATE vs. TIMEOUT).  Given the bug with
		  ImageSize being recomputed but not used due to UPDATE
		  vs. TIMEOUT confusion when publishing it, I'm inclined to
		  ignore the performance cost of publishing the same stuff
		  every timeout.  If, for some crazy reason, this becomes a
		  problem, we can always seperate these into UPDATE + TIMEOUT
		  attributes and only publish accordingly...  
		  Derek <wright@cs.wisc.edu> 2005-08-11
		*/

	line.sprintf( "%s = %f", ATTR_CURRENT_RANK, c_rank );
	ad->Insert( line.Value() );

	if( c_client ) {
		remoteUser = c_client->user();
		if( remoteUser ) {
			line.sprintf( "%s=\"%s\"", ATTR_REMOTE_USER, remoteUser );
			ad->Insert( line.Value() );
		}
		tmp = c_client->owner();
		if( tmp ) {
			line.sprintf( "%s=\"%s\"", ATTR_REMOTE_OWNER, tmp );
			ad->Insert( line.Value() );
		}
		tmp = c_client->accountingGroup();
		if( tmp ) {
			char *uidDom = NULL;
				// The accountant wants to see ATTR_ACCOUNTING_GROUP 
				// fully qualified
			if ( remoteUser ) {
				uidDom = strchr(remoteUser,'@');
			}
			if ( uidDom ) {
				line.sprintf("%s=\"%s%s\"",ATTR_ACCOUNTING_GROUP,tmp,uidDom);
			} else {
				line.sprintf("%s=\"%s\"", ATTR_ACCOUNTING_GROUP, tmp );
			}
			ad->Insert( line.Value() );
		}
		tmp = c_client->host();
		if( tmp ) {
			line.sprintf( "%s=\"%s\"", ATTR_CLIENT_MACHINE, tmp );
			ad->Insert( line.Value() );
		}
	}

	if( (c_cluster > 0) && (c_proc >= 0) ) {
		line.sprintf( "%s=\"%d.%d\"", ATTR_JOB_ID, c_cluster, c_proc );
		ad->Insert( line.Value() );
	}

	if( c_global_job_id ) {
		line.sprintf( "%s=\"%s\"", ATTR_GLOBAL_JOB_ID, c_global_job_id );
		ad->Insert( line.Value() );
	}

	if( c_job_start > 0 ) {
		line.sprintf( "%s=%d", ATTR_JOB_START, c_job_start );
		ad->Insert( line.Value() );
	}

	if( c_last_pckpt > 0 ) {
		line.sprintf( "%s=%d", ATTR_LAST_PERIODIC_CHECKPOINT, c_last_pckpt );
		ad->Insert( line.Value() );
	}

		// update ImageSize attribute from procInfo (this is
		// only for the opportunistic job, not COD)
	if( isActive() ) {
		unsigned long imgsize = imageSize();
		line.sprintf( "%s = %lu", ATTR_IMAGE_SIZE, imgsize );
		ad->Insert( line.Value() );
	}

	publishStateTimes( ad );

}


void
Claim::publishCOD( ClassAd* ad )
{
	MyString line;
	char* tmp;

	line = codId();
	line += '_';
	line += ATTR_CLAIM_ID;
	line += "=\"";
	line += id();
	line += '"';
	ad->Insert( line.Value() );

	line = codId();
	line += '_';
	line += ATTR_CLAIM_STATE;
	line += "=\"";
	line += getClaimStateString( c_state );
	line += '"';
	ad->Insert( line.Value() );

	line = codId();
	line += '_';
	line += ATTR_ENTERED_CURRENT_STATE;
	line += '=';
	line += (int)c_entered_state;
	ad->Insert( line.Value() );

	if( c_client ) {
		tmp = c_client->user();
		if( tmp ) {
			line = codId();
			line += '_';
			line += ATTR_REMOTE_USER;
			line += "=\"";
			line += tmp;
			line += '"';
			ad->Insert( line.Value() );
		}
		tmp = c_client->accountingGroup();
		if( tmp ) {
			line = codId();
			line += '_';
			line += ATTR_ACCOUNTING_GROUP;
			line += "=\"";
			line += tmp;
			line += '"';
			ad->Insert( line.Value() );
		}
		tmp = c_client->host();
		if( tmp ) {
			line = codId();
			line += '_';
			line += ATTR_CLIENT_MACHINE;
			line += "=\"";
			line += tmp;
			line += '"';
			ad->Insert( line.Value() );
		}
	}

	if( c_starter ) {
		if( c_cod_keyword ) {
			line = codId();
			line += '_';
			line += ATTR_JOB_KEYWORD;
			line += "=\"";
			line += c_cod_keyword;
			line += '"';
			ad->Insert( line.Value() );
		}
		char buf[128];
		if( (c_cluster > 0) && (c_proc >= 0) ) {
			sprintf( buf, "%d.%d", c_cluster, c_proc );
		} else {
			strcpy( buf, "1.0" );
		}
		line = codId();
		line += '_';
		line += ATTR_JOB_ID;
		line += "=\"";
		line += buf;
		line += '"';
		ad->Insert( line.Value() );
		
		if( c_job_start > 0 ) {
			line = codId();
			line += '_';
			line += ATTR_JOB_START;
			line += '=';
			line += c_job_start; 
			ad->Insert( line.Value() );
		}	
	}
}

time_t
Claim::getJobTotalRunTime()
{
	time_t my_job_run = c_job_total_run_time;
	time_t now;
	if( c_state == CLAIM_RUNNING ) { 
		now = time(NULL);
		my_job_run += now - c_entered_state;
	}
	return my_job_run;
}

void
Claim::publishStateTimes( ClassAd* ad )
{
	MyString line;
	time_t now, time_dif = 0;
	time_t my_job_run = c_job_total_run_time;
	time_t my_job_sus = c_job_total_suspend_time;
	time_t my_claim_run = c_claim_total_run_time;
	time_t my_claim_sus = c_claim_total_suspend_time;

		// If we're currently claimed or suspended, add on the time
		// we've spent in the current state, since we only increment
		// the private data members on state changes... 
	if( c_state == CLAIM_RUNNING || c_state == CLAIM_SUSPENDED ) {
		now = time( NULL );
		time_dif = now - c_entered_state;
	}
	if( c_state == CLAIM_RUNNING ) { 
		my_job_run += time_dif;
		my_claim_run += time_dif;
	}
	if( c_state == CLAIM_SUSPENDED ) {
		my_job_sus += time_dif;
		my_claim_sus += time_dif;
	}

		// Now that we have all the right values, publish them.
	if( my_job_run > 0 ) {
		line.sprintf( "%s=%d", ATTR_TOTAL_JOB_RUN_TIME, my_job_run );
		ad->Insert( line.Value() );
	}
	if( my_job_sus > 0 ) {
		line.sprintf( "%s=%d", ATTR_TOTAL_JOB_SUSPEND_TIME, my_job_sus );
		ad->Insert( line.Value() );
	}
	if( my_claim_run > 0 ) {
		line.sprintf( "%s=%d", ATTR_TOTAL_CLAIM_RUN_TIME, my_claim_run );
		ad->Insert( line.Value() );
	}
	if( my_claim_sus > 0 ) {
		line.sprintf( "%s=%d", ATTR_TOTAL_CLAIM_SUSPEND_TIME, my_claim_sus );
		ad->Insert( line.Value() );
	}
}



void
Claim::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	c_rip->dprintf_va( flags, fmt, args );
	va_end( args );
}


void
Claim::refuseClaimRequest()
{
	if( !c_request_stream ) {
		return;
	}
	dprintf( D_ALWAYS, "Refusing request to claim Resource\n" );
	c_request_stream->encode();
	c_request_stream->put(NOT_OK);
	c_request_stream->end_of_message();
}


void
Claim::start_match_timer()
{
	if( c_match_tid != -1 ) {
			/*
			  We got matched twice for the same ClaimId.  This
			  must be because we got matched, we sent an update that
			  said we're unavailable, but the collector dropped that
			  update, and we got matched again.  This shouldn't be a
			  fatal error, b/c UDP gets dropped all the time.  We just
			  need to cancel the old timer, print a warning, and then
			  continue. 
			*/
		
	   dprintf( D_FAILURE|D_ALWAYS, "Warning: got matched twice for same ClaimId."
				" Canceling old match timer (%d)\n", c_match_tid );
	   if( daemonCore->Cancel_Timer(c_match_tid) < 0 ) {
		   dprintf( D_ALWAYS, "Failed to cancel old match timer (%d): "
					"daemonCore error\n", c_match_tid );
	   } else {
		   dprintf( D_FULLDEBUG, "Cancelled old match timer (%d)\n", 
					c_match_tid );
	   }
	   c_match_tid = -1;
	}

	c_match_tid = 
		daemonCore->Register_Timer( match_timeout, 0, 
								   (TimerHandlercpp)
								   &Claim::match_timed_out,
								   "match_timed_out", this );
	if( c_match_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started match timer (%d) for %d seconds.\n", 
			 c_match_tid, match_timeout );
}


void
Claim::cancel_match_timer()
{
	int rval;
	if( c_match_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( c_match_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel match timer (%d): "
					 "daemonCore error\n", c_match_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled match timer (%d)\n", 
					 c_match_tid );
		}
		c_match_tid = -1;
	}
}


int
Claim::match_timed_out()
{
	char* my_id = id();
	if( !my_id ) {
			// We're all confused.
			// Don't use our dprintf(), use the "real" version, since
			// if we're this confused, our rip pointer might be messed
			// up, too, and we don't want to seg fault.
		::dprintf( D_FAILURE|D_ALWAYS,
				   "ERROR: Match timed out but there's no ClaimId\n" );
		return FALSE;
	}
		
	Resource* rip = resmgr->get_by_any_id( my_id );
	if( !rip ) {
		::dprintf( D_FAILURE|D_ALWAYS,
				   "ERROR: Can't find resource of expired match\n" );
		return FALSE;
	}

	if( rip->r_cur->idMatches( id() ) ) {
		if( rip->state() != matched_state ) {
				/* 
				   This used to be an EXCEPT(), since it really
				   shouldn't ever happen.  However, it kept happening,
				   and we couldn't figure out why.  For now, just log
				   it and silently ignore it, since there's no real
				   harm done, anyway.  We use D_FULLDEBUG, since we
				   don't want people to worry about it if they see it
				   in D_ALWAYS in the 6.2.X stable series.  However,
				   in the 6.3 series, we should probably try to figure 
				   out what's going on with this, for example, by
				   sending email at this point with the last 300 lines
				   of the log file or something.  -Derek 10/9/00
				*/
			dprintf( D_FAILURE|D_FULLDEBUG, 
					 "WARNING: Current match timed out but in %s state.",
					 state_to_string(rip->state()) );
			return FALSE;
		}
		delete rip->r_cur;
			// once we've done this delete, the this pointer is now in
			// a weird, invalid state.  don't rely on using any member
			// functions or data until we return.
		rip->r_cur = new Claim( rip );
		rip->dprintf( D_FAILURE|D_ALWAYS, "State change: match timed out\n" );
		rip->change_state( owner_state );
	} else {
			// The match that timed out was the preempting claim.
		assert( rip->r_pre->idMatches( id() ) );
			// We need to generate a new preempting claim object,
			// restore our reqexp, and update the CM. 
		delete rip->r_pre;
		rip->r_pre = new Claim( rip );
		rip->r_reqexp->restore();
		rip->update();
	}		
	return TRUE;
}


void
Claim::beginClaim( void ) 
{
	ASSERT( c_state == CLAIM_UNCLAIMED );
	changeState( CLAIM_IDLE );

	if( ! c_is_cod ) {
			// if we're an opportunistic claim, we want to start our
			// claim timer, too.  
		startLeaseTimer();
	}
}



void
Claim::beginActivation( time_t now )
{
		// Get a bunch of info out of the request ad that is now
		// relevant, and store it in this Claim object

		// See if the classad we got includes an ATTR_USER field,
		// so we know who to charge for our services.  If not, we use
		// the same user that claimed us.
	char* tmp = NULL;
	if( ! c_ad->LookupString(ATTR_USER, &tmp) ) {
		if( ! c_is_cod ) { 
			c_rip->dprintf( D_FULLDEBUG, "WARNING: %s not defined in "
						  "request classad!  Using old value (%s)\n", 
						  ATTR_USER, c_client->user() );
		}
	} else {
		c_rip->dprintf( D_FULLDEBUG, 
						"Got RemoteUser (%s) from request classad\n", tmp ); 
		c_client->setuser( tmp );
		free( tmp );
		tmp = NULL;
	}

		// Only stash this if it's in the ad, but don't print anything
		// if it's not.
	if( c_ad->LookupString(ATTR_ACCOUNTING_GROUP, &tmp) ) {
		c_client->setAccountingGroup( tmp );
		free( tmp );
		tmp = NULL;
	}

	c_job_start = (int)now;

		// Everything else is only going to be valid if we're not a
		// COD job.  So, if we *are* cod, just return now, since we've
		// got everything we need...
	if( c_is_cod ) {
		return;
	}

	int universe;
	if( c_ad->LookupInteger(ATTR_JOB_UNIVERSE, universe) == 0 ) {
		if( c_is_cod ) {
			universe = CONDOR_UNIVERSE_VANILLA;
		} else {
			universe = CONDOR_UNIVERSE_STANDARD;
		}
		c_rip->dprintf( D_ALWAYS, "Default universe \"%s\" (%d) "
						"since not in classad\n",
						CondorUniverseName(universe), universe );
	} else {
		c_rip->dprintf( D_ALWAYS, "Got universe \"%s\" (%d) "
						"from request classad\n",
						CondorUniverseName(universe), universe );
	}
	c_universe = universe;

	if( universe == CONDOR_UNIVERSE_STANDARD ) {
		c_last_pckpt = (int)now;
	}
}


void
Claim::setaliveint( int alive )
{
	c_aliveint = alive;
		// for now, set our lease_duration, too, just so it's
		// initalized to something reasonable.  once we get the job ad
		// we'll reset it to the real value if it's defined.
	c_lease_duration = max_claim_alives_missed * alive;
}


void
Claim::saveJobInfo( ClassAd* request_ad )
{
		// this does not make a copy, so we assume we have control
		// over the ClassAd once this method has been called.
	setad( request_ad );

	c_ad->LookupInteger( ATTR_CLUSTER_ID, c_cluster );
	c_ad->LookupInteger( ATTR_PROC_ID, c_proc );
	if( c_cluster >= 0 ) { 
			// if the cluster is set and the proc isn't, use 0.
		if( c_proc < 0 ) { 
			c_proc = 0;
		}
			// only print this if the request specified it...
		c_rip->dprintf( D_ALWAYS, "Remote job ID is %d.%d\n", 
						c_cluster, c_proc );
	}

	c_ad->LookupString( ATTR_GLOBAL_JOB_ID, &c_global_job_id );
	if( c_global_job_id ) {
			// only print this if the request specified it...
		c_rip->dprintf( D_FULLDEBUG, "Remote global job ID is %s\n", 
						c_global_job_id );
	}

		// check for an explicit job lease duration.  if it's not
		// there, we have to use the old default of 3 * aliveint. :( 
	if( c_ad->LookupInteger(ATTR_JOB_LEASE_DURATION, c_lease_duration) ) {
		dprintf( D_FULLDEBUG, "%s defined in job ClassAd: %d\n", 
				 ATTR_JOB_LEASE_DURATION, c_lease_duration );
		dprintf( D_FULLDEBUG,
				 "Resetting ClaimLease timer (%d) with new duration\n", 
				 c_lease_tid );
	} else {
		c_lease_duration = max_claim_alives_missed * c_aliveint;
		dprintf( D_FULLDEBUG, "%s not defined: using %d ("
				 "alive_interval [%d] * max_missed [%d]\n", 
				 ATTR_JOB_LEASE_DURATION, c_lease_duration,
				 c_aliveint, max_claim_alives_missed );
	}
		/* 
		   This resets the timer for us, and also, we should consider
		   a request to activate a claim (which is what just happened
		   if we're in this function) as another keep-alive...
		*/
	alive();  
}


void
Claim::startLeaseTimer()
{
	if( c_lease_duration < 0 ) {
		dprintf( D_ALWAYS, "Warning: starting ClaimLease timer before "
				 "lease duration set.\n" );
		c_lease_duration = 1200;
	}
	if( c_lease_tid != -1 ) {
	   EXCEPT( "Claim::startLeaseTimer() called w/ c_lease_tid = %d", 
			   c_lease_tid );
	}
	c_lease_tid =
		daemonCore->Register_Timer( c_lease_duration, 0, 
				(TimerHandlercpp)&Claim::leaseExpired,
				"Claim::leaseExpired", this );
	if( c_lease_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started ClaimLease timer (%d) w/ %d second "
			 "lease duration\n", c_lease_tid, c_lease_duration );
}


void
Claim::cancelLeaseTimer()
{
	int rval;
	if( c_lease_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( c_lease_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel ClaimLease timer (%d): "
					 "daemonCore error\n", c_lease_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled ClaimLease timer (%d)\n",
					 c_lease_tid );
		}
		c_lease_tid = -1;
	}
}


int
Claim::leaseExpired()
{
	Resource* rip = resmgr->get_by_cur_id( id() );
	if( !rip ) {
		EXCEPT( "Can't find resource of expired claim" );
	}
		// Note that this claim timed out so we don't try to send a 
		// command to our client.
	if( c_client ) {
		delete c_client;
		c_client = NULL;
	}

	dprintf( D_FAILURE|D_ALWAYS, "State change: claim lease expired "
			 "(condor_schedd gone?)\n" );

		// Kill the claim.
	rip->kill_claim();
	return TRUE;
}


void
Claim::alive()
{
	dprintf( D_PROTOCOL, "Keep alive for ClaimId %s\n", id() );
		// Process a keep alive command
	daemonCore->Reset_Timer( c_lease_tid, c_lease_duration, 0 );
}


// Set our ad to the given pointer
void
Claim::setad(ClassAd *ad) 
{
	if( c_ad ) {
		delete( c_ad );
	}
	c_ad = ad;
}


void
Claim::setRequestStream(Stream* stream)
{
	if( c_request_stream ) {
		delete( c_request_stream );
	}
	c_request_stream = stream;
}


char*
Claim::id( void )
{
	if( c_id ) {
		return c_id->id();
	} else {
		return NULL;
	}
}


bool
Claim::idMatches( const char* id )
{
	if( c_id ) {
		return c_id->matches( id );
	}
	return false;
}


float
Claim::percentCpuUsage( void )
{
	if( c_starter ) {
		return c_starter->percentCpuUsage();
	} else {
		return 0.0;
	}
}


unsigned long
Claim::imageSize( void )
{
	if( c_starter ) {
		return c_starter->imageSize();
	} else {
		return 0;
	}
}


CODMgr*
Claim::getCODMgr( void )
{
	if( ! c_rip ) {
		return NULL;
	}
	return c_rip->r_cod_mgr;
}

int
Claim::spawnStarter( time_t now, Stream* s )
{
	int rval;
	if( ! c_starter ) {
			// Big error!
		dprintf( D_ALWAYS, "ERROR! Claim::spawnStarter() called "
				 "w/o a Starter object! Returning failure\n" );
		return FALSE;
	}

	rval = c_starter->spawn( now, s );
	if( ! rval ) {
		resetClaim();
		return FALSE;
	}

	changeState( CLAIM_RUNNING );

		// Fake ourselves out so we take another snapshot in 15
		// seconds, once the starter has had a chance to spawn the
		// user job and the job as (hopefully) done any initial
		// forking it's going to do.  If we're planning to check more
		// often that 15 seconds, anyway, don't bother with this.
	if( pid_snapshot_interval > 15 ) {
		c_starter->set_last_snapshot( (now + 15) -
									  pid_snapshot_interval );
	} 
	return TRUE;
}


void
Claim::setStarter( Starter* s )
{
	if( s && c_starter ) {
		EXCEPT( "Claim::setStarter() called with existing starter!" );
	}
	c_starter = s;
	if( s ) {
		s->setClaim( this );
	}
}


void
Claim::starterExited( void )
{
		// Now that the starter is gone, we need to change our state
	changeState( CLAIM_IDLE );

		// Notify our starter object that its starter exited, so it
		// can cancel timers any pending timers, cleanup the starter's
		// execute directory, and do any other cleanup. 
	c_starter->exited();
	
		// Now, clear out this claim with all the starter-specific
		// info, including the starter object itself.
	resetClaim();

		// finally, let our resource know that our starter exited, so
		// it can do the right thing.
	c_rip->starterExited( this );
}


bool
Claim::starterPidMatches( pid_t starter_pid )
{
	if( c_starter && c_starter->pid() == starter_pid ) {
		return true;
	}
	return false;
}


bool
Claim::isDeactivating( void )
{
	if( c_state == CLAIM_VACATING || c_state == CLAIM_KILLING ) {
		return true;
	}
	return false;
}


bool
Claim::isActive( void )
{
	switch( c_state ) {
	case CLAIM_RUNNING:
	case CLAIM_SUSPENDED:
	case CLAIM_VACATING:
	case CLAIM_KILLING:
		return true;
		break;
	case CLAIM_IDLE:
	case CLAIM_UNCLAIMED:
		return false;
		break;
	}
	return false;
}


bool
Claim::isRunning( void )
{
	switch( c_state ) {
	case CLAIM_RUNNING:
	case CLAIM_VACATING:
	case CLAIM_KILLING:
		return true;
		break;
	case CLAIM_SUSPENDED:
	case CLAIM_IDLE:
	case CLAIM_UNCLAIMED:
		return false;
		break;
	}
	return false;
}


bool
Claim::deactivateClaim( bool graceful )
{
	if( isActive() ) {
		if( graceful ) {
			return starterKillSoft();
		} else {
			return starterKillHard();
		}
	}
		// not active, so nothing to do
	return true;
}


bool
Claim::removeClaim( bool graceful )
{
	if( isActive() ) {
		c_wants_remove = true;
		if( graceful ) {
			starterKillSoft();
		} else {
			starterKillHard();
		}
		dprintf( D_FULLDEBUG, "Removing active claim %s "
				 "(waiting for starter pid %d to exit)\n", id(), 
				 c_starter->pid() );
		return false;
	}
	dprintf( D_FULLDEBUG, "Removing inactive claim %s\n", id() );
	return true;
}


bool
Claim::suspendClaim( void )
{
	changeState( CLAIM_SUSPENDED );

	if( c_starter ) {
		return (bool)c_starter->suspend();
	}
		// if there's no starter, we don't need to do anything, so
		// it worked...  
	return true;
}


bool
Claim::resumeClaim( void )
{
	if( c_starter ) {
		changeState( CLAIM_RUNNING );
		return (bool)c_starter->resume();
	}
		// if there's no starter, we don't need to do anything, so
		// it worked...  
	changeState( CLAIM_IDLE );

	return true;
}


bool
Claim::starterKill( int sig )
{
		// don't need to work about the state, since we don't use this
		// method to send any signals that change the claim state...
	if( c_starter ) {
		return c_starter->kill( sig );
	}
		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillPg( int sig )
{
	if( c_starter ) {
			// if we're using KillPg, we're trying to hard-kill the
			// starter and all its children
		changeState( CLAIM_KILLING );
		return c_starter->killpg( sig );
	}
		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillSoft( void )
{
	if( c_starter ) {
		changeState( CLAIM_VACATING );
		return c_starter->killSoft();
	}
		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillHard( void )
{
	if( c_starter ) {
		changeState( CLAIM_KILLING );
		return c_starter->killHard();
	}
		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::makeCODStarterArgs( ArgList &args )
{
		// first deal with everthing that's shared, no matter what.
	args.AppendArg("condor_starter");
	args.AppendArg("-f");
	args.AppendArg("-append");
	args.AppendArg("cod");
	args.AppendArg("-header");

	MyString cod_id_arg;
	cod_id_arg += "(";
	if( resmgr->is_smp() ) {
		cod_id_arg += c_rip->r_id_str;
		cod_id_arg += ':';
	}
	cod_id_arg += codId();
	cod_id_arg += ")";
	args.AppendArg(cod_id_arg.Value());

		// if we've got a cluster and proc for the job, append those
	if( c_cluster >= 0 ) {
		args.AppendArg("-job-cluster");
		args.AppendArg(c_cluster);
	} 
	if( c_proc >= 0 ) {
		args.AppendArg("-job-proc");
		args.AppendArg(c_proc);
	} 

		// finally, specify how the job should get its ClassAd
	if( c_cod_keyword ) { 
		args.AppendArg("-job-keyword");
		args.AppendArg(c_cod_keyword);
	}

	if( c_has_job_ad ) { 
		args.AppendArg("-job-input-ad");
		args.AppendArg("-");
	}

	return true;
}


bool
Claim::verifyCODAttrs( ClassAd* req )
{

	if( c_cod_keyword ) {
		EXCEPT( "Trying to activate a COD claim that has a keyword" );
	}

	req->LookupString( ATTR_JOB_KEYWORD, &c_cod_keyword );
	req->EvalBool( ATTR_HAS_JOB_AD, NULL, c_has_job_ad );

	if( c_cod_keyword || c_has_job_ad ) {
		return true;
	}
	return false;
}


bool
Claim::publishStarterAd( ClassAd* ad )
{
	MyString line;
	
	if( ! c_starter ) {
		return false;
	}

	char* ip_addr = c_starter->getIpAddr();
	if( ip_addr ) {
		line = ATTR_STARTER_IP_ADDR;
		line += "=\"";
		line += ip_addr;
		line += '"';
		ad->Insert( line.Value() );
	}

		// stuff in everything we know about from the Claim object
	this->publish( ad, A_PUBLIC );

		// stuff in starter-specific attributes, if we have them.
	StringList ability_list;
	c_starter->publish( ad, A_STATIC | A_PUBLIC, &ability_list );
	char* ability_str = ability_list.print_to_string();
	if( ability_str ) {
		line = ATTR_STARTER_ABILITY_LIST;
		line += "=\"";
		line += ability_str;
		line += '"';
		ad->Insert( line.Value() );
		free( ability_str );
	}

		// TODO add more goodness to this ClassAd??

	return true;
}


bool
Claim::periodicCheckpoint( void )
{
	if( c_starter ) {
		if( ! c_starter->kill(DC_SIGPCKPT) ) { 
			return false;
		}
	}
	c_last_pckpt = (int)time(NULL);
	return true;
}


bool
Claim::ownerMatches( const char* owner )
{
	if( ! strcmp(c_client->owner(), owner) ) {
		return true;
	}
		// TODO: handle COD_SUPER_USERS
	return false;
}


bool
Claim::globalJobIdMatches( const char* id )
{
	if( c_global_job_id && !strcmp(c_global_job_id, id) ) {
		return true;
	}
	return false;
}


bool
Claim::setPendingCmd( int cmd )
{
 		// TODO: we might want to check what our currently pending
		// command is and do something special... 
	c_pending_cmd = cmd;

		// also, keep track of what state we were in when we got this
		// request, since we might want that in the reply classad. 
	c_last_state = c_state;

	return true;
}


int
Claim::finishPendingCmd( void )
{
	switch( c_pending_cmd ) {
	case -1:
		return FALSE;
		break;
	case CA_RELEASE_CLAIM:
		return finishReleaseCmd();
		break;
	case CA_DEACTIVATE_CLAIM:
		return finishDeactivateCmd();
		break;
	default:
		EXCEPT( "Claim::finishPendingCmd called with unknown cmd: %s (%d)",
				getCommandString(c_pending_cmd), c_pending_cmd );
		break;
	}
	return TRUE;
}


int
Claim::finishReleaseCmd( void )
{
	ClassAd reply;
	MyString line;
	int rval;

	line = ATTR_RESULT;
	line += " = \"";
	line += getCAResultString( CA_SUCCESS );
	line += '"';
	reply.Insert( line.Value() );

	line = ATTR_LAST_CLAIM_STATE;
	line += "=\"";
	line += getClaimStateString( c_last_state );
	line += '"';
	reply.Insert( line.Value() );
	
		// TODO: hopefully we can put the final job update ad in here,
		// too. 
	
	rval = sendCAReply( c_request_stream, "CA_RELEASE_CLAIM", &reply );
	
		// now that we're done replying, we need to delete this stream
		// so we don't leak memory, since DaemonCore's not going to do
		// that for us in this case
	delete c_request_stream;
	c_request_stream = NULL;
	c_pending_cmd = -1;
	
	dprintf( D_ALWAYS, "Finished releasing claim %s (owner: '%s')\n", 
			 id(), client()->owner() );  

	c_rip->removeClaim( this );

		// THIS OBJECT IS NOW DELETED, WE CAN *ONLY* RETURN NOW!!!
	return rval;
}


int
Claim::finishDeactivateCmd( void )
{
	ClassAd reply;
	MyString line;
	int rval;

	line = ATTR_RESULT;
	line += " = \"";
	line += getCAResultString( CA_SUCCESS );
	line += '"';
	reply.Insert( line.Value() );

	line = ATTR_LAST_CLAIM_STATE;
	line += "=\"";
	line += getClaimStateString( c_last_state );
	line += '"';
	reply.Insert( line.Value() );
	
		// TODO: hopefully we can put the final job update ad in here,
		// too. 
	
	rval = sendCAReply( c_request_stream, "CA_DEACTIVATE_CLAIM", &reply );
	
		// now that we're done replying, we need to delete this stream
		// so we don't leak memory, since DaemonCore's not going to do
		// that for us in this case
	delete c_request_stream;
	c_request_stream = NULL;
	c_pending_cmd = -1;

	dprintf( D_ALWAYS, "Finished deactivating claim %s (owner: '%s')\n", 
			 id(), client()->owner() );  

		// also, we must reset all the attributes we're storing in
		// this Claim object that are specific to a given activation. 
	resetClaim();

	return rval;
}


void
Claim::resetClaim( void )
{
	if( c_starter ) {
		delete( c_starter );
		c_starter = NULL;
	}
	if( c_ad && c_is_cod ) {
		delete( c_ad );
		c_ad = NULL;
	}
	c_universe = -1;
	c_cluster = -1;
	c_proc = -1;
	c_job_start = -1;
	c_last_pckpt = -1;
	if( c_global_job_id ) {
		free( c_global_job_id );
		c_global_job_id = NULL;
	}
	if( c_cod_keyword ) {
		free( c_cod_keyword );
		c_cod_keyword = NULL;
	}
	c_has_job_ad = 0;
	c_job_total_run_time = 0;
	c_job_total_suspend_time = 0;
	c_may_unretire = true;
	c_preempt_was_true = false;
}


void
Claim::changeState( ClaimState s )
{
	if( c_state == s ) {
		return;
	}
	
	time_t now = time(NULL);
	if( c_state == CLAIM_RUNNING || c_state == CLAIM_SUSPENDED ) {
			// the state we're leaving is one of the ones we're
			// keeping track of total times for, so we've got to
			// update some things.
		time_t time_dif = now - c_entered_state;
		if( c_state == CLAIM_RUNNING ) { 
			c_job_total_run_time += time_dif;
			c_claim_total_run_time += time_dif;
		}
		if( c_state == CLAIM_SUSPENDED ) {
			c_job_total_suspend_time += time_dif;
			c_claim_total_suspend_time += time_dif;

		}
	}
	if( c_state == CLAIM_UNCLAIMED && c_claim_started == 0 ) {
		c_claim_started = time(NULL);
	}

		// now that all the appropriate time values are updated, we
		// can actually do the deed.
	c_state = s;
	c_entered_state = now;

		// everytime a cod claim changes state, we want to update the
		// collector. 
	if( c_is_cod ) {
		c_rip->update();
	}
}

time_t
Claim::getClaimAge()
{
	time_t now = time(NULL);
	if(c_claim_started) {
		return now - c_claim_started;
	}
	return 0;
}

bool
Claim::writeJobAd( int fd )
{
	FILE* fp;
	int rval;
	fp = fdopen( fd, "w" );
	if( ! fp ) { 
		dprintf( D_ALWAYS, "Failed to open FILE* from fd %d: %s "
				 "(errno %d)\n", fd, strerror(errno), errno );
		return false;
	}

	rval = c_ad->fPrint( fp );

		// since this is really a DC pipe that we have to close with
		// Close_Pipe(), we can't call fclose() on it.  so, unless we
		// call fflush(), we won't get any output. :(
	if( fflush(fp) < 0 ) {
		dprintf( D_ALWAYS, "writeJobAd: fflush() failed: %s (errno %d)\n", 
				 strerror(errno), errno );
	}  

	return rval;
}

///////////////////////////////////////////////////////////////////////////
// Client
///////////////////////////////////////////////////////////////////////////

Client::Client()
{
	c_user = NULL;
	c_owner = NULL;
	c_acctgrp = NULL;
	c_addr = NULL;
	c_host = NULL;
}


Client::~Client() 
{
	if( c_user) free( c_user );
	if( c_owner) free( c_owner );
	if( c_acctgrp) free( c_acctgrp );
	if( c_addr) free( c_addr );
	if( c_host) free( c_host );
}


void
Client::setuser( const char* user )
{
	if( c_user ) {
		free( c_user);
	}
	if( user ) {
		c_user = strdup( user );
	} else {
		c_user = NULL;
	}
}


void
Client::setowner( const char* owner )
{
	if( c_owner ) {
		free( c_owner);
	}
	if( owner ) {
		c_owner = strdup( owner );
	} else {
		c_owner = NULL;
	}
}


void
Client::setAccountingGroup( const char* grp ) 
{
	if( c_acctgrp ) {
		free( c_acctgrp);
	}
	if( grp ) {
		c_acctgrp = strdup( grp );
	} else {
		c_acctgrp = NULL;
	}
}


void
Client::setaddr( const char* addr )
{
	if( c_addr ) {
		free( c_addr);
	}
	if( addr ) {
		c_addr = strdup( addr );
	} else {
		c_addr = NULL;
	}
}


void
Client::sethost( const char* host )
{
	if( c_host ) {
		free( c_host);
	}
	if( host ) {
		c_host = strdup( host );
	} else {
		c_host = NULL;
	}
}


void
Client::vacate(char* id)
{
	ReliSock* sock;

	if( ! (c_addr || c_host || c_owner ) ) {
			// Client not really set, nothing to do.
		return;
	}

	dprintf(D_FULLDEBUG, "Entered vacate_client %s %s...\n", c_addr, c_host);

	Daemon my_schedd( DT_SCHEDD, c_addr, NULL);
	sock = (ReliSock*)my_schedd.startCommand( RELEASE_CLAIM,
											  Stream::reli_sock, 20 );
	if( ! sock ) {
		dprintf(D_FAILURE|D_ALWAYS, "Can't connect to schedd (%s)\n", c_addr);
		return;
	}
	if( !sock->put( id ) ) {
		dprintf(D_ALWAYS, "Can't send ClaimId to client\n");
	} else if( !sock->eom() ) {
		dprintf(D_ALWAYS, "Can't send EOM to client\n");
	}

	sock->close();
	delete sock;
}


///////////////////////////////////////////////////////////////////////////
// ClaimId
///////////////////////////////////////////////////////////////////////////

static 
int
newIdString( char** id_str_ptr )
{
		// ClaimId string is of the form:
		// "<ip:port>#startd_bday#sequence_num"

	static int sequence_num = 0;
	sequence_num++;

	MyString id;
	id += daemonCore->InfoCommandSinfulString();
	id += '#';
	id += (int)startd_startup;
	id += '#';
	id += sequence_num;
	*id_str_ptr = strdup( id.Value() );
	return sequence_num;
}


ClaimId::ClaimId( bool is_cod )
{
	int num = newIdString( &c_id );
	if( is_cod ) { 
		char buf[64];
		sprintf( buf, "COD%d", num );
		c_cod_id = strdup( buf );
	} else {
		c_cod_id = NULL;
	}
}


ClaimId::~ClaimId()
{
	free( c_id );
	if( c_cod_id ) {
		free( c_cod_id );
	}
}


bool
ClaimId::matches( const char* id )
{
	return( strcmp(id, c_id) == 0 );
}


void
ClaimId::dropFile( int vm_id )
{
	if( ! param_boolean("STARTD_SHOULD_WRITE_CLAIM_ID_FILE", true) ) {
		return;
	}
	char* filename = startdClaimIdFile( vm_id );  
	if( ! filename ) {
		dprintf( D_ALWAYS, "Error getting claim id filename, not writing\n" );
		return;
	}

	MyString filename_old = filename;
	MyString filename_new = filename;
	free( filename );
	filename = NULL;

	filename_new += ".new";

	FILE* NEW_FILE = fopen( filename_new.Value(), "w" );
	if( ! NEW_FILE ) {
		dprintf( D_ALWAYS,
				 "ERROR: can't open claim id file: %s: %s (errno: %d)\n",
				 filename_new.Value(), strerror(errno), errno );
 		return;
	}
	fprintf( NEW_FILE, "%s\n", c_id );
	fclose( NEW_FILE );
	if( rotate_file(filename_new.Value(), filename_old.Value()) < 0 ) {
		dprintf( D_ALWAYS, "ERROR: failed to move %s into place, removing\n",
				 filename_new.Value() );
		unlink( filename_new.Value() );
	}
}
