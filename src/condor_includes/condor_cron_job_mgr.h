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

#ifndef _CONDOR_CRON_JOB_MGR_H
#define _CONDOR_CRON_JOB_MGR_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_cron_job.h"
#include "condor_cron_job_list.h"
#include "condor_cron_param.h"
#include "condor_cron_job_params.h"

// Job Managers parameters -- base version
class CronJobMgrParams : public CronParamBase
{
  public:
	CronJobMgrParams( const char &base ) 
		: CronParamBase( base ) { };
	virtual ~CronJobMgrParams( void ) { };
};

// Define a simple class to run child tasks periodically.
class CronJobMgr : public Service
{
  public:
	CronJobMgr( void );
	virtual ~CronJobMgr( );
	virtual int Initialize( const char *name );
	virtual int HandleReconfig( void );

	const char *GetName( void ) const { return m_name; };
	const char *GetParamBase( void ) const { return m_param_base; };

	int KillAll( bool force );
	bool IsAllIdle(std::string * names = nullptr);	// string returns names of not-idle jobs
	virtual bool ShouldStartJob( const CronJob & ) const;
	virtual bool JobStarted( const CronJob & );
	virtual bool JobExited( const CronJob & );
	virtual CronJobMode DefaultJobMode( void ) const {
		return CRON_PERIODIC;
	};

	bool StartOnDemandJobs( void );
	bool ScheduleAllJobs( void );

	double GetCurJobLoad( void ) const { return m_cur_job_load; };
	// Return max job load allowed with a very small fuzz factor
	double GetMaxJobLoad( void ) const { return (m_max_job_load+0.000001); };
	void SetMaxJobLoad( double v ) { m_max_job_load = v; };
	int GetNumActiveJobs( void ) const;
	int GetNumJobs( void ) const { return m_job_list.NumJobs(); };

  protected:
	virtual CronJobMgrParams *CreateMgrParams( const char &base );
	virtual CronJobParams *CreateJobParams( const char *job_name );
	virtual CronJob *CreateJob( CronJobParams *job_params );

	int SetName( const char *name, 
				 const char *setParamBase = NULL,
				 const char *setParamExt = NULL );
	int SetParamBase( const char *base, const char *ext );

  protected:
	CondorCronJobList	 m_job_list;

  private:
	const char			*m_name;			// Logical name
	const char			*m_param_base;		// Base of calls to param()
	CronJobMgrParams	*m_params;			// Lookup parameters
	const char			*m_config_val_prog;	// Config val program to run
	double				 m_max_job_load;	// Max job load
	double				 m_cur_job_load;	// Current job load
	int					 m_schedule_timer;	// DaemonCore timerID

	// Private member functions
	void ScheduleJobsFromTimer( int timerID = -1 );
	int DoConfig( bool initial = false );
	int ParseJobList( const char *JobListString );

};

#endif /* _CONDOR_CRON_JOB_MGR_H */
