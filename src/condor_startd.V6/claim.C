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
	c_cap = new Capability( is_cod );
	c_ad = NULL;
	c_starter = NULL;
	c_rank = 0;
	c_oldrank = 0;
	c_universe = -1;
	c_request_stream = NULL;
	c_match_tid = -1;
	c_claim_tid = -1;
	c_aliveint = -1;
	c_cluster = -1;
	c_proc = -1;
	c_job_start = -1;
	c_last_pckpt = -1;
	c_rip = rip;
	c_is_cod = is_cod;
	c_cod_keyword = NULL;
	c_has_job_ad = 0;
	c_pending_cmd = -1;
	c_wants_remove = false;
		// to make purify happy, we want to initialize this to
		// something.  however, we immediately set it to UNCLAIMED
		// (which is what it should really be) by using changeState()
		// so we get all the nice functionality that method provides.
	c_state = CLAIM_IDLE;
	changeState( CLAIM_UNCLAIMED );
}


Claim::~Claim()
{	
	if( c_is_cod ) {
		dprintf( D_FULLDEBUG, "Deleted claim %s (owner '%s')\n", 
				 c_cap->id(), 
				 c_client->owner() ? c_client->owner() : "unknown" );  
	}

		// Cancel any timers associated with this claim
	this->cancel_match_timer();
	this->cancel_claim_timer();

		// Free up memory that's been allocated
	if( c_ad ) {
		delete( c_ad );
	}
	delete( c_cap );
	if( c_client ) {
		delete( c_client );
	}
	if( c_request_stream ) {
		delete( c_request_stream );
	}
	if( c_starter ) {
		delete( c_starter );
	}
	if( c_cod_keyword ) {
		free( c_cod_keyword );
	}
}	
	

void
Claim::vacate() 
{
	assert( c_cap );
		// warn the client of this claim that it's being vacated
	if( c_client && c_client->addr() ) {
		c_client->vacate( c_cap->capab() );
	}
}


