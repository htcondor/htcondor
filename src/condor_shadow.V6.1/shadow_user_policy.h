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


#ifndef _SHADOW_USER_POLICY_H
#define _SHADOW_USER_POLICY_H

#include "condor_user_policy.h"
#include "condor_common.h"
#include "condor_classad.h"

class BaseShadow;

/** A class for dealing with the user's specified policy for when the
	job should be put on hold, removed, or re-queued.  
	@see UserPolicy
	@author Derek Wright
*/
class ShadowUserPolicy : public BaseUserPolicy
{
 public:

		/// Default constructor
	ShadowUserPolicy();

		/// Destructor
	~ShadowUserPolicy();

		/** Initialize this class.  
		 */
	void init( ClassAd* job_ad_ptr, BaseShadow* shadow_ptr );

 protected:

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
	void doAction( int action, bool is_periodic );
	
		/**
		 * Figure out the when the job was started. The times will be
		 * the number of seconds elapsed since 00:00:00 on January 1, 1970,
		 * Coordinated Universal Time (UTC)
		 * 
		 * @return the UTC timestamp of the job's birthday
		 **/
	time_t getJobBirthday( );

		// Data
	BaseShadow* shadow;

};

#endif /* _SHADOW_USER_POLICY_H */
