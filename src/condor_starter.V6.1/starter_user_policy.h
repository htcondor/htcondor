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

#ifndef STARTER_USER_POLICY_H
#define STARTER_USER_POLICY_H

#include "condor_user_policy.h"
#include "condor_common.h"
#include "condor_classad.h"
#include "starter.h"
#include "job_info_communicator.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

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
		int getJobBirthday( );
		
			/**
			 * This is the object that we will use to communicate
			 * to the Starter the various actions to take on a job
			 **/
		JobInfoCommunicator *jic;
		
}; // END CLASS

#endif // STARTER_USER_POLICY_H
