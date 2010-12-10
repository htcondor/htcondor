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

#ifndef USER_ERROR_POLICY_H
#define USER_ERROR_POLICY_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "user_job_policy.h"

//
// Error Actions Codes
//
enum {	ERROR_ACTION_DEFAULT = 0,
		ERROR_ACTION_RETRY,
		ERROR_ACTION_HOLD,
		ERROR_ACTION_REMOVE,
		ERROR_ACTION_UNKNOWN
};
//
// Error Actions String Versions
// These will be converted to the appropriate int values 
// by condor_submit. These are defined in user_error_policy.C
//
extern const char ERROR_ACTION_DEFAULT_STR[];
extern const char ERROR_ACTION_RETRY_STR[];
extern const char ERROR_ACTION_HOLD_STR[];
extern const char ERROR_ACTION_REMOVE_STR[];

/**
 * This object is used to determine which actions to take when a job
 * error occurs. The user can specify an action that they would like
 * to occur when an error is encountered:
 * 
 * 	ERROR_ACTION_DEFAULT:
 * 	This will cause UserErrorPolicy to use whatever the internal default
 * 	action that is specified by Condor when the error is registed with the
 * 	job. This default action can be found in the ATTR_ERROR_ACTION_DEFAULT
 * 	job attribute.
 * 
 * 	ERROR_ACTION_RETRY:
 * 	Condor will attempt to put the job back into the state that it
 * 	was in when the error occured. For instance, if the job was running
 * 	when the error occured, then it will be requeued.
 * 
 * 	ERROR_ACTION_HOLD:
 * 	The job is always put on hold
 * 
 * 	ERROR_ACTION_REMOVE:
 * 	The job is always removed from the queue.
 *
 * To use this object, you will want to first initialize it with the ClassAd
 * that you want to use, then call analyzePolicy() to get back the action 
 * code. We will use the action identifiers from UserJobPolicy:
 * 
 * 	STAYS_IN_QUEUE
 * 	REMOVE_FROM_QUEUE
 * 	HOLD_IN_QUEUE
 * 	UNDEFINED_EVAL
 * 	RELEASE_FROM_HOLD
 * 
 * @see user_job_policy.h
 **/
class ErrorPolicy {
	public:
			/**
			 * Constructor
			 * Just set up our data members
			 **/
		ErrorPolicy();

			/**
			 * Deconstructor
			 * We do not want to free the memory for the job ad
			 **/		
		~ErrorPolicy();
		
			/**
			 * This is a static helper method that can be used to look in
			 * a ClassAd and see if the ad contains error infomation. If it
			 * does then we will return true.
			 * 
			 * @param ad - the ClassAd to check for error information
			 * @return true if the ClassAd has an error code, false otherwise
			 **/
		static bool containsError( ClassAd *ad );

			/**
			 * This class NEVER owns this memory, it just has a reference
			 * to it. It also makes sure the default policy expressions
			 * are set in the classad if they were undefined. This must be
			 * called FIRST when you initially set up one of these classes.
			 * 
			 **/
		void init( ClassAd *ad );

			/**
			 * After we have been initialized with a ClassAd, we now can
			 * look at the ErrorAction attribute for the ad and determine what
			 * should be done with the job. If the ClassAd does not have the 
			 * ATTR_ERROR_REASON_CODE defined then we are unable to proceed.
			 * If this code was set to default, then we need to look at the 
			 * ATTR_ERROR_ACTION_DEFAULT attribute which is provided by the
			 * developer who set up the trigger for the error. We will use
			 * this action code if the user specified for us to take the
			 * default action.
			 * 
			 * Now we based on our ClassAd's action code we will return a queue 
			 * action, as defined by UserJobPolicy.
			 * 
			 * @return an action code that specifies what should be done with this job
			 * @see user_job_policy.h
			 **/
		int analyzePolicy();
		
			/**
			 * Based on the action code that was pulled from the ClassAd
			 * in analyzePolicy() we will generate a reason message that
			 * can be used by whomever to  add to the job's ad when preforming
			 * the appropriate action. For instance, if the action code was
			 * ATTR_ERROR_REASON_CODE, then when the job goes on hold the
			 * HoldReason can have the error action reason that we are
			 * generating here. If no firing expression occurred, then 
			 * the reason that is returned will be empty
			 * 
			 * @return the reason why we generated the action code that we did
			 **/
		MyString errorActionReason(void);

			/**
			 * An accessor method to get what the calculated action code is for
			 * this object. This should be called after analyzePolicy()
			 * 
			 * @return the current action code determined for this object
			 **/
		int errorAction( void );

			/**
			 * The user can specify to use whatever the internal action
			 * code is for an error. When this occurs, we have to store
			 * what the default action was into the action data member for
			 * our object. So in order to retain the fact that it was
			 * original default action code that made us do whatever the new
			 * action code is, we use the action_default flag. This will cause
			 * us to generate a more verbose action reason later on.
			 * 
			 * @return true if the ClassAd specified to use the default action
			 **/
		bool errorActionDefault( void );

	protected:

			/**
			 * If the ClassAd that our object was initialized with doesn't
			 * have the ATTR_ERROR_ACTION, then we will insert the default
			 * action code
			 **/
		void setDefaults(void);

			/**
			 * A static helper method that converts an action code integer into
			 * its string representation. We will return an "Unknown" string if
			 * we are given a wrong action code 
			 * 
			 * @param action - the code that should be converted into a string
			 * @return the string version of the action code
			 **/
	 	static MyString actionCodetoString( int action );

		/* I can't be copied */
		ErrorPolicy(const ErrorPolicy&);
		ErrorPolicy& operator=(const ErrorPolicy&);

			///
			/// The job ad that will analyze 
			///
		ClassAd *ad;
			///
			/// The action code generated by analyzePolicy()
			///
		int action;
			///
			/// Whether the ErrorAction attribute said to use the internal default
			///
		bool action_default;
			///
			/// What state the job was in when we evaluated the error information
			/// This is necessary so that we don't do certain things like
			/// release jobs that were alreay on hold
			///
		int job_state;		
}; // END CLASS

#endif
