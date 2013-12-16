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
#include "condor_cron_job_mgr.h"
#include "classad_cron_job.h"
#include "startd_bench_job.h"

StartdBenchJob::StartdBenchJob( ClassAdCronJobParams *job_params,
								CronJobMgr &mgr ) :
		StartdCronJob( job_params, mgr )
{
	if (  Params().OptReconfigRerun()  ) {
	}
}

// StartdBenchJob destructor
StartdBenchJob::~StartdBenchJob( void )
{
}

int
StartdBenchJob::Initialize( void )
{
	ASSERT( dynamic_cast<StartdBenchJobParams *>( m_params ) != NULL );
	return StartdCronJob::Initialize( );
}

int
StartdBenchJob::Publish( const char *ad_name, ClassAd *ad )
{
	resmgr->adlist_replace( ad_name, GetPrefix(), ad, false );
	return 0;
}
