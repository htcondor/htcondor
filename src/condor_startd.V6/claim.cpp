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
#include "condor_crypt.h"
#include "dc_schedd.h"

// for startdClaimIdFile
#include "misc_utils.h"

// for starter exit codes
#include "exit.h"

///////////////////////////////////////////////////////////////////////////
// Claim
///////////////////////////////////////////////////////////////////////////

Claim::Claim( Resource* res_ip, ClaimType claim_type, int lease_duration )
	: c_rip(res_ip)
	, c_client(NULL)
	, c_id(0)
	, c_type(claim_type)
	, c_jobad(NULL)
	, c_starter_pid(0)
	, c_rank(0)
	, c_oldrank(0)
	, c_universe(-1)
	, c_cluster(-1)
	, c_proc(-1)
	, c_global_job_id(NULL)
	, c_job_start(-1)
	, c_last_pckpt(0)
	, c_claim_started(0)
	, c_entered_state(0)
	, c_job_total_run_time(0)
	, c_job_total_suspend_time(0)
	, c_claim_total_run_time(0)
	, c_claim_total_suspend_time(0)
	, c_activation_count(0)
	, c_request_stream(NULL)
	, c_match_tid(-1)
	, c_lease_tid(-1)
	, c_sendalive_tid(-1)
	, c_alive_inprogress_sock(NULL)
	, c_lease_duration(lease_duration)
	, c_aliveint(-1)
	, c_lease_endtime(0)
	, c_starter_handles_alives(false)
	, c_startd_sends_alives(false)
	, c_cod_keyword(NULL)
	, c_has_job_ad(false)
	, c_state(CLAIM_IDLE)
	, c_last_state(CLAIM_UNCLAIMED)
	, c_pending_cmd(-1)
	, c_wants_remove(false)
	, c_may_unretire(true)
	, c_retire_peacefully(false)
	, c_preempt_was_true(false)
	, c_badput_caused_by_draining(false)
	, c_badput_caused_by_preemption(false)
	, c_schedd_closed_claim(false)
	, c_pledged_machine_max_vacate_time(0)
	, c_cpus_usage(0)
	, c_image_size(0)
	, c_want_matching(true)
{
	//dprintf(D_ALWAYS | D_BACKTRACE, "constructing claim %p on resource %p\n", this, res_ip);

	c_client = new Client;
	c_id = new ClaimId( claim_type, res_ip->r_id_str );
	if( claim_type == CLAIM_OPPORTUNISTIC ) {
		c_id->dropFile( res_ip->r_id );
	}
		// to make purify happy, we want to initialize this to
		// something.  however, we immediately set it to UNCLAIMED
		// (which is what it should really be) by using changeState()
		// so we get all the nice functionality that method provides.
	c_state = CLAIM_IDLE;
	changeState( CLAIM_UNCLAIMED );
	c_last_state = CLAIM_UNCLAIMED;
}


Claim::~Claim()
{
	if( c_type == CLAIM_COD ) {
		dprintf( D_FULLDEBUG, "Deleted claim %s (owner '%s')\n",
				 c_id->id(),
				 c_client->c_owner.c_str() );
	}

	// The resources assigned to this claim must have been freed by now.
	if( c_rip != NULL && c_rip->r_classad != NULL ) {
		resmgr->adlist_unset_monitors( c_rip->r_id, c_rip->r_classad );
	} else {
		dprintf( D_ALWAYS, "Unable to unset monitors in claim destructor.  The StartOfJob* attributes will be stale.  (%p, %p)\n", c_rip, c_rip == NULL ? NULL : c_rip->r_classad );
	}

		// Cancel any daemonCore events associated with this claim
	this->cancel_match_timer();
	this->cancelLeaseTimer();
	if ( c_alive_inprogress_sock ) {
		daemonCore->Cancel_Socket(c_alive_inprogress_sock);
		c_alive_inprogress_sock = NULL;
	}

	// if we were associated with a starter, then we need to check to see if that starter was reaped
	// before we can delete the job ad.
	if (c_starter_pid && c_jobad) {
		Starter * starter = findStarterByPid(c_starter_pid);
		if (starter && starter->notYetReaped()) {
			dprintf(D_ALWAYS, "Deleting claim while starter is still alive. The STARTD history for job %d.%d may be incomplete\n", c_cluster, c_proc);

			// update stat for JobBusyTime
			if (c_job_start > 0) {
				double busyTime = condor_gettimestamp_double() - c_job_start;
				resmgr->startd_stats.job_busy_time += busyTime;
			}

			// Transfer ownership of our jobad to the starter so it can write a correct history entry.
			starter->setOrphanedJob(c_jobad);
			c_jobad = NULL;
		}
	}

		// Free up memory that's been allocated
	if (c_jobad) {
		delete(c_jobad);
		c_jobad = NULL;
	}
	delete( c_id );
	if( c_client ) {
		delete( c_client );
	}
		// delete the request stream and do any necessary related cleanup
	setRequestStream( NULL );

	if( c_global_job_id ) { 
		free( c_global_job_id );
	}
	if( c_cod_keyword ) {
		free( c_cod_keyword );
	}
}	

void
Claim::scheddClosedClaim() {
		// This tells us that there is no need to send RELEASE_CLAIM
		// to the schedd, because it was the schedd that told _us_
		// to close the claim.
	c_schedd_closed_claim = true;
}	

void
Claim::vacate() 
{
	ASSERT( c_id );
		// warn the client of this claim that it's being vacated
	if( c_client && !c_client->c_addr.empty() && !c_schedd_closed_claim ) {
		dprintf(D_FULLDEBUG, "Entered vacate_client %s %s...\n",
		        c_client->c_addr.c_str(), c_client->c_host.c_str());

		Daemon my_schedd( DT_SCHEDD, c_client->c_addr.c_str(), NULL);
		Sock* sock = my_schedd.startCommand(RELEASE_CLAIM, Stream::reli_sock,
		                                    20, nullptr, nullptr, false,
		                                    c_id->secSessionId());
		if( ! sock ) {
			dprintf(D_ERROR, "Can't connect to schedd (%s)\n",
			        c_client->c_addr.c_str());
		} else {
			if( !sock->put_secret(c_id->id()) ) {
				dprintf(D_ALWAYS, "Can't send ClaimId to client\n");
			} else if( !sock->end_of_message() ) {
				dprintf(D_ALWAYS, "Can't send EOM to client\n");
			}

			sock->close();
			delete sock;
		}
	}

#if HAVE_JOB_HOOKS
	if ((c_type == CLAIM_FETCH) && c_has_job_ad) {
		resmgr->m_hook_mgr->hookEvictClaim(c_rip);
	}
#endif /* HAVE_JOB_HOOKS */

}


