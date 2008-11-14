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
#include "condor_cronjob_classad.h"
#include "scheduler.h"
#include "schedd_cronjob.h"

extern Scheduler scheduler;

ScheddCronJob::ScheddCronJob( const char *mgrName, const char *jobName ) :
		ClassAdCronJob( mgrName, jobName )
{
	// Register it with the Resource Manager
	scheduler.adlist_register( jobName );
}

// ScheddCronJob destructor
ScheddCronJob::~ScheddCronJob( )
{
	// Delete myself from the resource manager
	scheduler.adlist_delete( GetName() );
}

int
ScheddCronJob::Publish( const char *a_name, ClassAd *ad )
{
	return scheduler.adlist_replace( a_name, ad );
}
