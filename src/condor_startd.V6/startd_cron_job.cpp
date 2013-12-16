/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include "startd.h"
#include "classad_cron_job.h"
#include "startd_named_classad.h"
#include "startd_named_classad_list.h"
#include "startd_cron_job.h"

StartdCronJob::StartdCronJob( ClassAdCronJobParams *params,
							  CronJobMgr &mgr )
		: ClassAdCronJob( params, mgr ),
		  m_named_ad( NULL )
{
	// Register it with the Resource Manager
	m_named_ad = new StartdNamedClassAd( GetName(), *this );
	resmgr->adlist_register( m_named_ad );
}

// StartdCronJob destructor
StartdCronJob::~StartdCronJob( void )
{
	// Delete myself from the resource manager
	resmgr->adlist_delete( GetName() );
}

int
StartdCronJob::Initialize( void )
{
	ASSERT( dynamic_cast<StartdCronJobParams *>( m_params ) != NULL );
	return CronJob::Initialize( );
}

int
StartdCronJob::Publish( const char *ad_name, ClassAd *ad )
{
		// first, update the ad in the ResMgr, so we have the new
		// values.  we only want to do the (somewhat expensive)
		// operation of reportind a diff if we care about that.
	bool report_diff = false;
	CronAutoPublish_t auto_publish = cron_job_mgr->getAutoPublishValue();
	if( auto_publish == CAP_IF_CHANGED ) { 
		report_diff = true;
	}
	int rval = resmgr->adlist_replace( ad_name, GetPrefix(), ad, report_diff );

		// now, figure out if we need to update the collector based on
		// the startd_cron_autopublish stuff and if the ad changed...
	bool wants_update = false;
	switch( auto_publish ) {
	case CAP_NEVER:
		return rval;
		break;

	case CAP_ALWAYS:
		wants_update = true;
		dprintf( D_FULLDEBUG, "StartdCronJob::Publish() updating collector "
				 "[STARTD_CRON_AUTOPUBLISH=\"Always\"]\n" );
		break;

	case CAP_IF_CHANGED:
		if( rval == 1 ) {
			dprintf( D_FULLDEBUG, "StartdCronJob::Publish() has new data, "
					 "updating collector\n" );
			wants_update = true;
		} else {
			dprintf( D_FULLDEBUG, "StartdCronJob::Publish() has no new data, "
					 "skipping collector update\n" );
		}
		break;

	default:
		EXCEPT( "PROGRAMMER ERROR: unknown value of auto_publish (%d)", 
				(int)auto_publish );
		break;
	}
	if( wants_update ) {
		resmgr->update_all();
	}
	return rval;
}

