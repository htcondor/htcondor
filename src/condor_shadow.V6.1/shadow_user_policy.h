/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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

		// Data
	UserPolicy user_policy;
	BaseShadow* shadow;
	ClassAd* job_ad;
	int tid;

};

#endif /* _SHADOW_USER_POLICY_H */
