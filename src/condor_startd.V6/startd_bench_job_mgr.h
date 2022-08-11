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

#ifndef _STARTD_BENCH_JOB_MGR_H
#define _STARTD_BENCH_JOB_MGR_H

#include "condor_cron_job_mgr.h"
#include "startd_bench_job.h"
#include "startd_bench_job_params.h"

class StartdBenchJobMgr;

// Override the JobMgr params class so that we can stuff in default values
class StartdBenchJobMgrParams : public CronJobMgrParams
{
public:
	StartdBenchJobMgrParams( const char &base, const CronJobMgr &mgr );

private:
	char * GetDefault( const char *item ) const;
	void GetDefault( const char *item, double &dv ) const;
};


// Define a simple class to run child tasks periodically.
class Resource;
class StartdBenchJobMgr : public CronJobMgr
{
  public:
	StartdBenchJobMgr( void );
	~StartdBenchJobMgr( void );
	int Initialize( const char *name );
	bool StartBenchmarks( Resource *res, int &count );
	int Reconfig( void );
	int Shutdown( bool force );
	bool ShutdownOk( void );
	bool ShouldStartJob( const CronJob & ) const;
	bool JobStarted( const CronJob & );
	bool JobExited( const CronJob & );
	bool BenchmarksFinished( void );
	CronJobMode DefaultJobMode( void ) const {
		return CRON_ON_DEMAND;
	};
	bool NumActiveBenchmarks( void ) const {
		return GetNumActiveJobs( );
	};

  protected:
	StartdBenchJobMgrParams *CreateMgrParams( const char &base );
	StartdBenchJobParams *CreateJobParams( const char *job_name );
	StartdBenchJob *CreateJob( CronJobParams *job_params );

  private:
	bool			 m_shutting_down;
	bool			 m_is_running;
	Resource		*m_rip;

	bool ParamRunBenchmarks( void );
};

#endif /* _STARTD_BENCH_JOB_MGR_H */
