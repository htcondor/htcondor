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
#include "startd.h"
#include <algorithm>


CODMgr::CODMgr( Resource* my_rip )
{
	rip = my_rip;
}


CODMgr::~CODMgr()
{
	for (auto claim: claims) {
		delete claim;
	}
	claims.clear();
}


void
CODMgr::publish( ClassAd* ad )
{
	int num_claims = numClaims();
	if( ! num_claims ) {
		return;
	}
	std::string claim_names;
	bool first = true;
	for (auto claim: claims) {
		claim->publishCOD( ad );

		if (first) {
			first = false;
		} else {
			claim_names += ", ";
		}

		if ( claim->codId() ) {
			claim_names += claim->codId();
		}
	}

	ad->Assign( ATTR_NUM_COD_CLAIMS, num_claims );

	ad->Assign( ATTR_COD_CLAIMS, claim_names );
}



Claim*
CODMgr::findClaimById( const char* id )
{
	for (auto claim: claims) {
		if( claim->idMatches(id) ) {
			return claim;
		}
	}
	return nullptr;
}


Claim*
CODMgr::findClaimByPid( pid_t pid )
{
	for (auto claim: claims) {
		if( claim->starterPidMatches(pid) ) {
			return claim;
		}
	}
	return nullptr;
}


Claim*
CODMgr::addClaim( int lease_duration ) 
{
	Claim* new_claim;
	new_claim = new Claim( rip, CLAIM_COD, lease_duration );
	new_claim->beginClaim();
	claims.push_back( new_claim );
	return new_claim;
}


bool
CODMgr::removeClaim( Claim* c ) 
{
	auto it = std::find(claims.begin(), claims.end(), c);
	if (it != claims.end()) {
		delete *it;
		claims.erase(it);
		rip->update_needed(Resource::WhyFor::wf_cod);
	} else {
		dprintf( D_ALWAYS, 
				 "WARNING: CODMgr::removeClaim() could not find claim %s\n", 
				 c->id() );
	}
	return it != claims.end();
}


void
CODMgr::starterExited( Claim* c ) 
{
	bool claim_removed = false;
	if( c->hasPendingCmd() ) {
			// if our pending command is to release the claim, as soon
			// as we finish that command, the claim will be deleted,
			// so we won't want to access it anymore.
		if( c->pendingCmd() == CA_RELEASE_CLAIM ) {
			claim_removed = true;
		}

			// if we're in the middle of a pending command, we can
			// finally complete it and reply now that the starter is
			// gone and we're done cleaning up everything.
		c->finishPendingCmd();
	}

	if( ! claim_removed ) {
		if( c->wantsRemove() ) {
			removeClaim( c );
			claim_removed = true;
		}
	}

		// otherwise, the claim is back to idle again, so we should
		// see if we can resume our opportunistic claim, if we've got
		// one... 
	interactionLogicCODStopped();
}


int
CODMgr::numClaims( void )
{
	return (int) claims.size();
}


bool
CODMgr::hasClaims( void )
{
	return (claims.size() > 0);
}


bool
CODMgr::isRunning( void )
{
	for (auto claim: claims) {
		if( claim->isRunning() ) {
			return true;
		}
	}
	return false;
}


void
CODMgr::shutdownAllClaims( bool graceful )
{
	auto it = claims.begin();
	while (it != claims.end()) {
		if( (*it)->removeClaim(graceful) ) {
			delete(*it);
			it = claims.erase(it);
		} else {
			it++;
		}
			// else, there was a starter which we sent a signal to,
			// so, we'll delete it and clean up in the reaper...
	}
}


int
CODMgr::release( Stream* s, ClassAd* req, Claim* claim )
{
	VacateType vac_type = getVacateType( req );

		// tell this claim we're trying to release it
	claim->setPendingCmd( CA_RELEASE_CLAIM );
	claim->setRequestStream( s );

	switch( claim->state() ) {

	case CLAIM_UNCLAIMED:
			// This is a programmer error.  we can't possibly get here  
		EXCEPT( "Trying to release a claim that was never claimed!" ); 
		break;

	case CLAIM_IDLE:
			// it's not running a job, so we can remove it
			// immediately.
		claim->finishPendingCmd();
		break;

	case CLAIM_RUNNING:
	case CLAIM_SUSPENDED:
			// for these two, we have to kill the starter, and then
			// clean up the claim when it's gone.  so, all we can do
			// now is stash the Stream in the claim, and signal the
			// starter as appropriate;
		claim->deactivateClaim( vac_type == VACATE_GRACEFUL );
		break;

	case CLAIM_VACATING:
			// if we're already preempting gracefully, but the command
			// requested a fast shutdown, do the hardkill.  otherwise,
			// now that we know to release this claim, there's nothing
			// else to do except wait for the starter to exit.
		if( vac_type == VACATE_FAST ) {
			claim->deactivateClaim( false );
		}
		break;

	case CLAIM_KILLING:
			// if we're already trying to fast-kill, there's nothing
			// we can do now except wait for the starter to exit. 
		break;

	}
		// in general, we're going to have to wait to reply to the
  		// requesting entity until the starter exists.  even if we're
		// ready to reply right now, the finishPendingCmd() method will
		// have deleted the stream, so in all cases, we want
		// DaemonCore to leave it alone.
	return KEEP_STREAM;
}


