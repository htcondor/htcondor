/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
#include "job_info_communicator.h"
#include "starter_user_policy.h"
#include "MyString.h"

/**
 * Constructor
 * Just initializes the data members. You'll want to use
 * init() to setup the object properly. We call the BaseUserPolicy
 * object's constructor explicitly so that our object is setup
 * correctly
 **/
StarterUserPolicy::StarterUserPolicy() : BaseUserPolicy()
{
	this->jic = NULL;
}

/**
 * Deconstructor
 * There is nothing we need to because the base object's 
 * deconstructor will be called implicitly
 **/
StarterUserPolicy::~StarterUserPolicy()
{
		// Don't touch the memory for the job_ad and starter, since
		// we're not responsible for that
}

/**
 * Our object needs a job ad to act on and a JIC to 
 * talk to make the Starter do certain things. We will call the
 * BaseUserPolicy object's init() method make sure we setup things correcly
 * 
 * @param job_ad_ptr - the job ad to use for policy evaluations
 * @param jic - the JIC object that doAction() communicates with
 **/
void
StarterUserPolicy::init( ClassAd *job_ad, JobInfoCommunicator *jic )
{
	BaseUserPolicy::init( job_ad );
	this->jic = jic;
}

/**
 * Figure out the when the job was started. The times will be
 * the number of seconds elapsed since 00:00:00 on January 1, 1970,
 * Coordinated Universal Time (UTC)
 * 
 * @return the UTC timestamp of the job's birthday
 **/
int
StarterUserPolicy::getJobBirthday( ) 
{
	int bday = 0;
	if ( this->job_ad ) {
			//
			// Now for some reason local universe jobs have a 
			// shadow birthdate, but I don't think that's what 
			// we want to use here. So we're going to use 
			// the JobStartDate attribute instead
			//
		this->job_ad->LookupInteger( ATTR_JOB_START_DATE, bday );
	}
	return ( bday );
}

/**
 * This is the heart of the policy object. When an expression
 * evaluates to true in either checkAtExit() or checkPeriodic(), 
 * doAction() will call the JIC to do whatever it is the action
 * called for.
 * 
 * @param action - the action ID of what we need to do
 * @param is_periodic - whether the action was fired from checkPeriodic()
 **/
void
StarterUserPolicy::doAction( int action, bool is_periodic ) 
{
	MyString reason = this->user_policy.FiringReason();
	if ( reason.IsEmpty() ) {
		EXCEPT( "StarterUserPolicy: Empty FiringReason." );
	}

	switch( action ) {
		// ---------------------------------
		// UNDEFINED_EVAL
		// ---------------------------------
		case UNDEFINED_EVAL:
				//
				// If a given policy expression was undefined, the user
				// screwed something up and they better deal with it. We
				// don't just want the job to leave the queue
				// So the this case should fall through into HOLD_IN_QUEUE
				//
		// ---------------------------------
		// HOLD_IN_QUEUE
		// ---------------------------------
		case HOLD_IN_QUEUE:
			this->jic->holdJob( reason.Value() );
			break;
		// ---------------------------------
		// REMOVE_FROM_QUEUE
		// ---------------------------------
		case REMOVE_FROM_QUEUE:
				//
				// We need to make a distinction that we 
				// are removing the job because it completed its
				// execution, or it was being removed by PERIODIC_REMOVE
				//
			if( is_periodic ) {
				this->jic->removeJob( reason.Value() );
			} else {
					//
					// I am passing the reason, but apparently 
					// it isn't necessary?
					//
				this->jic->terminateJob( reason.Value() );
			}
			break;
		// ---------------------------------
		// STAYS_IN_QUEUE
		// ---------------------------------
		case STAYS_IN_QUEUE:
			if( is_periodic ) {
				EXCEPT( "STAYS_IN_QUEUE should never be handled by "
						"periodic doAction()" );
			}
			this->jic->requeueJob( reason.Value() );
			break;

		// ---------------------------------		
		// UNKNOWN
		// ---------------------------------
		default:
			EXCEPT( "Unknown action (%d) in StarterUserPolicy::doAction", 
					action );
	} // SWITCH
	return;
}
