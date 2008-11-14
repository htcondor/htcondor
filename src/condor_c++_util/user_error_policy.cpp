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
#include "condor_classad.h"
#include "condor_classad_util.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "user_error_policy.h"
#include "user_job_policy.h"

//
// Error Actions String Versions
// These will be converted to the appropriate int values 
// by condor_submit
//
const char ERROR_ACTION_DEFAULT_STR	[] = "Default";
const char ERROR_ACTION_RETRY_STR	[] = "Retry";
const char ERROR_ACTION_HOLD_STR	[] = "Hold";
const char ERROR_ACTION_REMOVE_STR	[] = "Remove";

/**
 * Constructor
 * Just set up our data members
 **/
ErrorPolicy::ErrorPolicy()
{
	this->ad = NULL;
	this->action = ERROR_ACTION_UNKNOWN;
	this->action_default = false;
}

/**
 * Deconstructor
 * We do not want to free the memory for the job ad
 **/
ErrorPolicy::~ErrorPolicy()
{
	this->ad = NULL;
}

/**
 * Given a ClassAd, we call setDefaults() to add in any attributes
 * that are necssary. This follows the same paradigm from UserJobPolicy
 * This method must be called before analyzePolicy()
 * 
 * @param ad - the ClassAd that we will analyze for error information
 **/
void
ErrorPolicy::init( ClassAd *ad )
{
	this->ad = ad;
	this->setDefaults();
}

/**
 * This is a static helper method that can be used to look in
 * a ClassAd and see if the ad contains error infomation. If it
 * does then we will return true.
 * 
 * @param ad - the ClassAd to check for error information
 * @return true if the ClassAd has an error code, false otherwise
 **/
bool
ErrorPolicy::containsError( ClassAd *ad )
{
	return ( ad->Lookup( ATTR_ERROR_REASON_CODE ) );
}

/**
 * If the ClassAd that our object was initialized with doesn't
 * have the ATTR_ERROR_ACTION, then we will insert the default
 * action code
 **/
void
ErrorPolicy::setDefaults( )
{
		//
		// Make sure we actually have a ClassAd to work with
		//
	if ( this->ad != NULL ) {
			//
			// If they didn't specify an ErrorAction, we'll use
			// our default value
			//
		if ( this->ad->Lookup( ATTR_ERROR_ACTION ) == NULL) {
			this->ad->Assign( ATTR_ERROR_ACTION, ERROR_ACTION_DEFAULT );
		}
	}
}

/**
 * After we have been initialized with a ClassAd, we now can
 * look at the ErrorAction attribute for the ad and determine what
 * should be done with the job. If the ClassAd does not have the 
 * ATTR_ERROR_REASON_CODE defined then we are unable to proceed.
 * If this code was set to default, then we need to look at the 
 * ATTR_ERROR_ACTION_DEFAULT attribute which is provided by the developer
 * who set up the trigger for the error. We will use this action code
 * if the user specified for us to take the default action.
 * 
 * Now we based on our ClassAd's action code we will return a queue 
 * action, as defined by UserJobPolicy.
 * 
 * @return an action code that specifies what should be done with this job
 * @see user_job_policy.h
 **/