int
CODMgr::activate( Stream* s, ClassAd* req, Claim* claim )
{
	std::string err_msg;

	switch( claim->state() ) {
	case CLAIM_IDLE:
			// this is the only state that makes sense
		break;
	case CLAIM_UNCLAIMED:
			// This is a programmer error.  we can't possibly get here  
		EXCEPT( "Trying to activate a claim that was never claimed!" ); 
		break;
	case CLAIM_SUSPENDED:
	case CLAIM_RUNNING:
	case CLAIM_VACATING:
	case CLAIM_KILLING:
		err_msg = "Cannot activate claim: already active (";
		err_msg += getClaimStateString( claim->state() );
		err_msg += ')';
		return sendErrorReply( s, "CA_ACTIVATE_CLAIM",
							   CA_INVALID_STATE, err_msg.c_str() );
		break;
	}

	Starter* tmp_starter = new Starter;

		// verify the ClassAd to make sure it's got what we need to
		// spawn the COD job 
	if( ! claim->verifyCODAttrs(req) ) {
		err_msg = "Request contains neither ";
		err_msg += ATTR_JOB_KEYWORD;
		err_msg += " nor ";
		err_msg += ATTR_HAS_JOB_AD;
		err_msg += ", so server has no way to find job information\n";
		delete tmp_starter;
		return sendErrorReply( s, "CA_ACTIVATE_CLAIM",
							   CA_INVALID_REQUEST, err_msg.c_str() ); 
	}

		// we need to make a copy of this, since the original is on
		// the stack in command.C:command_classad_handler().
		// otherwise, once the handler completes, we've got a dangling
		// pointer, and if we try to access this variable, we'll crash 
	ClassAd* new_req_ad = new ClassAd( *req );

		// now that we've gotten this far, we know we're going to
		// start a starter.  so, we call the interactionLogic method
		// to deal with the state changes of the opportunistic claim
	interactionLogicCODRunning();

		// finally, spawn the starter and COD job itself
		// spawnStarter will take ownership even on failure

	pid_t starter_pid = claim->spawnStarter(tmp_starter, new_req_ad);
	if( !starter_pid ) {
			// Failed to spawn, make sure everything goes back to
			// normal with the opportunistic claim
		interactionLogicCODStopped();
	} else {
		// We delete the starter object here even though we created it successfully
		// because this will have the side effect of removing the starter object
		// from the list of unreaped starters.  The reaper has been written to
		// tolerate reaping a starter for which there is no Starter object as long
		// as there is still a claim that knows about the pid.
		// This is not a good design, but it's been like that forever, and does not seem
		// worth the effort to fix it.
		delete tmp_starter;
	}
	tmp_starter = nullptr;

	ClassAd reply;
	reply.Assign( ATTR_RESULT,
			starter_pid ? getCAResultString(CA_SUCCESS)
				       : getCAResultString(CA_FAILURE) );

		// TODO any other info for the reply?
	sendCAReply( s, "CA_ACTIVATE_CLAIM", &reply );

	return starter_pid;
}


int
CODMgr::deactivate( Stream* s, ClassAd* req, Claim* claim )
{
	std::string err_msg;
	VacateType vac_type = getVacateType( req );

	claim->setPendingCmd( CA_DEACTIVATE_CLAIM );
	claim->setRequestStream( s );

	switch( claim->state() ) {

	case CLAIM_UNCLAIMED:
			// This is a programmer error.  we can't possibly get here  
		EXCEPT( "Trying to deactivate a claim that was never claimed!" ); 
		break;

	case CLAIM_IDLE:
			// it is not activate, so return an error
		err_msg = "Cannot deactivate a claim that is not active (";
		err_msg += getClaimStateString( CLAIM_IDLE );
		err_msg += ')';

		sendErrorReply( s, "CA_DEACTIVATE_CLAIM",
						CA_INVALID_STATE, err_msg.c_str() ); 
		claim->setRequestStream( NULL );
		claim->setPendingCmd( -1 );
		break;

	case CLAIM_RUNNING:
	case CLAIM_SUSPENDED:
			// for these two, we have to kill the starter, and then
			// notify the other side when it's gone.  so, all we can
			// do now is stash the Stream in the claim, and signal the
			// starter as appropriate;
		claim->deactivateClaim( vac_type == VACATE_GRACEFUL );
		break;

	case CLAIM_VACATING:
			// if we're already preempting gracefully, but the command
			// requested a fast shutdown, do the hardkill.  otherwise,
			// now that we set the flag so we know to reply to this
			// stream, there's nothing else to do except wait for the
			// starter to exit.
		if( vac_type == VACATE_FAST ) {
			claim->deactivateClaim( false );
		}
		break;

	case CLAIM_KILLING:
			// if we're already trying to fast-kill, there's nothing
			// we can do now except wait for the starter to exit. 
		break;

	}
		// in general, we're going to have to wait to reply to the
  		// requesting entity until the starter exists.  even if we
		// just replied, we already deleted the stream by resetting
		// the stashed stream in the claim object.  so, in all cases,
		// we want DaemonCore to leave it alone.
	return KEEP_STREAM;
}