void
Claim::publish( ClassAd* cad )
{
	std::string line;

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

	cad->Assign( ATTR_CURRENT_RANK, c_rank );

	if( c_client ) {
		if (!c_client->c_scheddName.empty()) {
			cad->Assign(ATTR_REMOTE_SCHEDD_NAME, c_client->c_scheddName);
		}
		if( !c_client->c_user.empty() ) {
			cad->Assign(ATTR_REMOTE_USER, c_client->c_user);
		}
		if( !c_client->c_owner.empty() ) {
			cad->Assign(ATTR_REMOTE_OWNER, c_client->c_owner);
		}
		if( !c_client->c_acctgrp.empty() ) {
			const char *uidDom = NULL;
				// The accountant wants to see ATTR_ACCOUNTING_GROUP 
				// fully qualified
			if ( !c_client->c_user.empty() ) {
				uidDom = strchr(c_client->c_user.c_str(), '@');
			}
			line = c_client->c_acctgrp;
			if ( uidDom ) {
				line += uidDom;
			}
			cad->Assign( ATTR_ACCOUNTING_GROUP, line );
		}
		if( !c_client->c_host.empty() ) {
			cad->Assign(ATTR_CLIENT_MACHINE, c_client->c_host);
		}

		if ( !c_client->c_concurrencyLimits.empty() ) {
			cad->Assign(ATTR_CONCURRENCY_LIMITS, c_client->c_concurrencyLimits);
		}

		cad->Assign(ATTR_NUM_PIDS, c_client->c_numPids);

		if ( !c_client->c_rmtgrp.empty() ) {
			cad->Assign(ATTR_REMOTE_GROUP, c_client->c_rmtgrp);
		}
		if ( !c_client->c_neggrp.empty() ) {
			cad->Assign(ATTR_REMOTE_NEGOTIATING_GROUP, c_client->c_neggrp);
			cad->Assign(ATTR_REMOTE_AUTOREGROUP, c_client->c_autorg);
		}
		if (c_client->c_cgroupActive) {
			cad->Assign(ATTR_CGROUP_ENFORCED, true);
		}
	}

	if( (c_cluster > 0) && (c_proc >= 0) ) {
		formatstr( line, "%d.%d", c_cluster, c_proc );
		cad->Assign( ATTR_JOB_ID, line );
	}

	if( c_global_job_id ) {
		cad->Assign( ATTR_GLOBAL_JOB_ID, c_global_job_id );
	}

	if( c_job_start > 0 ) {
		// The "JobStart" attribute is traditionally an integer, so we truncate the time to an int for this assignment.
		cad->Assign(ATTR_JOB_START, (time_t)c_job_start);
	}

	if( c_last_pckpt > 0 ) {
		cad->Assign( ATTR_LAST_PERIODIC_CHECKPOINT, c_last_pckpt );
	}

		// update ImageSize attribute from procInfo (this is
		// only for the opportunistic job, not COD)
	if( isActive() ) {
		// put the image size value from the last call to updateUsage into the ad.
		cad->Assign(ATTR_IMAGE_SIZE, c_image_size);
		// also the CpusUsage value
		cad->Assign(ATTR_CPUS_USAGE, c_cpus_usage);
	}

	// If this claim is for vm universe, update some info about VM
	if (c_starter_pid > 0) {
		resmgr->m_vmuniverse_mgr.publishVMInfo(c_starter_pid, cad);
	}

	if (!c_working_cm.empty()) {
		cad->Assign("WorkingCM", c_working_cm);
	}

	if (c_rip->is_partitionable_slot() && c_state != CLAIM_UNCLAIMED) {
		cad->Assign(ATTR_WANT_MATCHING, c_want_matching);
	}

	publishStateTimes( cad );

}

void
Claim::publishPreemptingClaim( ClassAd* cad )
{
	std::string line;

	if( c_client && !c_client->c_user.empty() ) {
		cad->Assign( ATTR_PREEMPTING_RANK, c_rank );

		if( !c_client->c_user.empty() ) {
			cad->Assign(ATTR_PREEMPTING_USER, c_client->c_user);
		}
		if( !c_client->c_owner.empty() ) {
			cad->Assign(ATTR_PREEMPTING_OWNER, c_client->c_owner);
		}
		if( !c_client->c_acctgrp.empty() ) {
			const char *uidDom = NULL;
				// The accountant wants to see ATTR_ACCOUNTING_GROUP 
				// fully qualified
			if ( !c_client->c_user.empty() ) {
				uidDom = strchr(c_client->c_user.c_str(), '@');
			}
			line = c_client->c_acctgrp;
			if ( uidDom ) {
				line += uidDom;
			}
			cad->Assign( ATTR_PREEMPTING_ACCOUNTING_GROUP, line );
		}

		if ( !c_client->c_concurrencyLimits.empty() ) {
			cad->Assign(ATTR_PREEMPTING_CONCURRENCY_LIMITS, c_client->c_concurrencyLimits);
		}
	}
	else {
		cad->Delete(ATTR_PREEMPTING_RANK);
		cad->Delete(ATTR_PREEMPTING_OWNER);
		cad->Delete(ATTR_PREEMPTING_USER);
		cad->Delete(ATTR_PREEMPTING_ACCOUNTING_GROUP);
	}
}


