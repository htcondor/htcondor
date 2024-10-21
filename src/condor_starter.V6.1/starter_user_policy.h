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


#ifndef STARTER_USER_POLICY_H
#define STARTER_USER_POLICY_H

#include "condor_user_policy.h"
#include "condor_common.h"
#include "condor_classad.h"
#include "starter.h"
#include "job_info_communicator.h"
#include "condor_daemon_core.h"

class JobInfoCommunicator;

/** 
 * Policy Expressions for the Starter
 * Allows the Starter to do periodic and check on exit
 * expression evaluation. The object communicates with a JIC
 * to perform the necessary actions
 * 
 * @see BaseUserPolicy
 * @author Andy Pavlo - pavlo@cs.wisc.edu
*/
class StarterUserPolicy : public BaseUserPolicy
{
	public:
			/**
			 * Default constructor
			 */
		StarterUserPolicy();
	
			/**
			 * Destructor
			 */
		~StarterUserPolicy();

			/**
			 * Our object needs a job ad to act on and a JIC to 
			 * talk to make the Starter do certain things. We will call
			 * the BaseUserPolicy object's init() method make sure
			 * we setup things correcly
			 * 
			 * @param job_ad_ptr - the job ad to use for policy evaluations
			 * @param jic - the JIC object that doAction() communicates with
			 **/
		void init( ClassAd*, JobInfoCommunicator* );
	
	protected:
			/**
			 * This is the heart of the policy object. When an expression
			 * evaluates to true in either checkAtExit() or checkPeriodic(), 
			 * doAction() will call the JIC to do whatever it is the action
			 * called for.
			 * 
			 * @param action - what action we should perform.
			 * @param is_periodic - whether it was fired from checkPeriodic()
			 * @see UserPolicy
			 */
		void doAction( int action, bool is_periodic );
		
		/**
		 * Figure out the when the job was started. The times will be
		 * the number of seconds elapsed since 00:00:00 on January 1, 1970,
		 * Coordinated Universal Time (UTC)
		 * 
		 * @return the UTC timestamp of the job's birthday
		 **/
		time_t getJobBirthday( );
		
			/**
			 * This is the object that we will use to communicate
			 * to the Starter the various actions to take on a job
			 **/
		JobInfoCommunicator *jic;
		
}; // END CLASS

#endif // STARTER_USER_POLICY_H
