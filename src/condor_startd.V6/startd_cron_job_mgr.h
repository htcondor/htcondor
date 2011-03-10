/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef _STARTD_CRON_JOB_MGR_H
#define _STARTD_CRON_JOB_MGR_H

#include "enum_utils.h"
#include "condor_cron_job_mgr.h"
#include "startd_cron_job.h"
#include "startd_cron_job_params.h"

// Define a simple class to run child tasks periodically.
class StartdCronJobMgr : public CronJobMgr
{
  public:
	StartdCronJobMgr( void );
	~StartdCronJobMgr( void );
	int Initialize( const char *name );
	int Reconfig( void );
	bool ShouldStartJob( const CronJob & ) const;
	bool ShouldStartBenchmarks( void ) const;
	bool JobStarted( const CronJob & );
	bool JobExited( const CronJob & );

	int Shutdown( bool force );
	bool ShutdownOk( void );
	CronAutoPublish_t getAutoPublishValue( void ) const {
		return m_auto_publish;
	};

  protected:
	StartdCronJobParams *CreateJobParams( const char *job_name );
	StartdCronJob *CreateJob( CronJobParams *job_params );

  private:
	bool m_shutting_down;
	void ParamAutoPublish( void );
	CronAutoPublish_t m_auto_publish;
};

#endif /* _STARTD_CRON_JOB_MGR_H */
