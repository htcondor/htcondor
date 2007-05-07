/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef _SHADOW_USER_POLICY_H
#define _SHADOW_USER_POLICY_H

#include "condor_user_policy.h"
#include "condor_common.h"
#include "baseshadow.h"
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
	int getJobBirthday( );

		// Data
	BaseShadow* shadow;

};

#endif /* _SHADOW_USER_POLICY_H */