void
Claim::publish( ClassAd* ad, amask_t how_much )
{
	char line[256];
	char* tmp;

	if( IS_PRIVATE(how_much) ) {
		return;
	}

	sprintf( line, "%s = %f", ATTR_CURRENT_RANK, c_rank );
	ad->Insert( line );

	if( c_client ) {
		tmp = c_client->user();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_REMOTE_USER, tmp );
			ad->Insert( line );
		}
		tmp = c_client->owner();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_REMOTE_OWNER, tmp );
			ad->Insert( line );
		}
		tmp = c_client->host();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_CLIENT_MACHINE, tmp );
			ad->Insert( line );
		}
	}

	if( (c_cluster > 0) && (c_proc >= 0) ) {
		sprintf( line, "%s=\"%d.%d\"", ATTR_JOB_ID, c_cluster, c_proc );
		ad->Insert( line );
	}

	if( c_job_start > 0 ) {
		sprintf(line, "%s=%d", ATTR_JOB_START, c_job_start );
		ad->Insert( line );
	}

	if( c_last_pckpt > 0 ) {
		sprintf(line, "%s=%d", ATTR_LAST_PERIODIC_CHECKPOINT, c_last_pckpt );
		ad->Insert( line );
	}
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
			  We got matched twice for the same capability.  This
			  must be because we got matched, we sent an update that
			  said we're unavailable, but the collector dropped that
			  update, and we got matched again.  This shouldn't be a
			  fatal error, b/c UDP gets dropped all the time.  We just
			  need to cancel the old timer, print a warning, and then
			  continue. 
			*/
		
	   dprintf( D_FAILURE|D_ALWAYS, "Warning: got matched twice for same capability."
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
	char* my_cap = capab();
	if( !my_cap ) {
			// We're all confused.
			// Don't use our dprintf(), use the "real" version, since
			// if we're this confused, our rip pointer might be messed
			// up, too, and we don't want to seg fault.
		::dprintf( D_FAILURE|D_ALWAYS,
				   "ERROR: Match timed out but there's no capability\n" );
		return FALSE;
	}
		
	Resource* rip = resmgr->get_by_any_cap( my_cap );
	if( !rip ) {
		::dprintf( D_FAILURE|D_ALWAYS,
				   "ERROR: Can't find resource of expired match\n" );
		return FALSE;
	}

	if( rip->r_cur->cap()->matches( capab() ) ) {
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
		assert( rip->r_pre->cap()->matches( capab() ) );
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
		start_claim_timer();
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
	char* remote_user = NULL;
	if( ! c_ad->LookupString(ATTR_USER, &remote_user) ) {
		if( ! c_is_cod ) { 
			c_rip->dprintf( D_FULLDEBUG, "WARNING: %s not defined in "
						  "request classad!  Using old value (%s)\n", 
						  ATTR_USER, c_client->user() );
		}
	} else {
		c_rip->dprintf( D_FULLDEBUG, 
					  "Got RemoteUser (%s) from request classad\n",	
					  remote_user );
		c_client->setuser( remote_user );
	}

	c_job_start = (int)now;

		// Everything else is only going to be valid if we're not a
		// COD job.  So, if we *are* cod, just return now, since we've
		// got everything we need...
	if( c_is_cod ) {
		if (remote_user != NULL) {
			free(remote_user);
		}
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

	if (remote_user != NULL) {
		free(remote_user);
	}
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
}


void
Claim::start_claim_timer()
{
	if( c_aliveint < 0 ) {
		dprintf( D_ALWAYS, 
				 "Warning: starting claim timer before alive interval set.\n" );
		c_aliveint = 300;
	}
	if( c_claim_tid != -1 ) {
	   EXCEPT( "Claim::start_claim_timer() called w/ c_claim_tid = %d", 
			   c_claim_tid );
	}
	c_claim_tid =
		daemonCore->Register_Timer( (max_claim_alives_missed * c_aliveint),
				0, (TimerHandlercpp)&Claim::claim_timed_out,
				"claim_timed_out", this );
	if( c_claim_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started claim timer (%d) w/ %d second "
			 "alive interval.\n", c_claim_tid, c_aliveint );
}


void
Claim::cancel_claim_timer()
{
	int rval;
	if( c_claim_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( c_claim_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel claim timer (%d): "
					 "daemonCore error\n", c_claim_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled claim timer (%d)\n",
					 c_claim_tid );
		}
		c_claim_tid = -1;
	}
}


int
Claim::claim_timed_out()
{
	Resource* rip = resmgr->get_by_cur_cap( capab() );
	if( !rip ) {
		EXCEPT( "Can't find resource of expired claim." );
	}
		// Note that this claim timed out so we don't try to send a 
		// command to our client.
	if( c_client ) {
		delete c_client;
		c_client = NULL;
	}

	dprintf( D_FAILURE|D_ALWAYS, "State change: claim timed out (condor_schedd gone?)\n" );

		// Kill the claim.
	rip->kill_claim();
	return TRUE;
}


void
Claim::alive()
{
	dprintf( D_PROTOCOL, "Keep alive for ClaimId %s\n", id() );
		// Process a keep alive command
	daemonCore->Reset_Timer( c_claim_tid,
							 (max_claim_alives_missed * c_aliveint), 0 );
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
	return capab();
}


char*
Claim::capab( void )
{
	if( c_cap ) {
		return c_cap->id();
	} else {
		return NULL;
	}
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


char* 
Claim::makeCODStarterArgs( void )
{
	MyString args;

		// first deal with everthing that's shared, no matter what.
	args = "condor_starter -f -append cod ";
	args += "-header (";
	if( resmgr->is_smp() ) {
		args += c_rip->r_id_str;
		args += ':';
	}
	args += codId();
	args += ") ";

		// if we've got a cluster and proc for the job, append those
	if( c_cluster >= 0 ) {
		args += " -job-cluster ";
		args += c_cluster;
	} 
	if( c_proc >= 0 ) {
		args += " -job-proc ";
		args += c_proc;
	} 

		// finally, specify how the job should get its ClassAd
	if( c_cod_keyword ) { 
		args += " -job-keyword ";
		args += c_cod_keyword;
	}

	if( c_has_job_ad ) { 
		args += " -job-input-ad - ";
	}

	return strdup( args.Value() );
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
	if( c_cod_keyword ) {
		free( c_cod_keyword );
		c_cod_keyword = NULL;
	}
	c_has_job_ad = 0;
}


void
Claim::changeState( ClaimState s )
{
	if( c_state == s ) {
		return;
	}
	c_state = s;
	c_entered_state = time(NULL); 
		// everytime a cod claim changes state, we want to update the
		// collector. 
	if( c_is_cod ) {
		c_rip->update();
	}
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
	c_addr = NULL;
	c_host = NULL;
}


Client::~Client() 
{
	if( c_user) free( c_user );
	if( c_owner) free( c_owner );
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
Client::vacate(char* cap)
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
	if( !sock->put( cap ) ) {
		dprintf(D_ALWAYS, "Can't send capability to client\n");
	} else if( !sock->eom() ) {
		dprintf(D_ALWAYS, "Can't send EOM to client\n");
	}

	sock->close();
	delete sock;
}


///////////////////////////////////////////////////////////////////////////
// Capability
///////////////////////////////////////////////////////////////////////////

char*
newCapabilityString()
{
	char cap[128];
	char tmp[128];
	char randbuf[12];
	randbuf[0] = '\0';
	int i, len;

		// Create a really mangled 10 digit random number: The first 6
		// digits are generated as follows: for the ith digit, pull
		// the ith digit off a new random int.  So our 1st slot comes
		// from the 1st digit of 1 random int, the 2nd from the 2nd
		// digit of a 2nd random it, etc...  If we're trying to get a
		// digit from a number that's too small to have that many, we
		// just use the last digit.  The last 4 digits of our number
		// come from the first 4 digits of the current time multiplied
		// by a final random int.  That should keep 'em guessing. :)
		// -Derek Wright 1/8/98
	for( i=0; i<6; i++ ) {
		sprintf( tmp, "%d", get_random_int() );
		len = strlen(tmp);
		if( i < len ) {
			tmp[i+1] = '\0';
			strcat( randbuf, tmp+i );
		} else {
			strcat( randbuf, tmp+(len-1) );
		}
	}
	sprintf( tmp, "%f", (double)((float)time(NULL) * (float)get_random_int()) );
	tmp[4]='\0';
	strcat( randbuf, tmp );

		// Capability string is "<ip:port>#random_number"
	strcpy( cap, daemonCore->InfoCommandSinfulString() );
	strcat( cap, "#" );
	strcat( cap, randbuf );
	return strdup( cap );
}



int
newCODIdString( char** id_str_ptr )
{
		// COD id string (capability) is of the form:
		// "<ip:port>#COD#startd_bday#sequence_num"

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


Capability::Capability( bool is_cod )
{

	if( is_cod ) { 
		int num = newCODIdString( &c_id );
		char buf[64];
		sprintf( buf, "COD%d", num );
		c_cod_id = strdup( buf );
	} else {
		c_id = newCapabilityString();
		c_cod_id = NULL;
	}
}


Capability::~Capability()
{
	free( c_id );
	if( c_cod_id ) {
		free( c_cod_id );
	}
}


bool
Capability::matches( const char* capab )
{
	return( strcmp(capab, c_id) == 0 );
}

