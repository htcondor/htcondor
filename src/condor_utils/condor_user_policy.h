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


#ifndef _CONDOR_USER_POLICY_H
#define _CONDOR_USER_POLICY_H

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "user_job_policy.h"
#include "condor_daemon_core.h"

///
/// Default value for PERIODIC_EXPR_INTERVAL
/// This is just in case we don't have a config value
///
#define DEFAULT_PERIODIC_EXPR_INTERVAL 60

/**
 * An abstract class for dealing with the user's specified policy for when the
 * job should be put on hold, removed, or re-queued. This is a wrapper
 * for the UserPolicy object
 * 
 * @see UserPolicy
 * @author Andy Pavlo
 * @author Derek Wright (original author)
*/
class BaseUserPolicy : public Service
{
 public:

		/// Default constructor
	BaseUserPolicy();

		/// Destructor
	virtual ~BaseUserPolicy();

		/**
		 * Start a timer to evaluate the periodic policy
		 * The timer interval is defined by PERIODIC_EXPR_INTERVAL
		 **/
	void startTimer( void );

		/**
		 * Cancel our periodic timer
		 **/
	void cancelTimer( void );

		/** Check the UserPolicy to see if we should do anything
			special with the job once it has exited.  This method will
			act on the return value from UserPolicy.AnalyzePolicy(),
			and perform all the necessary actions as appropriate.
		*/
	void checkAtExit( void );

		/** Check the UserPolicy to see if we should do anything
			special with the job as it is running.  This method will
			act on the return value from UserPolicy.AnalyzePolicy(),
			and perform all the necessary actions as appropriate.
			This method is registered as a periodic timer handler
			while the job is executing.
		*/
	void checkPeriodic( void );

 protected:

		/** Initialize this class.  
		 */
	void init( ClassAd*);


		/** This function is called whenever the UserPolicy code has
			decided to do something with the job.  In all cases, we
			want to figure out what policy expression caused the
			action, place that reason string into the jobAd, and
			invoke the shadow's appropriate method to perform the
			specified action (either hold, remove, or requeue).

			@param action What action we should perform.  The values
			used for this int are the same as the return values from
			UserPolicy::AnalyzePolicy(). 
		*/
	virtual void doAction( int action, bool is_periodic ) = 0;
	
		/**
		 * The derived classes need to define a method for how
		 * to determine the birthday for a job. For example,
		 * the ShadowUserPolicy will want to use the Shadow's
		 * birthday while the StarterUserPolicy might want to 
		 * use the JobStartDate
		 * 
		 * @return the UTC timestamp of the job's birthday
		 **/
	virtual int getJobBirthday( ) = 0;

		/**
		 * Before evaluating user policy expressions, temporarily update
		 * any stale time values. The point that is passed in
		 * while have the old run time for a job that was updated
		 **/
	void updateJobTime(double *old_run_time);
	
		/**
		 * Undos the time updates of updateJobTime()
		 * The method must be given the old run time to be put
		 * back into the ClassAd
		 **/
	void restoreJobTime(double old_run_time);

		// Data
	UserPolicy user_policy;
	ClassAd* job_ad;
	int tid;
	int interval;
};

#endif /* _CONDOR_USER_POLICY_H */
