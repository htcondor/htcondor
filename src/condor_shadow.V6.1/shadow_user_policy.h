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

#ifndef _SHADOW_USER_POLICY_H
#define _SHADOW_USER_POLICY_H

#include "condor_common.h"
#include "baseshadow.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_classad.h"
#include "user_job_policy.h"



class BaseShadow;

/** A class for dealing with the user's specified policy for when the
	job should be put on hold, removed, or re-queued.  
	@see UserPolicy
	@author Derek Wright
*/
class ShadowUserPolicy : public Service
{
 public:

		/// Default constructor
	ShadowUserPolicy();

		/// Destructor
	~ShadowUserPolicy();

		/** Initialize this class.  
		 */
	void init( ClassAd* job_ad_ptr, BaseShadow* shadow_ptr );

		/// Start a timer to evaluate the periodic policy
	void startTimer( void );

		/// Cancel our periodic timer
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

 private:

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

	void UpdateJobTime( float *old_run_time );
	void RestoreJobTime( float old_run_time );

		// Data
	UserPolicy user_policy;
	BaseShadow* shadow;
	ClassAd* job_ad;
	int tid;

};

#endif /* _SHADOW_USER_POLICY_H */
