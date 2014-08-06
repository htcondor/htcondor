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
#include "schedd_cron_job.h"
#include "classad_cron_job.h"
#include "scheduler.h"

extern Scheduler scheduler;

class CronJobMgr;
ScheddCronJob::ScheddCronJob( ClassAdCronJobParams *job_params,
							  CronJobMgr &mgr )
		: ClassAdCronJob( job_params, mgr )
{
	// Register it with the Resource Manager
	scheduler.adlist_register( GetName() );
}

// ScheddCronJob destructor
ScheddCronJob::~ScheddCronJob( )
{
	// Delete myself from the resource manager
	scheduler.adlist_delete( GetName() );
}

int
ScheddCronJob::Publish( const char *a_name, const char * /*args*/, ClassAd *ad )
{
	return scheduler.adlist_replace( a_name, ad );
}
