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


#include "condor_common.h"
#include "baseshadow.h"
#include "shadow_user_policy.h"
#include "MyString.h"


ShadowUserPolicy::ShadowUserPolicy() 
{
	tid = -1;
	job_ad = NULL;
	shadow = NULL;

}


ShadowUserPolicy::~ShadowUserPolicy() 
{
	cancelTimer();
		// Don't touch the memory for the job_ad and shadow, since
		// we're not responsible for that
}


void
ShadowUserPolicy::init( ClassAd* job_ad_ptr, BaseShadow* shadow_ptr )
{
	job_ad = job_ad_ptr;
	shadow = shadow_ptr;
	user_policy.Init( job_ad );
}


void
ShadowUserPolicy::cancelTimer( void )
{
	if( tid != -1 ) {
		daemonCore->Cancel_Timer( tid );
		tid = -1;
	}
}


void
ShadowUserPolicy::startTimer( void )
{
		// first, make sure we don't already have a timer running
	cancelTimer();

		// now, register a periodic timer to evaluate the user policy
	tid = daemonCore->
		Register_Timer(20, 20, (TimerHandlercpp)
					   &ShadowUserPolicy::checkPeriodic,
					   "checkPeriodic", this );
	if( tid < 0 ) {
        EXCEPT( "Can't register DC timer!" );
    }
}


void
ShadowUserPolicy::checkAtExit( void )
{
	float old_run_time;
	UpdateJobTime(&old_run_time);

	int action = user_policy.AnalyzePolicy( PERIODIC_THEN_EXIT );

	RestoreJobTime(old_run_time);

		// All we have to do now is perform the appropriate action.
		// Since this is all shared code w/ the periodic case, we just
		// call a helper function to do the real work.  This helper
		// will not return... it'll eventually result in a DC_Exit(). 
	doAction( action, false );
}


void
ShadowUserPolicy::checkPeriodic( void )
{
	float old_run_time;
	UpdateJobTime(&old_run_time);

	int action = user_policy.AnalyzePolicy( PERIODIC_ONLY );

	RestoreJobTime(old_run_time);

	if( action == STAYS_IN_QUEUE ) {
			// at periodic evaluations, this is the normal case: the
			// job should stay in the queue.  so, there's nothing
			// special to do, we'll just return now.
		return;
	}
	
		// if we're supposed to do anything else with the job, we
		// need to perform some actions now, so call our helper:
	doAction( action, true );
}


/*Before evaluating user policy expressions, temporarily update
  any stale time values.  Currently, this is just RemoteWallClock.
*/
void
ShadowUserPolicy::UpdateJobTime( float *old_run_time )
{
  float previous_run_time,total_run_time;
  int shadow_bday;
  time_t now = time(NULL);
  char buf[100];

  if(!job_ad) return;

  job_ad->LookupInteger(ATTR_SHADOW_BIRTHDATE,shadow_bday);
  job_ad->LookupFloat(ATTR_JOB_REMOTE_WALL_CLOCK,previous_run_time);

  if(old_run_time) *old_run_time = previous_run_time;
  total_run_time = previous_run_time;
  if(shadow_bday) total_run_time += (now - shadow_bday);

  sprintf(buf,"%s = %f",ATTR_JOB_REMOTE_WALL_CLOCK,total_run_time);
  job_ad->InsertOrUpdate(buf);
}

/*After evaluating user policy expressions, this is
  called to restore time values to their original state.
*/
void
ShadowUserPolicy::RestoreJobTime( float old_run_time )
{
  char buf[100];

  if(!job_ad) return;

  sprintf(buf,"%s = %f",ATTR_JOB_REMOTE_WALL_CLOCK,old_run_time);
  job_ad->InsertOrUpdate(buf);
}

void
ShadowUserPolicy::doAction( int action, bool is_periodic ) 
{
	const char* firing_expr = user_policy.FiringExpression();
//	char* tmp = NULL;
	const char* tmp;
//	ExprTree *tree, *rhs = NULL;
	ExprTree *tree;
	ClassAdUnParser unp;
	std::string bufString;

	tree = job_ad->Lookup( firing_expr );
//	if( tree && (rhs=tree->RArg()) ) {
	if( tree ) {
			// cool, we've found the right expression.  let's print it
			// out to a string so we can use it.
//		rhs->PrintToNewStr( &tmp );
		unp.Unparse( bufString, tree );
		tmp = bufString.c_str( );
	} else {
			// this is really strange, we think the firing expr went
			// off, but now we can't look it up in the job ad.
		EXCEPT( "ShadowUserPolicy: Can't find %s in job ClassAd\n",
				firing_expr );
	}

	int firing_value = user_policy.FiringExpressionValue();

	MyString reason = "The ";
	reason += firing_expr;
	reason += " expression '";
	reason += tmp;
//	free( tmp );

	reason += "' evaluated to ";
	switch( firing_value ) {
	case 0:
		reason += "FALSE";
		break;
	case 1:
		reason += "TRUE";
		break;
	case -1:
		reason += "UNDEFINED";
		break;
	default:
		EXCEPT( "Unrecognized FiringExpressionValue: %d", 
				firing_value ); 
		break;
	}

	switch( action ) {
	case UNDEFINED_EVAL:
			/* 
			   If a given policy expression was undefined, the user
			   screwed something up and they better deal with it.  We
			   don't just want the job to leave the queue...
			*/
		shadow->holdJob( reason.Value() );
		break;

	case REMOVE_FROM_QUEUE:
		if( is_periodic ) {
			shadow->removeJob( reason.Value() );
		} else {
			shadow->terminateJob();
		}
		break;

	case STAYS_IN_QUEUE:
		if( is_periodic ) {
			EXCEPT( "STAYS_IN_QUEUE should never be handled by "
					"periodic doAction()" );
		}
		shadow->requeueJob( reason.Value() );
		break;

	case HOLD_IN_QUEUE:
		shadow->holdJob( reason.Value() );
		break;

	default:
		EXCEPT( "Unknown action (%d) in ShadowUserPolicy::doAction", 
				action );
	}
}