void
Claim::publishCOD( ClassAd* cad )
{
	std::string prefix;
	std::string attrn;
	const char* tmp;

	tmp = codId();
	if ( tmp ) {
		prefix = tmp;
	}
	prefix += '_';

	attrn = prefix + ATTR_CLAIM_ID;
	cad->Assign( attrn, id() );

	attrn = prefix + ATTR_CLAIM_STATE;
	cad->Assign( attrn, getClaimStateString( c_state ) );

	attrn = prefix + ATTR_ENTERED_CURRENT_STATE;
	cad->Assign( attrn, c_entered_state );

	if( c_client ) {
		if( !c_client->c_user.empty() ) {
			attrn = prefix + ATTR_REMOTE_USER;
			cad->Assign(attrn, c_client->c_user);
		}
		if( !c_client->c_acctgrp.empty() ) {
			attrn = prefix + ATTR_ACCOUNTING_GROUP;
			cad->Assign(attrn, c_client->c_acctgrp);
		}
		if( !c_client->c_host.empty() ) {
			attrn = prefix + ATTR_CLIENT_MACHINE;
			cad->Assign( attrn, c_client->c_host);
		}
	}

	if( c_starter_pid )
	{
		if( c_cod_keyword ) {
			attrn = prefix + ATTR_JOB_KEYWORD;
			cad->Assign( attrn, c_cod_keyword );
		}
		char buf[128];
		if( (c_cluster > 0) && (c_proc >= 0) ) {
			snprintf( buf, 128, "%d.%d", c_cluster, c_proc );
		} else {
			strcpy( buf, "1.0" );
		}
		attrn = prefix + ATTR_JOB_ID;
		cad->Assign( attrn, buf );
		
		if( c_job_start > 0 ) {
			attrn = prefix + ATTR_JOB_START;
			cad->Assign( attrn, (time_t)c_job_start );
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
Claim::publishStateTimes( ClassAd* cad )
{
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
		cad->Assign( ATTR_TOTAL_JOB_RUN_TIME, my_job_run );
	}
	if( my_job_sus > 0 ) {
		cad->Assign( ATTR_TOTAL_JOB_SUSPEND_TIME, my_job_sus );
	}
	if( my_claim_run > 0 ) {
		cad->Assign( ATTR_TOTAL_CLAIM_RUN_TIME, my_claim_run );
	}
	if( my_claim_sus > 0 ) {
		cad->Assign( ATTR_TOTAL_CLAIM_SUSPEND_TIME, my_claim_sus );
	}
}



void
Claim::dprintf( int flags, const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	if (c_rip) {
		c_rip->dprintf_va( flags, fmt, args );
	} else {
		const DPF_IDENT ident = 0; // REMIND: maybe something useful here??
		::_condor_dprintf_va( flags, ident, fmt, args );
	}
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
		
	   dprintf( D_ERROR, "Warning: got matched twice for same ClaimId."
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


void
Claim::match_timed_out( int /* timerID */ )
{
	char* my_id = id();
	if( !my_id ) {
			// We're all confused.
			// Don't use our dprintf(), use the "real" version, since
			// if we're this confused, our rip pointer might be messed
			// up, too, and we don't want to seg fault.
		::dprintf( D_ERROR,
				   "ERROR: Match timed out but there's no ClaimId\n" );
		return;
	}
		
	Resource* res_ip = resmgr->get_by_any_id( my_id );
	if( !res_ip ) {
		::dprintf( D_ERROR,
				   "ERROR: Can't find resource of expired match\n" );
		return;
	}

	if( res_ip->r_cur->idMatches( id() ) ) {
#if HAVE_BACKFILL
		if( res_ip->state() == backfill_state ) {
				/*
				  If the resource is in the backfill state when the
				  match timed out, it means that the backfill job is
				  taking longer to exit than the match timeout, which
				  should be an extremely rare case.  However, if it
				  happens, we need to handle it.  Luckily, all we have
				  to do is change our destination state and return,
				  since the ResState code will deal with resetting the
				  claim objects once we hit the owner state...
				*/
			dprintf( D_ERROR, "WARNING: Match timed out "
					 "while still in the %s state. This might mean "
					 "your MATCH_TIMEOUT setting (%d) is too small, "
					 "or that there's a problem with how quickly your "
					 "backfill jobs can evict themselves.\n", 
					 state_to_string(res_ip->state()), match_timeout );
			res_ip->set_destination_state( owner_state );
			return;
		}
#endif /* HAVE_BACKFILL */
		if( res_ip->state() != matched_state ) {
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
			dprintf( D_ERROR | D_VERBOSE,
					 "WARNING: Current match timed out but in %s state.\n",
					 state_to_string(res_ip->state()) );
			return;
		}
		delete res_ip->r_cur;
			// once we've done this delete, the this pointer is now in
			// a weird, invalid state.  don't rely on using any member
			// functions or data until we return.
		res_ip->r_cur = new Claim( res_ip );
		res_ip->dprintf( D_ERROR, "State change: match timed out\n" );
		res_ip->change_state( owner_state );
	} else {
			// The match that timed out was the preempting claim.
		Claim *c = NULL;
		if( res_ip->r_pre && res_ip->r_pre->idMatches( id() ) ) {
			c = res_ip->r_pre;
		}
		else if( res_ip->r_pre_pre && res_ip->r_pre_pre->idMatches( id() ) ) {
			c = res_ip->r_pre_pre;
		}
		else {
			EXCEPT("Unexpected timeout of claim id: %s",id());
		}
			// We need to generate a new preempting claim object,
			// restore our reqexp, and update the CM. 
		res_ip->removeClaim( c );
		res_ip->reqexp_restore();
		res_ip->update_needed(Resource::WhyFor::wf_removeClaim);
	}		
	return;
}


void
Claim::beginClaim( void ) 
{
	ASSERT( c_state == CLAIM_UNCLAIMED );
	changeState( CLAIM_IDLE );

	startLeaseTimer();
}

/** Copy info about the client and resource request from another claim object.
    Used when claiming multiple slots in a single request_claim call. Note that
    the job ad at this point is normally a resource request not ad, not an activation ad
*/
void
Claim::copyClientInfo(const Claim & that)
{
	if (c_jobad) { delete(c_jobad); c_jobad = nullptr; }
	if (that.c_jobad) { c_jobad = new ClassAd(*that.c_jobad); }

	c_rank = that.c_rank;
	c_oldrank = that.c_oldrank;

	if (that.c_client) {
		if ( ! c_client) c_client = new Client();
		*c_client = *that.c_client;
		c_client->c_numPids = 0;
	}
}

void
Claim::loadAccountingInfo()
{
		// Get a bunch of info out of the request ad that is now
		// relevant, and store it in this Claim object
		// TODO This code appears to update incorrectly if a claim is
		//   reused for a job with a different owner or accounting group.

		// See if the classad we got includes an ATTR_USER field,
		// so we know who to charge for our services.  If not, we use
		// the same user that claimed us.
	if( ! c_jobad->LookupString(ATTR_USER, c_client->c_user) ) {
		if( c_type != CLAIM_COD ) { 
			c_rip->dprintf( D_FULLDEBUG, "WARNING: %s not defined in "
						  "request classad!  Using old value (%s)\n", 
						  ATTR_USER, c_client->c_user.c_str() );
		}
	} else {
		c_rip->dprintf( D_FULLDEBUG,
		                "Got RemoteUser (%s) from request classad\n",
		                c_client->c_user.c_str() );
	}

		// Only stash this if it's in the ad, but don't print anything
		// if it's not.
	c_jobad->LookupString(ATTR_ACCOUNTING_GROUP, c_client->c_acctgrp);

	if(c_client->c_owner.empty()) {
			// Only if Owner has never been initialized, load it now.
		c_jobad->LookupString(ATTR_OWNER, c_client->c_owner);
	}
}

void
Claim::loadRequestInfo()
{
		// Stash the ATTR_CONCURRENCY_LIMITS, necessary to advertise
		// them if they exist
	std::string limits;
	(void) EvalString(ATTR_CONCURRENCY_LIMITS, c_jobad, c_rip->r_classad, limits);
	if (!limits.empty()) {
		c_client->c_concurrencyLimits = limits;
	}

	// stash information about what accounting group match was negotiated under
	c_jobad->LookupString(ATTR_REMOTE_GROUP, c_client->c_rmtgrp);
	c_jobad->LookupString(ATTR_REMOTE_NEGOTIATING_GROUP, c_client->c_neggrp);
	c_jobad->LookupBool(ATTR_REMOTE_AUTOREGROUP, c_client->c_autorg);
}

void
Claim::loadStatistics()
{
		// Stash the ATTR_NUM_PIDS, necessary to advertise
		// them if they exist
	if ( c_client ) {
		c_client->c_numPids = 0;
		c_jobad->LookupInteger(ATTR_NUM_PIDS, c_client->c_numPids);
		c_jobad->LookupBool(ATTR_CGROUP_ENFORCED, c_client->c_cgroupActive);
	}
}

void
Claim::beginActivation( double now )
{
	loadAccountingInfo();

	c_activation_count += 1;

	c_job_start = now;

	c_pledged_machine_max_vacate_time = 0;
	if(c_rip->r_classad->LookupExpr(ATTR_MACHINE_MAX_VACATE_TIME)) {
		if( !EvalInteger(
			ATTR_MACHINE_MAX_VACATE_TIME, c_rip->r_classad,
			c_jobad,
			c_pledged_machine_max_vacate_time))
		{
			dprintf(D_ALWAYS,"Failed to evaluate %s as an integer.\n",ATTR_MACHINE_MAX_VACATE_TIME);
			c_pledged_machine_max_vacate_time = 0;
		}
	}
	dprintf(D_FULLDEBUG,"pledged MachineMaxVacateTime = %d\n",c_pledged_machine_max_vacate_time);


		// Everything else is only going to be valid if we're not a
		// COD job.  So, if we *are* cod, just return now, since we've
		// got everything we need...
	if( c_type == CLAIM_COD ) {
		return;
	}

	int univ;
	if( c_jobad->LookupInteger(ATTR_JOB_UNIVERSE, univ) == 0 ) {
		univ = CONDOR_UNIVERSE_VANILLA;
		c_rip->dprintf( D_ALWAYS, "Default universe \"%s\" (%d) "
						"since not in classad\n",
						CondorUniverseName(univ), univ );
	} else {
		c_rip->dprintf( D_ALWAYS, "Got universe \"%s\" (%d) "
						"from request classad\n",
						CondorUniverseName(univ), univ );
	}
	c_universe = univ;

	bool wantCheckpoint = false;
	switch( univ ) {
		case CONDOR_UNIVERSE_VANILLA:
			c_jobad->LookupBool( ATTR_WANT_CHECKPOINT_SIGNAL, wantCheckpoint );
			if( ! wantCheckpoint ) { break; }
			//@fallthrough@
		case CONDOR_UNIVERSE_VM:
			c_last_pckpt = now;
		default:
			break;
	}

	c_rip->setAcceptedWhileDraining();
	resmgr->adlist_reset_monitors( c_rip->r_id, c_rip->r_classad );
	resmgr->startd_stats.total_job_starts += 1;
}


void
Claim::setaliveint( int new_alive )
{
	c_aliveint = new_alive;
		// for now, set our lease_duration, too, just so it's
		// initalized to something reasonable.  once we get the job ad
		// we'll reset it to the real value if it's defined.
	c_lease_duration = max_claim_alives_missed * new_alive;
}

void
Claim::setLeaseEndtime(time_t end_time)
{
	// TODO reset timer if updated after timer start?
	c_lease_endtime = end_time;
}

void Claim::cacheJobInfo( ClassAd* job )
{
	job->LookupInteger( ATTR_CLUSTER_ID, c_cluster );
	job->LookupInteger( ATTR_PROC_ID, c_proc );
	if( c_cluster >= 0 ) { 
			// if the cluster is set and the proc isn't, use 0.
		if( c_proc < 0 ) { 
			c_proc = 0;
		}
			// only print this if the request specified it...
		c_rip->dprintf( D_ALWAYS, "Remote job ID is %d.%d\n", 
						c_cluster, c_proc );
	}

	job->LookupString( ATTR_GLOBAL_JOB_ID, &c_global_job_id );
	if( c_global_job_id ) {
			// only print this if the request specified it...
		c_rip->dprintf( D_FULLDEBUG, "Remote global job ID is %s\n", 
						c_global_job_id );
	}

		// check for an explicit job lease duration.  if it's not
		// there, we have to use the old default of 3 * aliveint. :( 
	if( job->LookupInteger(ATTR_JOB_LEASE_DURATION, c_lease_duration) ) {
		dprintf( D_FULLDEBUG, "%s defined in job ClassAd: %d\n", 
				 ATTR_JOB_LEASE_DURATION, c_lease_duration );
		dprintf( D_FULLDEBUG,
				 "Resetting ClaimLease timer (%d) with new duration\n", 
				 c_lease_tid );
	} else if( c_type == CLAIM_OPPORTUNISTIC ) {
		c_lease_duration = max_claim_alives_missed * c_aliveint;
		dprintf( D_FULLDEBUG, "%s not defined: using %d ("
				 "alive_interval [%d] * max_missed [%d]\n", 
				 ATTR_JOB_LEASE_DURATION, c_lease_duration,
				 c_aliveint, max_claim_alives_missed );
	}
}

void
Claim::startLeaseTimer()
{
	if( c_lease_duration < 0 ) {
		if( c_type == CLAIM_COD ) {
				// COD claims have no lease by default.
			return;
		}
		dprintf( D_ALWAYS, "Warning: starting ClaimLease timer before "
				 "lease duration set.\n" );
		c_lease_duration = 1200;
	}
	if( c_lease_tid != -1 ) {
	   EXCEPT( "Claim::startLeaseTimer() called w/ c_lease_tid = %d", 
			   c_lease_tid );
	}
	int when = c_lease_duration;
	time_t now = time(nullptr);
	if (c_lease_endtime && (c_lease_endtime < now + c_lease_duration)) {
		when = (int)(c_lease_endtime - now);
	}
	c_lease_tid =
		daemonCore->Register_Timer( when, 0,
				(TimerHandlercpp)&Claim::leaseExpired,
				"Claim::leaseExpired", this );
	if( c_lease_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}

	// Figure out who should sending keep alives
	// note that the job-ad lookups MUST be here rather than in cacheJobInfo
	// because the  job ad we get from the shadow at activation time doesn't have
	// the necessary attributes - they are only in the job ad we get from the schedd at claim time
	// see #6568 for more details.
	std::string value;
	param( value, "STARTD_SENDS_ALIVES", "peer" );
	if ( c_jobad && c_jobad->LookupBool( ATTR_STARTD_SENDS_ALIVES, c_startd_sends_alives ) ) {
		// Use value from ad
	} else if ( strcasecmp( value.c_str(), "false" ) == 0 ) {
		c_startd_sends_alives = false;
	} else if ( strcasecmp( value.c_str(), "true" ) == 0 ) {
		c_startd_sends_alives = true;
	} else {
		// No direction from the schedd or config file.
		c_startd_sends_alives = false;
	}

	// If startd is sending the alives, look to see if the schedd is requesting
	// that we let only send alives when there is no starter present (i.e. when
	// the claim is idle).
	c_starter_handles_alives = false;  // default to false unless schedd tells us
	if (c_jobad) {
		c_jobad->LookupBool( ATTR_STARTER_HANDLES_ALIVES, c_starter_handles_alives );
	}

	if ( c_startd_sends_alives &&
		 c_type != CLAIM_COD &&
		 c_lease_duration > 0 )	// prevent divide by zero
	{
		if ( c_sendalive_tid != -1 ) {
			daemonCore->Cancel_Timer(c_sendalive_tid);
		}
		
		c_sendalive_tid =
			daemonCore->Register_Timer( c_lease_duration / 3, 
				c_lease_duration / 3, 
				(TimerHandlercpp)&Claim::sendAlive,
				"Claim::sendAlive", this );
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

		// Anytime we cancel the lease, we should also cancel
		// the timer to renew the lease.  So do that now.
	if ( c_sendalive_tid != -1 ) {
		daemonCore->Cancel_Timer(c_sendalive_tid);
		c_sendalive_tid = -1;
	}

}

void
Claim::sendAlive( int /* timerID */ )
{
	const char* c_addr = NULL;

	if (c_rip->is_partitionable_slot()) {
		// For a claimed pslot, assume the schedd is alive.
		// The claim has a limited lifetime, which will eventually
		// be hit.
		// TODO We plan to add a variant of the ALIVE command that
		//   includes the current pslot ad.
		alive();
		return;
	}

	if ( c_starter_handles_alives && isActive() ) {
			// If the starter is dealing with the alive protocol,
			// then we only need to send alives when claimed/idle.
			// In this instance, the claim is active and thus 
			// there is a starter... so just push forward the lease
			// without making any connections.
		alive();	// this will push forward the lease & alive expiration timer
		return;
	}

	if ( c_client ) {
		c_addr = c_client->c_addr.c_str();
	}

	if( !c_addr ) {
			// Client not really set, nothing to do.
		return;
	}

	if ( c_alive_inprogress_sock ) {
			// already did it
		return;
	}

	DCSchedd matched_schedd ( c_addr, NULL );
	Sock* sock = NULL;

	dprintf( D_PROTOCOL, "Sending alive to schedd %s...\n", c_addr); 

	int connect_timeout = MAX(20, ((c_lease_duration / 3)-3) );

	if (!(sock = matched_schedd.reliSock( connect_timeout, 0, NULL, true ))) {
		dprintf( D_ERROR,
				"Alive failed - couldn't initiate connection to %s\n",
		         c_addr );
		return;
	}

	char to_schedd[256];
	snprintf ( to_schedd, 256, "Alive to schedd %s", c_addr );

	int reg_rc = daemonCore->
			Register_Socket( sock, "<Alive Contact Socket>",
			  (SocketHandlercpp)&Claim::sendAliveConnectHandler,
			  to_schedd, this );

	if(reg_rc < 0) {
		dprintf( D_ALWAYS,
		         "Failed to register socket for alive to schedd at %s.  "
		         "Register_Socket returned %d.\n",
		         c_addr,reg_rc);
		delete sock;
		return;
	}

	c_alive_inprogress_sock = sock;
}

/* ALIVE_BAILOUT def:
   before each bad return we reset the alive timer to try again
   soon, since the heartbeat failed */
#define ALIVE_BAILOUT									\
	daemonCore->Reset_Timer(c_sendalive_tid, 5, 5 );	\
	c_alive_inprogress_sock = NULL;						\
	return FALSE;

int
Claim::sendAliveConnectHandler(Stream *s)
{
	const char* c_addr = "(unknown)";
	if ( c_client ) {
		c_addr = c_client->c_addr.c_str();
	}

	char *claimId = id();

	if ( !claimId ) {
		dprintf( D_ALWAYS, "ERROR In Claim::sendAliveConnectHandler, id unknown\n");
		ALIVE_BAILOUT;
	}

	Sock *sock = (Sock *)s;
	DCSchedd matched_schedd( c_addr, NULL );

	dprintf( D_PROTOCOL, "In Claim::sendAliveConnectHandler id %s\n", publicClaimId());

	if (!sock) {
		dprintf( D_ERROR,
				 "NULL sock when connecting to schedd %s\n",
				 c_addr );
		ALIVE_BAILOUT;  // note daemonCore will close sock for us
	}

	ASSERT(c_alive_inprogress_sock == sock);

	if (!sock->is_connected()) {
		dprintf( D_ERROR, "Failed to connect to schedd %s\n",
				 c_addr );
		ALIVE_BAILOUT;  // note daemonCore will close sock for us
	}

		// Protocl of sending an alive to the schedd: we send
		// the claim id, and schedd responds with an int ack.

	if (!matched_schedd.startCommand(ALIVE, sock, 20, NULL, NULL, false, secSessionId() )) {
		dprintf( D_ERROR,
				"Couldn't send ALIVE to schedd at %s\n",
				 c_addr );
		ALIVE_BAILOUT;  // note daemonCore will close sock for us
	}

	sock->encode();

	if ( !sock->put_secret( claimId ) || !sock->end_of_message() ) {
			dprintf( D_ERROR,
				 "Failed to send Alive to schedd %s for job %d.%d id %s\n",
				 c_addr, c_cluster, c_proc, publicClaimId() );
		ALIVE_BAILOUT;  // note daemonCore will close sock for us
	}

	daemonCore->Cancel_Socket( sock ); //Allow us to re-register this socket.

	char to_schedd[256];
	snprintf ( to_schedd, 256, "Alive to schedd %s", c_addr );
	int reg_rc = daemonCore->
			Register_Socket( sock, "<Alive Contact Socket>",
			  (SocketHandlercpp)&Claim::sendAliveResponseHandler,
			  to_schedd, this );

	if(reg_rc < 0) {
		dprintf( D_ALWAYS,
		         "Failed to register socket for alive to schedd at %s.  "
		         "Register_Socket returned %d.\n",
		         c_addr,reg_rc);
		ALIVE_BAILOUT;  // note daemonCore will close sock for us
	}

	dprintf( D_PROTOCOL, 
		"Leaving Claim::sendAliveConnectHandler success id %s\n",publicClaimId());

	// The stream will be closed when we get a callback
	// in sendAliveResponseHandler.  Keep it open for now.
	c_alive_inprogress_sock = sock;
	return KEEP_STREAM;
}

int
Claim::sendAliveResponseHandler( Stream *sock )
{
	int reply;
	const char* c_addr = "(unknown)";

	if ( c_client ) {
		c_addr = c_client->c_addr.c_str();
	}

	// Now, we set the timeout on the socket to 1 second.  Since we 
	// were called by as a Register_Socket callback, this should not 
	// block if things are working as expected.  
	sock->timeout(1);

 	if( !sock->rcv_int(reply, TRUE) ) {
		dprintf( D_ALWAYS, 
			"Response problem from schedd %s on ALIVE job %d.%d.\n", 
			c_addr, c_cluster, c_proc );	
		ALIVE_BAILOUT;  // note daemonCore will close sock for us
	}

		// So here we got a response from the schedd.  
	dprintf(D_PROTOCOL,
		"Received Alive response of %d from schedd %s job %d.%d\n",
		reply, c_addr, c_cluster, c_proc);

		// If the response is -1, that means the schedd knows nothing
		// about this claim, so relinquish it.
	if ( reply == -1 ) {
		dprintf(D_ERROR,"State change: claim no longer recognized "
			 "by the schedd - removing claim\n" );
		c_alive_inprogress_sock = NULL;
		finishKillClaim();	// get rid of the claim
		return FALSE;
	}

	alive();	// this will push forward the lease & alive expiration timer

		// and now clear c_alive_inprogress_sock since this alive is done.
	c_alive_inprogress_sock = NULL;

	return TRUE;
}


void
Claim::leaseExpired( int /* timerID */ )
{
	cancelLeaseTimer();  // cancel timer(s) in case we are being called directly

	if( c_type == CLAIM_COD ) {
		dprintf( D_ERROR, "COD claim %s lease expired "
				 "(client must not have called 'condor_cod renew' within %d seconds)\n", id(), c_lease_duration );
		if( removeClaim(false) ) {
				// There is no starter, so remove immediately.
				// Otherwise, we will be removed when starter exits.
			CODMgr* pCODMgr = getCODMgr();
			if (pCODMgr)
				pCODMgr->removeClaim(this);
		}
		return;
	}

	dprintf( D_ERROR, "State change: claim lease expired "
			 "(condor_schedd gone?), evicting claim\n" );

		// Kill the claim.
	finishKillClaim();
	return;
}

int
Claim::finishKillClaim()
{
	Resource* res_ip = resmgr->get_by_cur_id( id() );
	if( !res_ip ) {
		EXCEPT( "Can't find resource of expired claim" );
	}
		// Note that this claim either (a) timed out, or (b) is unknown 
		// by the schedd, so we don't try to send a 
		// command to our client.
	if( c_client ) {
		delete c_client;
		c_client = NULL;
	}

		// Kill the claim.
	res_ip->kill_claim();
	return TRUE;
}

void
Claim::alive( bool alive_from_schedd )
{
	dprintf( D_PROTOCOL, "Keep alive for ClaimId %s job %d.%d%s\n", 
			 publicClaimId(), c_cluster, c_proc,
			 c_starter_handles_alives && isActive() ? 
				" auto refreshed because starter running" : 
				alive_from_schedd ? ", received from schedd" : " sent to schedd" );

		// Process an alive command.  This is called whenever we
		// "heard" from the schedd since a claim was created, 
		// for instance:
		//  1 - an alive send from the schedd
		//  2 - an acknowledgement from the schedd to an alive
		//      sent by the startd.
		//  3 - a claim activation.
		//  4 - a starter is still running and c_starter_handles_alives==True

		// First push forward our lease timer.
	if( c_lease_tid == -1 ) {
		startLeaseTimer();
	}
	else {
		int when = c_lease_duration;
		if (c_lease_endtime && (c_lease_endtime < time(NULL) + c_lease_duration)) {
			when = (int)(c_lease_endtime - time(NULL));
		}
		daemonCore->Reset_Timer( c_lease_tid, when, 0 );
	}

		// And now push forward our send alives timer.  Plus,
		// it is possible that c_lease_duration changed on activation
		// of a claim, so our timer reset here will handle that case
		// as well since alive() is called upon claim activation.
		// If we got an alive message from the schedd, send our own
		// alive message soon, since that should cause the schedd to
		// stop sending them (since we're supposed to be sending them).
	if ( c_sendalive_tid != -1 ) {
		if ( alive_from_schedd ) {
			daemonCore->Reset_Timer(c_sendalive_tid, 10, 
									c_lease_duration / 3);
		} else {
			daemonCore->Reset_Timer(c_sendalive_tid, c_lease_duration / 3, 
									c_lease_duration / 3);
		}
	}
}


bool
Claim::hasJobAd() {
	bool has_it = false;
	if (c_has_job_ad) {
		has_it = true;
	}
#if HAVE_JOB_HOOKS
	else if (c_type == CLAIM_FETCH && c_jobad != NULL) {
		has_it = true;
	}
#endif /* HAVE_JOB_HOOKS */
	return has_it;
}


// Set our ad to the given pointer
void Claim::setjobad(ClassAd *cad)
{
	if (c_jobad && (c_jobad != cad)) {
		delete c_jobad;
	}
	c_jobad = cad;
}


void
Claim::setRequestStream(Stream* stream)
{
	if( c_request_stream ) {
		daemonCore->Cancel_Socket( c_request_stream );
		delete( c_request_stream );
	}
	c_request_stream = stream;

	if( c_request_stream ) {
		std::string desc;
		formatstr(desc, "request claim %s", publicClaimId() );

		int register_rc = daemonCore->Register_Socket(
			c_request_stream,
			desc.c_str(),
			(SocketHandlercpp)&Claim::requestClaimSockClosed,
			"requestClaimSockClosed",
			this );

		if( register_rc < 0 ) {
			dprintf(D_ALWAYS,
					"Failed to register claim request socket "
					" to detect premature closure for claim %s.\n",
					publicClaimId() );
		}
	}
}

int
Claim::requestClaimSockClosed(Stream *s)
{
	dprintf( D_ALWAYS,
			 "Request claim socket closed prematurely for claim %s. "
			 "This probably means the schedd gave up.\n",
			 publicClaimId() );

	ASSERT( s == c_request_stream );
	c_request_stream = NULL; // socket will be closed when we return

	c_rip->removeClaim( this );

	return FALSE;
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

char const*
Claim::publicClaimId( void )
{
	if( c_id ) {
		return c_id->publicClaimId();
	} else {
		return "<unknown>";
	}
}

char const*
Claim::secSessionId( void )
{
	if( c_id ) {
		return c_id->secSessionId();
	}
	return NULL;
}


bool
Claim::idMatches( const char* req_id )
{
	if( c_id ) {
		return c_id->matches( req_id );
	}
	return false;
}


void Claim::updateUsage(double & percentCpuUsage, long long & imageSize)
{
	percentCpuUsage = 0.0;
	imageSize = 0;
	if (c_starter_pid) {
		Starter *starter = findStarterByPid(c_starter_pid);
		if ( ! starter) {
			EXCEPT( "Claim::updateUsage(): Could not find starter object for pid %d", c_starter_pid );
		}
		const ProcFamilyUsage & usage = starter->updateUsage();
		percentCpuUsage = usage.percent_cpu;
		imageSize = usage.total_image_size;
	}
	// save off the last values so we can use them in the ::publish method
	c_cpus_usage = percentCpuUsage / 100;
	c_image_size = imageSize;
}


CODMgr*
Claim::getCODMgr( void )
{
	if( ! c_rip ) {
		return NULL;
	}
	return c_rip->r_cod_mgr;
}

// spawn a starter in the given starter object.
// on successful spawn, the claim will take ownership of the job classad.
// job can be NULL in the case where we are doing a delayed spawn because of preemption
// or when doing fetchwork.  when job is NULL, the c_ad member of this class must not be.
pid_t Claim::spawnStarter( Starter* starter, ClassAd * job, Stream* s)
{
	if( ! starter ) {
			// Big error!
		dprintf( D_ALWAYS, "ERROR! Claim::spawnStarter() called "
				 "w/o a Starter object! Returning failure\n" );
		return FALSE;
	}

	// the starter had better not already have an active process.
	ASSERT ( ! starter->pid());

	double now = condor_gettimestamp_double();

	// grab job id, etc out of the job ad and write it into the claim.
	if ( ! job) {
		// refresh cached values from the ad that we already own.
		// this happends when the spawn of the starter was delayed
		// because of preemption
		ASSERT(c_jobad);
		cacheJobInfo(c_jobad);
	} else {
		// this also caches other values from the ad
		// but does NOT take ownership of the job ad (we do that after we spawn)
		cacheJobInfo(job);
	}

	std::string prefix;
	formatstr(prefix, "%s[%d.%d]", c_rip->r_id_str, c_cluster, c_proc);
	starter->set_dprintf_prefix(prefix.c_str());

	// HACK!! Starter::spawn reaches back into the claim object to grab values out of the c_ad member
	// so we have to temporarily set it, even though we have not yet decided to take ownership of the
	// passed in job (we will only own it if the spawn succeeds)
	// when job is NULL and c_ad has already been set, then this HACK does nothing.
	// the code should probably be refactored so that the job ad is an explicit argument to Starter::spawn.
	ClassAd * old_c_ad = c_jobad;
	if (job) { c_jobad = job; }

	c_starter_pid = starter->spawn( this, (time_t)now, s );

	c_jobad = old_c_ad;

	if (c_starter_pid) {
		if (job) { setjobad(job); } // transfer ownership of the job ad to the claim.
		alive();
	} else {
		resetClaim();
		return FALSE;
	}

	changeState( CLAIM_RUNNING );

		// Do other bookkeeping so this claim knows it started an activation.
	beginActivation(now);

		// WE USED TO....
		//
		// Fake ourselves out so we take another snapshot in 15
		// seconds, once the starter has had a chance to spawn the
		// user job and the job as (hopefully) done any initial
		// forking it's going to do.  If we're planning to check more
		// often that 15 seconds, anyway, don't bother with this.
		//
		// TODO: should we set a timer here to tell the procd
		// explicitly to take a snapshot in 15 seconds???

	return TRUE;
}


void
Claim::starterExited( Starter* starter, int status)
{
		// Now that the starter is gone, we need to change our state
	changeState( CLAIM_IDLE );

		// Notify our starter object that its starter exited, so it
		// can cancel timers any pending timers, cleanup the starter's
		// execute directory, and do any other cleanup. 
		// note: null pointer check here is to make coverity happy, not because we think it possible for starter to be null.
	if (starter) {
		starter->exited(this, status);
		delete starter; starter = NULL;
	}

	// update stat for JobBusyTime
	if (c_job_start > 0) {
		double busyTime = condor_gettimestamp_double() - c_job_start;
		resmgr->startd_stats.job_busy_time += busyTime;
	}

	if( c_badput_caused_by_draining ) {
		time_t badput = getJobTotalRunTime() * c_rip->r_attr->num_cpus();
		dprintf(D_ALWAYS,"Adding to badput caused by draining: %ld cpu-seconds\n",badput);
		resmgr->addToDrainingBadput( badput );
	}

		// Now, clear out this claim with all the starter-specific
		// info, including the starter object itself.
	resetClaim();

		// If exit code of starter is 0, it is a normal exit.
		// If exit code of starter is 1, starter failed to startup 
		// If exit code of starter is 2, it lost connection to the shadow
	if ( WIFEXITED(status) && 
		 WEXITSTATUS(status) == STARTER_EXIT_LOST_SHADOW_CONNECTION ) 
	{
			// Starter lost connection to shadow, treat it as if the
			// lease on the slot expired.
			// We do not just directly call leaseExpired() here, since
			// that will remove some state in the Resource object that
			// the call to starterExited() below will want to inspect.
			// So instead, just set a zero second timer to call leaseExpired().
			// This way, starterExited() below gets to do the right thing, including
			// perhaps giving the resource away to a preempting claim... and
			// yet if this claim object still exists after starterExited() does its thing, 
			// when the timer goes off we will be sure to destroy this claim and
			// put the resource back to Unclaimed. See gt#4807.  Todd Tannenbaum 1/15
		if( c_lease_tid == -1 ) {
			startLeaseTimer();
		}
		daemonCore->Reset_Timer( c_lease_tid, 0 );
	}

	// Check for exit codes that indicate the Starter could not clean up and
	// the startd will probably be unable to also, so it should mark the resource as broken
	if (WIFEXITED(status) && 
		WEXITSTATUS(status) >= STARTER_EXIT_BROKEN_RES_FIRST &&
		WEXITSTATUS(status) <= STARTER_EXIT_BROKEN_RES_LAST)
	{
		int code = BROKEN_CODE_UNCLEAN;
		const char * reason = "Could not clean up after job";
		switch (WEXITSTATUS(status)) {
		case STARTER_EXIT_IMMORTAL_LVM:
			code = BROKEN_CODE_UNCLEAN_LV;
			reason = "Could not clean up Logical Volume";
			break;
		case STARTER_EXIT_IMMORTAL_JOB_PROCESS:
			code = BROKEN_CODE_HUNG_PID;
			reason = "Could not terminate all job processes";
			break;
		default:
			// go with generic code and reason
			break;
		}

		if ( ! c_rip->r_attr->is_broken()) {
			// changing the resource bag to broken changes the resources of the slot
			c_rip->r_attr->set_broken(code, reason);
			resmgr->refresh_classad_resources(c_rip);
			// force a refresh to the collector as well
			c_rip->update_needed(Resource::WhyFor::wf_refreshRes);
			resmgr->rip_update_needed(1<<Resource::WhyFor::wf_daemonAd);
		}
	}

		// Finally, let our resource know that our starter exited, so
		// it can do the right thing.
		// This should be done as the last thing in this method; it
		// is possible that starterExited may destroy this object.
	c_rip->starterExited( this );

	// Think twice about doing anything after returning from starterExited(),
	// as perhaps this claim object has now been destroyed.
}


bool
Claim::starterPidMatches( pid_t starter_pid ) const
{
	if (c_starter_pid && starter_pid == c_starter_pid) {
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
				 c_starter_pid);
		return false;
	}
	dprintf( D_FULLDEBUG, "Removing inactive claim %s\n", id() );
	return true;
}


bool
Claim::suspendClaim( void )
{
	changeState( CLAIM_SUSPENDED );

	Starter* starter = findStarterByPid(c_starter_pid);
	if (starter) {
		return (bool)starter->suspend();
	}

		// if there's no starter, we don't need to do anything, so
		// it worked...  
	return true;
}


bool
Claim::resumeClaim( void )
{
	Starter* starter = findStarterByPid(c_starter_pid);
	if (starter)  {
		changeState( CLAIM_RUNNING );
		return starter->resume();
	}

		// if there's no starter, we don't need to do anything, so
		// it worked...  
	changeState( CLAIM_IDLE );

	return true;
}


bool
Claim::starterSignal( int sig ) const
{
		// don't need to work about the state, since we don't use this
		// method to send any signals that change the claim state...
	Starter* starter = findStarterByPid(c_starter_pid);
	if (starter)  {
		return starter->signal( sig );
	}

		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillFamily()
{
	Starter* starter = findStarterByPid(c_starter_pid);
	if (starter) {
			// if we're using killfamily(), we're trying to hard-kill the
			// starter and all its children
		changeState( CLAIM_KILLING );
		return starter->killfamily();
	}

		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillSoft( bool state_change )
{
	Starter* starter = findStarterByPid(c_starter_pid);
	if (starter) {
		changeState( CLAIM_VACATING );
		int timeout = c_rip ? c_rip->evalMaxVacateTime() : 0;
		return starter->killSoft( timeout, state_change );
	}

		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillHard( void )
{
	Starter* starter = findStarterByPid(c_starter_pid);
	if (starter) {
		changeState( CLAIM_KILLING );
		int timeout = (universe() == CONDOR_UNIVERSE_VM) ? vm_killing_timeout : killing_timeout;
		return starter->killHard(timeout);
	}

		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


void
Claim::starterHoldJob( char const *hold_reason,int hold_code,int hold_subcode,bool soft )
{
	Starter* starter = findStarterByPid(c_starter_pid);
	if (starter) {
		int timeout;
		if (soft) {
			timeout = c_rip ? c_rip->evalMaxVacateTime() : 0;
		} else {
			timeout = (universe() == CONDOR_UNIVERSE_VM) ? vm_killing_timeout : killing_timeout;
		}
		if( starter->holdJob(hold_reason,hold_code,hold_subcode,soft,timeout) ) {
			return;
		}
		dprintf(D_ALWAYS,"Starter unable to hold job, so evicting job instead.\n");
	}

	if( soft ) {
		starterKillSoft();
	}
	else {
		starterKillHard();
	}
}

void
Claim::makeStarterArgs( ArgList &args )
{
	switch (c_type) {
	case CLAIM_COD:
		makeCODStarterArgs(args);
		break;
#if HAVE_JOB_HOOKS
	case CLAIM_FETCH:
		makeFetchStarterArgs(args);
		break;
#endif /* HAVE_JOB_HOOKS */
	default:
		EXCEPT("Impossible: makeStarterArgs() called with unsupported claim type"); 
	}
}

#if HAVE_JOB_HOOKS
void
Claim::makeFetchStarterArgs( ArgList &args )
{
	args.AppendArg("condor_starter");
	args.AppendArg("-f");
	if ( resmgr->is_smp() ) {
		args.AppendArg("-a");
		args.AppendArg(rip()->r_id_str);
	}
	args.AppendArg("-job-input-ad");
	args.AppendArg("-");
}
#endif /* HAVE_JOB_HOOKS */


void
Claim::makeCODStarterArgs( ArgList &args )
{
		// first deal with everthing that's shared, no matter what.
	args.AppendArg("condor_starter");
	args.AppendArg("-f");
	args.AppendArg("-append");
	args.AppendArg("cod");
	args.AppendArg("-header");

	std::string cod_id_arg;
	cod_id_arg += "(";
	if( resmgr->is_smp() ) {
		cod_id_arg += c_rip->r_id_str;
		cod_id_arg += ':';
	}
	cod_id_arg += codId();
	cod_id_arg += ")";
	args.AppendArg(cod_id_arg);

		// if we've got a cluster and proc for the job, append those
	if( c_cluster >= 0 ) {
		args.AppendArg("-job-cluster");
		args.AppendArg(std::to_string(c_cluster));
	} 
	if( c_proc >= 0 ) {
		args.AppendArg("-job-proc");
		args.AppendArg(std::to_string(c_proc));
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
}


bool
Claim::verifyCODAttrs( ClassAd* req )
{

	if( c_cod_keyword ) {
		EXCEPT( "Trying to activate a COD claim that has a keyword" );
	}

	req->LookupString( ATTR_JOB_KEYWORD, &c_cod_keyword );
	req->LookupBool( ATTR_HAS_JOB_AD, c_has_job_ad );

	if( c_cod_keyword || c_has_job_ad ) {
		return true;
	}
	return false;
}


bool
Claim::publishStarterAd(ClassAd *cad) const
{
	Starter * starter = NULL;
	if (c_starter_pid) {
		starter = findStarterByPid(c_starter_pid);
	}

	if ( ! starter) {
		return false;
	}

	char const* ip_addr = starter->getIpAddr();
	if (ip_addr) {
		cad->Assign(ATTR_STARTER_IP_ADDR, ip_addr);
	}


		// stuff in starter-specific attributes, if we have them.
	starter->publish(cad);

		// TODO add more goodness to this ClassAd??

	return true;
}

bool
Claim::periodicCheckpoint( void )
{
	Starter * starter = findStarterByPid(c_starter_pid);
	if (starter) {
		if( ! starter->signal(DC_SIGPCKPT) ) {
			return false;
		}
	}
	c_last_pckpt = time(nullptr);
	return true;
}


bool
Claim::ownerMatches( const char* owner )
{
	if( ! strcmp(c_client->c_owner.c_str(), owner) ) {
		return true;
	}
		// TODO: handle COD_SUPER_USERS
	return false;
}


bool
Claim::globalJobIdMatches( const char* req_id )
{
	if( c_global_job_id && !strcmp(c_global_job_id, req_id) ) {
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
	int rval;

	reply.Assign(ATTR_RESULT, getCAResultString(CA_SUCCESS));

	reply.Assign(ATTR_LAST_CLAIM_STATE, getClaimStateString(c_last_state));
	
		// TODO: hopefully we can put the final job update ad in here,
		// too. 
	
	rval = sendCAReply( c_request_stream, "CA_RELEASE_CLAIM", &reply );
	
		// now that we're done replying, we need to delete this stream
		// so we don't leak memory, since DaemonCore's not going to do
		// that for us in this case
	setRequestStream(NULL);
	c_pending_cmd = -1;
	
	dprintf( D_ALWAYS, "Finished releasing claim %s (owner: '%s')\n", 
			 id(), client()->c_owner.c_str() );

	c_rip->removeClaim( this );

		// THIS OBJECT IS NOW DELETED, WE CAN *ONLY* RETURN NOW!!!
	return rval;
}


int
Claim::finishDeactivateCmd( void )
{
	ClassAd reply;
	int rval;

	reply.Assign(ATTR_RESULT, getCAResultString(CA_SUCCESS));
	reply.Assign(ATTR_LAST_CLAIM_STATE, getClaimStateString(c_last_state));
	
		// TODO: hopefully we can put the final job update ad in here,
		// too. 
	
	rval = sendCAReply( c_request_stream, "CA_DEACTIVATE_CLAIM", &reply );
	
		// now that we're done replying, we need to delete this stream
		// so we don't leak memory, since DaemonCore's not going to do
		// that for us in this case
	setRequestStream(NULL);
	c_pending_cmd = -1;

	dprintf( D_ALWAYS, "Finished deactivating claim %s (owner: '%s')\n", 
			 id(), client()->c_owner.c_str() );

		// also, we must reset all the attributes we're storing in
		// this Claim object that are specific to a given activation. 
	resetClaim();

	return rval;
}


void
Claim::resetClaim( void )
{
	c_starter_pid = 0;
	c_image_size = 0;
	c_cpus_usage = 0;

	if( c_jobad && c_type == CLAIM_COD ) {
		delete( c_jobad );
		c_jobad = NULL;
	}
	c_job_execution_overlay_ad.Clear();

	c_universe = -1;
	c_cluster = -1;
	c_proc = -1;
	c_job_start = -1;
	c_last_pckpt = 0;
	if( c_global_job_id ) {
		free( c_global_job_id );
		c_global_job_id = NULL;
	}
	if( c_cod_keyword ) {
		free( c_cod_keyword );
		c_cod_keyword = NULL;
	}
	c_has_job_ad = false;
	c_job_total_run_time = 0;
	c_job_total_suspend_time = 0;
	c_may_unretire = true;
	c_preempt_was_true = false;
	c_badput_caused_by_draining = false;
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

		// everytime a COD claim changes state, we want to update the
		// collector. 
	if( c_type == CLAIM_COD ) {
		c_rip->update_needed(Resource::WhyFor::wf_cod);
	}
}

time_t
Claim::getClaimAge() const
{
	time_t now = time(NULL);
	if(c_claim_started) {
		return now - c_claim_started;
	}
	return 0;
}

bool
Claim::writeJobAd( int pipe_end )
{
	// pipe_end is a DaemonCore pipe, so we must use
	// DC::Write_Pipe for writing to it
	
	std::string ad_str;
	// The starter doesn't need to know any secrets.
	sPrintAd(ad_str, *c_jobad);

	const char* ptr = ad_str.c_str();
	int len = ad_str.length();
	while (len) {
		int bytes_written = daemonCore->Write_Pipe(pipe_end, ptr, len);
		if (bytes_written == -1) {
			dprintf(D_ALWAYS, "writeJobAd: Write_Pipe failed\n");
			return false;
		}
		ptr += bytes_written;
		len -= bytes_written;
	}

	return true;
}

bool
Claim::writeMachAdAndOverlay( Stream* stream )
{
	//PRAGMA_REMIND("flatten SlotEval here before printing/sending ad on wire")
	if (IsDebugLevel(D_MACHINE)) {
		std::string adbuf;
		dprintf(D_MACHINE, "Sending Machine Ad to Starter :\n%s", formatAd(adbuf, *c_rip->r_classad, "\t"));
	} else {
		dprintf(D_FULLDEBUG, "Sending Machine Ad to Starter\n");
	}
	if (!putClassAd(stream, *c_rip->r_classad)) {
		dprintf(D_ALWAYS, "writeMachAd: Failed to write machine ClassAd to stream\n");
		return false;
	}

	// now write the execution overlay
	if (IsDebugLevel(D_JOB)) {
		std::string adbuf;
		dprintf(D_JOB, "Sending Execution Overlay Ad to Starter :\n%s", formatAd(adbuf, c_job_execution_overlay_ad, "\t"));
	} else {
		dprintf(D_FULLDEBUG, "Sending Execution Overlay Ad to Starter (%d attributes)\n", (int)c_job_execution_overlay_ad.size());
	}
	if (!putClassAd(stream, c_job_execution_overlay_ad) || !stream->end_of_message()) {
		dprintf(D_ALWAYS, "writeMachAd: Failed to write machine ClassAd to stream\n");
		return false;
	}
	return true;
}


///////////////////////////////////////////////////////////////////////////
// ClaimId
///////////////////////////////////////////////////////////////////////////

static 
int
newIdString( char** id_str_ptr )
{
		// ClaimId string is of the form:
		// (keep this in sync with condor_claimid_parser)
		// "<ip:port>#startd_bday#sequence_num#cookie"

	static int sequence_num = 0;
	sequence_num++;

	std::string id;
	// Keeping with tradition, we insert the startd's address in
	// the claim id.  As of condor 7.2, nothing relies on this.
	// Starting in 8.9, we use the full sinful string, which can
	// contain '#'. Parsers should look for the first '>' in the
	// string to reliably extract the startd's sinful.

	formatstr( id, "%s#%lld#%d#", daemonCore->publicNetworkIpAddr(),
	           (long long)startd_startup, sequence_num );

	char *keybuf = Condor_Crypt_Base::randomHexKey(SEC_SESSION_KEY_LENGTH_V9);
	id += keybuf;
	free( keybuf );

	*id_str_ptr = strdup( id.c_str() );
	return sequence_num;
}


ClaimId::ClaimId( ClaimType claim_type, char const * /*slotname*/ /*UNUSED*/ )
{
	int num = newIdString( &c_id );
	claimid_parser.setClaimId(c_id);
	if( claim_type == CLAIM_COD ) { 
		char buf[64];
		snprintf( buf, 64, "COD%d", num );
		buf[COUNTOF(buf)-1] = 0; // snprintf doesn't necessarly null terminate.
		c_cod_id = strdup( buf );
	} else {
		c_cod_id = NULL;
	}

	if( claim_type == CLAIM_OPPORTUNISTIC
		&& param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true) )
	{
		std::string session_id;
		std::string session_key;
		std::string session_info;
			// there is no sec session info yet in the claim id, so
			// we call secSessionId with ignore_session_info=true to
			// force it to give us the session id
		session_id = claimid_parser.secSessionId(/*ignore_session_info=*/true);
		session_key = claimid_parser.secSessionKey();

		bool rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
			DAEMON,
			session_id.c_str(),
			session_key.c_str(),
			NULL,
			AUTH_METHOD_MATCH,
			SUBMIT_SIDE_MATCHSESSION_FQU,
			NULL,
			0,
			nullptr, true );

		if( !rc ) {
			dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create "
					"security session for claim id %s\n", session_id.c_str());
		}
		else {
				// fill in session_info so that schedd will have
				// enough info to create a pre-built security session
				// compatible with the one we just created.
			rc = daemonCore->getSecMan()->ExportSecSessionInfo(
				session_id.c_str(),
				session_info );

			if( !rc ) {
				dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to get "
						"session info for claim id %s\n",session_id.c_str());
			}
			else {
				claimid_parser.setSecSessionInfo( session_info.c_str() );
				free( c_id );
				c_id = strdup(claimid_parser.claimId());

					// after rewriting session info, verify that session id
					// and session key are the same that we used when we
					// created the session
				ASSERT( session_id == claimid_parser.secSessionId() );
				ASSERT( session_key == claimid_parser.secSessionKey() );
				ASSERT( session_info == claimid_parser.secSessionInfo() );
			}
		}
	}
}


ClaimId::~ClaimId()
{
	if( claimid_parser.secSessionId()[0] != '\0' ) {
			// Expire the session after enough time to let the final
			// RELEASE_CLAIM command finish, in case it is still in
			// progress.  This also allows us to more gracefully
			// handle any final communication from the schedd that may
			// still be in flight.
		daemonCore->getSecMan()->SetSessionExpiration(claimid_parser.secSessionId(),time(NULL)+600);
	}

	free( c_id );
	if( c_cod_id ) {
		free( c_cod_id );
	}
}


bool
ClaimId::matches( const char* req_id )
{
	if( !req_id ) {
		return false;
	}
	return( strcmp(c_id, req_id) == 0 );
}


void
ClaimId::dropFile( int slot_id )
{
	if( ! param_boolean("STARTD_SHOULD_WRITE_CLAIM_ID_FILE", true) ) {
		return;
	}
	std::string filename = startdClaimIdFile( slot_id );  
	if( filename.empty() ) {
		dprintf( D_ALWAYS, "Error getting claim id filename, not writing\n" );
		return;
	}

	std::string filename_final = filename;
	std::string filename_tmp = filename;

	filename_tmp += ".new";

	FILE* NEW_FILE = safe_fopen_wrapper_follow( filename_tmp.c_str(), "w", 0600 );
	if( ! NEW_FILE ) {
		dprintf( D_ALWAYS,
				 "ERROR: can't open claim id file: %s: %s (errno: %d)\n",
				 filename_tmp.c_str(), strerror(errno), errno );
 		return;
	}
	fprintf( NEW_FILE, "%s\n", c_id );
	fclose( NEW_FILE );
	if( rotate_file(filename_tmp.c_str(), filename_final.c_str()) < 0 ) {
		dprintf( D_ALWAYS, "ERROR: failed to move %s into place, removing\n",
				 filename_tmp.c_str() );
		if (unlink(filename_tmp.c_str()) < 0) {
			dprintf( D_ALWAYS, "ERROR: failed to remove %s\n", filename_tmp.c_str() );
		}
	}
}

void
Claim::receiveJobClassAdUpdate( ClassAd &update_ad, bool final_update )
{
	ASSERT( c_jobad );

	const char *name;
	ExprTree *expr;
	for ( auto itr = update_ad.begin(); itr != update_ad.end(); itr++ ) {
		name = itr->first.c_str();
		expr = itr->second;

		ASSERT( name );
		if( !strcmp(name,ATTR_MY_TYPE) ||
			!strcmp(name,ATTR_TARGET_TYPE) )
		{
				// ignore MyType and TargetType
			continue;
		}

			// replace expression in current ad with expression from update ad
		ExprTree *new_expr = expr->Copy();
		ASSERT( new_expr );
		if ( ! c_jobad->Insert(name, new_expr)) {
			delete new_expr;
		}
	}
	loadStatistics();
	if( IsDebugVerbose(D_JOB) ) {
		std::string adbuf;
		dprintf(D_JOB | D_VERBOSE,"Updated job ClassAd:\n%s", formatAd(adbuf, *c_jobad, "\t"));
	}

	if (final_update) {
		double duration = 0.0;
		if ( ! c_jobad->LookupFloat(ATTR_JOB_DURATION, duration)) {
			duration = 0.0;
		}
		resmgr->startd_stats.job_duration += duration;
	}
}

bool
Claim::waitingForActivation() const {
	time_t maxDrainingActivationDelay = param_integer( "MAX_DRAINING_ACTIVATION_DELAY", 20 );
	return getClaimAge() < maxDrainingActivationDelay;
}

void
Claim::invalidateID() {
	delete c_id;
	c_id = new ClaimId( type(), c_rip->r_id_str );
}
