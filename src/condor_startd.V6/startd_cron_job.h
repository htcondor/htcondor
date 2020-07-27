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

#ifndef _STARTD_CRON_JOB_H
#define _STARTD_CRON_JOB_H

#include "classad_cron_job.h"
#include "startd_cron_job_params.h"
#include "startd_named_classad.h"

class CronJobMgr;
class StartdCronJob: public ClassAdCronJob
{
  public:
	StartdCronJob( ClassAdCronJobParams *job_params, CronJobMgr &mgr );
	virtual ~StartdCronJob( void );

	int Initialize( void );
	virtual StartdCronJobParams & Params( void ) const {
		return static_cast<StartdCronJobParams &>(*m_params);
	};

	bool isResourceMonitor() const { return Params().isResourceMonitor(); }

  private:
	virtual int Publish( const char *name, const char * sep_args, ClassAd *ad );
};

#endif /* _STARTD_CRON_JOB_H */
