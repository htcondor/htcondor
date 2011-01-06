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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_classad.h"

static int defaultJobLeaseDuration = -1;

void SetDefaultJobLeaseDuration( int duration )
{
	defaultJobLeaseDuration = duration;
}

bool CalculateJobLease( const ClassAd *job_ad, int &new_expiration,
						int default_duration, time_t *renew_time )
{
	//int cluster,proc;
	int expire_received = -1;
	int expire_sent = -1;
	int lease_duration = defaultJobLeaseDuration;
	bool last_renewal_failed = false;

	if ( default_duration != -1 ) {
		lease_duration = default_duration;
	}

	if ( renew_time ) {
		*renew_time = (time_t)INT_MAX;
	}

	new_expiration = -1;

	//job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	//job_ad->LookupInteger( ATTR_PROC_ID, proc );
	job_ad->LookupInteger( ATTR_TIMER_REMOVE_CHECK, expire_received );
	job_ad->LookupInteger( ATTR_JOB_LEASE_EXPIRATION, expire_sent );
	job_ad->LookupBool( ATTR_LAST_JOB_LEASE_RENEWAL_FAILED,
						last_renewal_failed );
	job_ad->LookupInteger( ATTR_JOB_LEASE_DURATION,	lease_duration );

		// If we didn't receive a lease, there's no lease to renew
	if ( expire_received == -1 && lease_duration == -1 ) {
		//dprintf(D_FULLDEBUG,"*** (%d.%d) CalculateLease: no received lease\n",cluster,proc);
		return false;
	}

		// If the lease we sent expires within 10 seconds of the lease we
		// received, don't renew it.
	if ( expire_received != -1 && expire_sent + 10 >= expire_received ) {
		//dprintf(D_FULLDEBUG,"*** (%d.%d) CalculateLease: new lease close enough to old lease\n",cluster,proc);
		return false;
	}

	if ( last_renewal_failed && expire_sent != -1 ) {
		new_expiration = expire_sent;
		//dprintf(D_FULLDEBUG,"*** (%d.%d) CalculateLease: retry failed lease at %d\n",cluster,proc,(int)new_expiration);
		return true;
	}

	if ( lease_duration != -1 ) {
		int now = time(NULL);
		if ( expire_sent == -1 ) {
			//dprintf(D_FULLDEBUG,"    Starting sent lease\n");
			new_expiration = now + lease_duration;
		} else if ( expire_sent - now < ( ( lease_duration * 2 ) / 3 ) + 10 ) {
			//dprintf(D_FULLDEBUG,"    lease left(%d) < 2/3 duration(%d)\n",expire_sent-now,(lease_duration*2)/3);
			new_expiration = now + lease_duration;
		} else {
			//dprintf(D_FULLDEBUG,"*** (%d.%d) CalculateLease: no new lease at present\n",cluster,proc);
			if ( renew_time ) {
				*renew_time = expire_sent - ( ( ( lease_duration * 2 ) / 3 ) + 10 );
			}
			return false;
		}
	}

	if ( expire_received != -1 && ( new_expiration == -1 ||
									expire_received < new_expiration ) ) {
		//dprintf(D_FULLDEBUG,"    Cap sent lease to received lease\n");
		new_expiration = expire_received;
	}

	if ( new_expiration != -1 ) {
		//dprintf(D_FULLDEBUG,"*** (%d.%d) CalculateLease: new lease should expire at %d\n",cluster,proc,(int)new_expiration);
		return true;
	} else {
		//dprintf(D_FULLDEBUG,"*** (%d.%d) CalculateLease: no new lease at present\n",cluster,proc);
		return false;
	}
}