int
ErrorPolicy::analyzePolicy( )
{
	int error_code;
	MyString error_reason;
	int ret = UNDEFINED_EVAL;
	int cluster, proc;

		//
		// Make sure we have a job add before we try to do anything
		//
	if ( this->ad == NULL) {
		EXCEPT( "ErrorPolicy: analyzePolicy() called before init()!" );
	}
		//
		// To make our output more useful, let's provide the job info
		//
	this->ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	this->ad->LookupInteger( ATTR_PROC_ID, proc );
	
		//
		// We need to get the job state out of the ad first
		// This is important because if they specify RETRY, then
		// we need to make sure we do the right action. We save the 
		// state in our object because it's important to know the 
		// state of the job at the moment we are being called to know
		// why we took the action that we did
		//
		// If there is no state, then we need to EXCEPT because this
		// is a big problem
		//
	if ( ! this->ad->LookupInteger( ATTR_JOB_STATUS, this->job_state ) ) {
		MyString error;
		error.sprintf( "ErrorPolicy: Job %d.%d does not have a state!\n",
						cluster, proc );
		EXCEPT( (char*)error.Value() );
	}
	
		//
		// Now make sure we have at least an error code in our 
		// job ad. If there isn't one, then we don't need to do
		// anything. I think it's appropriate to send a message 
		// in the error log, but we don't want to except because that would
		// take down this daemon, which is something we probably don't
		// want to do for this minor mistake
		//
	if ( ! this->ad->LookupInteger( ATTR_ERROR_REASON_CODE, error_code ) ) {
		dprintf( D_FULLDEBUG, "ErrorPolicy: Job %d.%d does not have an error code. "
				"Skipping\n",
				cluster, proc );
		return ( ret );
	}
	this->ad->LookupString( ATTR_ERROR_REASON, error_reason );
	
		//
		// Get the action we should take. If there isn't one
		// then we won't do anything
		//
	if ( ! this->ad->LookupInteger( ATTR_ERROR_ACTION, this->action ) ) {
		dprintf( D_ALWAYS, "ErrorPolicy: Job %d.%d does not have an ErrorAction. "
				"Skipping\n",
				cluster, proc );
		return ( ret );
	}
	
		//
		// We need to check first if we are to use Condor's default
		// action for handling this error.
		//
	if ( this->action == ERROR_ACTION_DEFAULT ) {
			//
			// Mark a flag so that we know that we were originally
			// told to use the default action for this error. This will
			// be useful later on if we need to figure out why we did
			// what we did
			//
		this->action_default = true;
			//
			// We need to pull out what Condor says we should be
			// doing in this case. If it the default isn't defined
			// then we need to....
			//
		if ( ! this->ad->LookupInteger( ATTR_ERROR_ACTION_DEFAULT,
									  	this->action ) ) {
			// Need to ask somebody!
		}
	}
	
		//
		// Based on their ErrorAction, determine the action 
		// that they should take with their job
		//
	switch ( this->action ) {
		// --------------------------------------------
		// ERROR_ACTION_DEFAULT
		// This is just a sanity check to make sure that the 
		// default action isn't set to be the default action,
		// which is something that should never happen and 
		// a developer made a mistake somewhere
		// --------------------------------------------
		case ERROR_ACTION_DEFAULT: {
				//
				// Let's try to be helpful and provide the error
				// code and reason
				//
			MyString error = "ErrorJob: The default action was set to ";
			error += "ERROR_ACTION_DEFAULT!\n";
			error += "ErrorCode   = ";
			error += error_code;
			error += "ErrorReason = ";
			error += (!error_reason.IsEmpty() ? error_reason : "<EMPTY>");
			dprintf( D_ALWAYS, (char*)error.Value() );
			break;
		}
		// --------------------------------------------
		// ERROR_ACTION_RETRY
		// --------------------------------------------
		case ERROR_ACTION_RETRY:
				//
				//
				//
			switch ( this->job_state ) {
				case RUNNING:
					ret = STAYS_IN_QUEUE;
					break;
				case HELD:
					ret = HOLD_IN_QUEUE;
					break;
					
			} // SWITCH
		
			break;			
		// --------------------------------------------
		// ERROR_ACTION_HOLD
		// Always put the job on hold
		// --------------------------------------------
		case ERROR_ACTION_HOLD:
			ret = HOLD_IN_QUEUE;
			break;
		// --------------------------------------------
		// ERROR_ACTION_REMOVE
		// Always remove the job from the queue
		// --------------------------------------------
		case ERROR_ACTION_REMOVE:
			ret = REMOVE_FROM_QUEUE;
			break;
		// --------------------------------------------
		// UNKNOWN
		// --------------------------------------------
		default: {
				//
				// We just want to print an error instead of excepting
				// because condor_submit should have picked out any invalid
				// ErrorAction attributes.
				//
			MyString error;
			error.sprintf( "ErrorPolicy: "
						   "Job %d.%d has an unknown action code '%d'\n",
						   cluster, proc, this->action );
			dprintf( D_ALWAYS, (char*)error.Value() );
			break;
		}
	} // SWITCH

	return ( ret );
}

