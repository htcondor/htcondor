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


#include "condor_common.h"
#include "baseshadow.h"
#include "shadow_user_policy.h"
#include "MyString.h"
#include "condor_holdcodes.h"


ShadowUserPolicy::ShadowUserPolicy() : BaseUserPolicy()
{
	this->shadow = NULL;

}


ShadowUserPolicy::~ShadowUserPolicy()
{
	//BaseUserPolicy::~BaseUserPolicy();
		// Don't touch the memory for the job_ad and shadow, since
		// we're not responsible for that
}


void
ShadowUserPolicy::init( ClassAd *job_ad_ptr, BaseShadow *shadow_ptr )
{
	BaseUserPolicy::init( job_ad_ptr );
	shadow = shadow_ptr;
}

int
ShadowUserPolicy::getJobBirthday( ) 
{
	int bday = 0;
	if ( this->job_ad ) {
		this->job_ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, bday );
	}
	return ( bday );
}

void
ShadowUserPolicy::doAction( int action, bool is_periodic ) 
{
	MyString reason = this->user_policy.FiringReason();
	if ( reason.IsEmpty() ) {
		EXCEPT( "ShadowUserPolicy: Empty FiringReason." );
	}

	switch( action ) {
	case UNDEFINED_EVAL:
			/* 
			   If a given policy expression was undefined, the user
			   screwed something up and they better deal with it.  We
			   don't just want the job to leave the queue...
			*/
		shadow->holdJob( reason.Value(), CONDOR_HOLD_CODE_JobPolicyUndefined, 0 );
		break;

	case REMOVE_FROM_QUEUE:
		if( is_periodic ) {
			this->shadow->removeJob( reason.Value() );
		} else {
			this->shadow->terminateJob();
		}
		break;

	case STAYS_IN_QUEUE:
		if( is_periodic ) {
			EXCEPT( "STAYS_IN_QUEUE should never be handled by "
					"periodic doAction()" );
		}
		this->shadow->requeueJob( reason.Value() );
		break;

	case HOLD_IN_QUEUE:
		shadow->holdJob( reason.Value(), CONDOR_HOLD_CODE_JobPolicy, 0 );
		break;

	default:
		EXCEPT( "Unknown action (%d) in ShadowUserPolicy::doAction", 
				action );
	}
}

