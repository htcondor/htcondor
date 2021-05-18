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
#include "condor_user_policy.h"
#include "MyString.h"

/**
 * Constructor
 * Just initializes the data members. You'll want to use
 * init() to setup the object properly
 **/
BaseUserPolicy::BaseUserPolicy() 
{
	this->tid = -1;
	this->job_ad = NULL;
	this->interval = DEFAULT_PERIODIC_EXPR_INTERVAL;
}

/**
 * Deconstructor
 * All we really do is cancel our periodic timer.
 * We do not want to free up any memory for the job ad.
 **/
BaseUserPolicy::~BaseUserPolicy() 
{
	this->cancelTimer();
		// Don't touch the memory for the job_ad since
		// we're not responsible for that
}

/**
 * Sets up our object for a given job ad.
 * 
 * @param job_ad_ptr - the job ad to use for policy evaluations
 **/
void
BaseUserPolicy::init( ClassAd* job_ad_ptr )
{
	this->job_ad = job_ad_ptr;
	this->user_policy.Init();
	this->interval = param_integer("PERIODIC_EXPR_INTERVAL",
								   DEFAULT_PERIODIC_EXPR_INTERVAL);
}

/**
 * If we have a periodic timer instantiated, we'll cancel it
 **/
void
BaseUserPolicy::cancelTimer( void )
{
	if ( daemonCore && this->tid != -1 ) {
		daemonCore->Cancel_Timer( this->tid );
		this->tid = -1;
	}
}

/**
 * Starts the periodic evaluation timer. The interval is 
 * defined by PERIODIC_EXPR_INTERVAL
 **/
void
BaseUserPolicy::startTimer( void )
{
		// first, make sure we don't already have a timer running
	this->cancelTimer();

		//
		// We will only start the timer if the interval is greater than zero
		//
	if ( this->interval > 0 ){
		this->tid = daemonCore->
			Register_Timer( this->interval,
							this->interval,
							(TimerHandlercpp)&BaseUserPolicy::checkPeriodic,
							"checkPeriodic",
							this );
		if ( this->tid < 0 ) {
			EXCEPT( "Can't register DC timer!" );
		}
		dprintf(D_FULLDEBUG, "Started timer to evaluate periodic user "
				"policy expressions every %d seconds\n", this->interval);
	}
}

/**
 * This is to be called when a job is exiting from a daemon
 * We pass the action id we get back from the user_policy object
 * to the derived object's doAction() method
 **/
void
BaseUserPolicy::checkAtExit( void )
{
	double old_run_time;
	this->updateJobTime( &old_run_time );

	int action = this->user_policy.AnalyzePolicy( *(this->job_ad), PERIODIC_THEN_EXIT );
	this->restoreJobTime( old_run_time );

		//
		// All we have to do now is perform the appropriate action.
		// Since this is all shared code w/ the periodic case, we just
		// call a helper function to do the real work.
		//
	this->doAction( action, false );
}

/**
 * Checks the periodic expressions for the job ad, and if there
 * is anything we need to do, we'll call the derived object's doAction()
 * method.
 **/
void
BaseUserPolicy::checkPeriodic( void )
{
	double old_run_time;
	this->updateJobTime( &old_run_time );

	int action = this->user_policy.AnalyzePolicy( *(this->job_ad), PERIODIC_ONLY );

	this->restoreJobTime( old_run_time );

	if ( action == STAYS_IN_QUEUE ) {
			// at periodic evaluations, this is the normal case: the
			// job should stay in the queue.  so, there's nothing
			// special to do, we'll just return now.
		return;
	}
	
		// if we're supposed to do anything else with the job, we
		// need to perform some actions now, so call our helper:
	this->doAction( action, true );
}

/**
 * Before evaluating user policy expressions, temporarily update
 * any stale time values.  Currently, this is just RemoteWallClock.
 * 
 * @param old_run_time - we will put the job's old run time in this
 **/
void
BaseUserPolicy::updateJobTime( double *old_run_time )
{
	if ( ! this->job_ad ) {
		return;
	}

	double previous_run_time=0.0;
	time_t now = time(NULL);

	job_ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_run_time );

		//
		// The objects that extend this class will need to 
		// implement how we can determine the start time for
		// a job
		//
	int bday = this->getJobBirthday( );
	
	double total_run_time = previous_run_time;
	if ( old_run_time ) {
		*old_run_time = previous_run_time;
	}
	if ( bday ) {
		total_run_time += (now - bday);
	}
	
	this->job_ad->Assign(ATTR_JOB_REMOTE_WALL_CLOCK, total_run_time);
}

/**
 * After evaluating user policy expressions, this is
 * called to restore time values to their original state.
 * 
 * @param old_run_time - the run time to put back into the ad
 **/
void
BaseUserPolicy::restoreJobTime( double old_run_time )
{
	if ( ! this->job_ad ) {
		return;
	}

	this->job_ad->Assign(ATTR_JOB_REMOTE_WALL_CLOCK, old_run_time);
}
