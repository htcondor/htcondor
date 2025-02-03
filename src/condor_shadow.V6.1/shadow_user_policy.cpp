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
#include "baseshadow.h"
#include "shadow_user_policy.h"
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

time_t
ShadowUserPolicy::getJobBirthday( )
{
	time_t bday = 0;
	if ( this->job_ad ) {
		this->job_ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, bday );
	}
	return ( bday );
}

void
ShadowUserPolicy::doAction( int action, bool is_periodic )
{
	std::string reason;
	int reason_code;
	int reason_subcode;
	this->user_policy.FiringReason(reason,reason_code,reason_subcode);
	if ( reason.empty() ) {
		EXCEPT( "ShadowUserPolicy: Empty FiringReason." );
	}

	switch( action ) {
	case UNDEFINED_EVAL:
	case HOLD_IN_QUEUE:
		if( is_periodic ) {
			shadow->holdJob( reason.c_str(), reason_code, reason_subcode );
		}
		else {
			shadow->holdJobAndExit( reason.c_str(), reason_code, reason_subcode );
		}
		break;

	case REMOVE_FROM_QUEUE:
		if( is_periodic ) {
			this->shadow->removeJob( reason.c_str() );
		} else {
			this->shadow->terminateJob();
		}
		break;

	case STAYS_IN_QUEUE:
		if( is_periodic ) {
			EXCEPT( "STAYS_IN_QUEUE should never be handled by "
					"periodic doAction()" );
		}
		this->shadow->requeueJob( reason.c_str() );
		break;

	case VACATE_FROM_RUNNING:
		if (reason_code == CONDOR_HOLD_CODE::JobPolicy) {
			reason_code = CONDOR_HOLD_CODE::JobPolicyVacate;
		} else if (reason_code == CONDOR_HOLD_CODE::SystemPolicy) {
			reason_code = CONDOR_HOLD_CODE::SystemPolicyVacate;
		}
		this->shadow->evictJob(JOB_SHOULD_REQUEUE, reason.c_str(), reason_code, reason_subcode);
		break;
	default:
		EXCEPT( "Unknown action (%d) in ShadowUserPolicy::doAction",
				action );
	}
}