/**
 * An accessor method to get what the calculated action code is for
 * this object. This should be called after analyzePolicy()
 * 
 * @return the current action code determined for this object
 **/
int
ErrorPolicy::errorAction( )
{
	return ( this->action );
}

/**
 * The user can specify to use whatever the internal action code is
 * for an error. When this occurs, we have to store what the default
 * action was into the action data member for our object. So in order
 * to retain the fact that it was original default action code that made
 * us do whatever the new action code is, we use the action_default flag.
 * This will cause us to generate a more verbose action reason later on.
 * 
 * @return true if the ClassAd specified to use the default action
 **/
bool
ErrorPolicy::errorActionDefault( )
{
	return ( this->action_default );
}

/**
 * Based on the action code that was pulled from the ClassAd in analyzePolicy()
 * we will generate a reason message that can be used by whomever to 
 * add to the job's ad when preforming the appropriate action. For instance,
 * if the action code was ATTR_ERROR_REASON_CODE, then when the job
 * goes on hold the HoldReason can have the error action reason that
 * we are generating here. If no firing expression occurred, then 
 * the reason that is returned will be empty
 * 
 * @return the reason why we generated the action code that we did
 **/
MyString
ErrorPolicy::errorActionReason( )
{
	MyString reason;
	MyString action_str;

		//
		// If we don't have a job ad, or we haven't evaluated
		// it yet, then there is nothing we can do
		//
	if ( this->ad == NULL || this->action == ERROR_ACTION_UNKNOWN ) {
		return ( reason );
	}
	
	action_str = this->actionCodetoString( this->action );

		//
		// If the original action was set to do whatever the default was,
		// we need to make sure that we let them know
		//
	if ( this->action_default ) {
		reason.sprintf( "The job attribute %s was set to %s. "
						"The default action was set to %s for the job.",
						ATTR_ERROR_ACTION,
						ERROR_ACTION_DEFAULT_STR,
						action_str.Value() );
	} else {
		reason.sprintf( "The job attribute %s evaluated to %s.",
						ATTR_ERROR_ACTION,
						action_str.Value() );
	}

	return ( reason );
}

/**
 * A static helper method that converts an action code integer into
 * its string representation. We will return an "Unknown" string if
 * we are given a wrong action code 
 * 
 * @param action - the code that should be converted into a string
 * @return the string version of the action code
 **/
MyString
ErrorPolicy::actionCodetoString( int action ) {
	MyString ret;
	switch ( action ) {
		// --------------------------------------------
		// ERROR_ACTION_DEFAULT
		// --------------------------------------------
		case ERROR_ACTION_DEFAULT:
			ret = ERROR_ACTION_DEFAULT_STR;
			break;
		// --------------------------------------------
		// ERROR_ACTION_RETRY
		// --------------------------------------------
		case ERROR_ACTION_RETRY:
			ret = ERROR_ACTION_RETRY_STR;
			break;
		// --------------------------------------------
		// ERROR_ACTION_HOLD
		// --------------------------------------------
		case ERROR_ACTION_HOLD:
			ret = ERROR_ACTION_HOLD_STR;
			break;
		// --------------------------------------------
		// ERROR_ACTION_REMOVE
		// --------------------------------------------
		case ERROR_ACTION_REMOVE:
			ret = ERROR_ACTION_REMOVE_STR;
			break;
		// --------------------------------------------
		// UNKNOWN
		// --------------------------------------------
		default:
			ret = "UNKNOWN (invalid action)";
	} // SWITCH
	return ( ret );
}