int
CODMgr::suspend( Stream* s, ClassAd* /*req*/ /*UNUSED*/, Claim* claim )
{
	int rval;
	ClassAd reply;
	std::string line;

	switch( claim->state() ) {

	case CLAIM_RUNNING:
		if( claim->suspendClaim() ) { 

			reply.Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );

				// TODO any other info for the reply?
			rval = sendCAReply( s, "CA_SUSPEND_CLAIM", &reply );

				// now that a COD job has stopped, invoke our helper
				// to deal w/ interactions w/ the opportunistic claim
			interactionLogicCODStopped();

			return rval;
		} else {
			return sendErrorReply( s, "CA_SUSPEND_CLAIM",
								   CA_FAILURE, 
						   "Failed to send suspend signal to starter" );
		}
		break;

	case CLAIM_UNCLAIMED:
			// This is a programmer error.  we can't possibly get here  
		EXCEPT( "Trying to suspend a claim that was never claimed!" ); 
		break;

	case CLAIM_SUSPENDED:
		return sendErrorReply( s, "CA_SUSPEND_CLAIM",
							   CA_INVALID_STATE, 
							   "Claim is already suspended" );
		break;

	case CLAIM_IDLE:
			// it's not running a job, so we can't suspend it!
		return sendErrorReply( s, "CA_SUSPEND_CLAIM",
							   CA_INVALID_STATE, 
							   "Cannot suspend an inactive claim" );
		break;

	case CLAIM_VACATING:
	case CLAIM_KILLING:
		line = "Cannot suspend a claim that is being evicted (";
		line += getClaimStateString( claim->state() );
		line += ')';
		return sendErrorReply( s, "CA_SUSPEND_CLAIM",
							   CA_INVALID_STATE, line.c_str() );
		break;
	}

	return FALSE;
}

int
CODMgr::renew( Stream* s, ClassAd* /*req*/ /* UNUSED*/, Claim* claim )
{
	ClassAd reply;

	claim->alive();

	reply.Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );

	return sendCAReply( s, "CA_RENEW_LEASE_FOR_CLAIM", &reply );
}

int
CODMgr::resume( Stream* s, ClassAd* /*req*/ /*UNUSED*/, Claim* claim )
{
	int rval;
	ClassAd reply;
	std::string line;

	switch( claim->state() ) {

	case CLAIM_SUSPENDED:
			// now that a COD job is about to start running again,
			// invoke our helper to deal w/ interactions w/ the
			// opportunistic claim.  we want to do this before we
			// resume the cod job, since we don't want both to run
			// simultaneously, even if it's only for a very short
			// period of time.
		interactionLogicCODRunning();
		if( claim->resumeClaim() ) { 
			reply.Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );

				// TODO any other info for the reply?
			return sendCAReply( s, "CA_RESUME_CLAIM", &reply );

		} else {
			rval = sendErrorReply( s, "CA_RESUME_CLAIM",
								   CA_FAILURE, 
						   "Failed to send resume signal to starter" );
				// since we didn't really suspend, let the
				// opportunistic job try to run again... 
			interactionLogicCODStopped();
			return rval;
		}
		break;

	case CLAIM_UNCLAIMED:
			// This is a programmer error.  we can't possibly get here  
		EXCEPT( "Trying to resume a claim that was never claimed!" ); 
		break;

	case CLAIM_RUNNING:
		return sendErrorReply( s, "CA_RESUME_CLAIM",
							   CA_INVALID_STATE, 
							   "Claim is already running" );
		break;


	case CLAIM_IDLE:
			// it's not running a job, so we can't resume it!
		return sendErrorReply( s, "CA_RESUME_CLAIM",
							   CA_INVALID_STATE, 
							   "Cannot resume an inactive claim" );
		break;

	case CLAIM_VACATING:
	case CLAIM_KILLING:
		line = "Cannot resume a claim that is being evicted (";
		line += getClaimStateString( claim->state() );
		line += ')';
		return sendErrorReply( s, "CA_RESUME_CLAIM",
							   CA_INVALID_STATE, line.c_str() );
		break;

	}

	return FALSE;
}


void
CODMgr::interactionLogicCODRunning( void )
{
	if( rip->isSuspendedForCOD() ) {
			// already suspended, nothing to do
		return;
	}
	rip->suspendForCOD();
}


void
CODMgr::interactionLogicCODStopped( void )
{
	if( isRunning() ) {
			// we've still got something running, nothing to do.
		return;
	} 
	rip->resumeForCOD();
}
