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

#ifndef _STARTD_BENCH_JOB_H
#define _STARTD_BENCH_JOB_H

#include "startd_cron_job.h"
#include "startd_bench_job_params.h"

class CronJobMgr;
class StartdBenchJob: public StartdCronJob
{
  public:
	StartdBenchJob( ClassAdCronJobParams *job_params, CronJobMgr &mgr );
	virtual ~StartdBenchJob( void );
	int Initialize( void );

  private:
	int Publish( const char *name, ClassAd *ad );
};

#endif /* _STARTD_BENCH_JOB_H */
