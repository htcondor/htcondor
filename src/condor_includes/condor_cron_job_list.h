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

#ifndef _CONDOR_CRON_JOB_LIST_H
#define _CONDOR_CRON_JOB_LIST_H 

#include "condor_common.h"
#include "condor_daemon_core.h"
#include <list>
#include "condor_cron_job.h"

// Define a simple class to run child tasks periodically.
class CronJobMgr;
class CondorCronJobList
{
  public:
	CondorCronJobList();
	~CondorCronJobList( );

	// Methods to manipulate the job list
	int InitializeAll( void );
	int HandleReconfig( void );
	int DeleteAll( void );
	int KillAll( bool force );
	int NumJobs( void ) const { return (int)m_job_list.size(); };
	int NumAliveJobs( void ) const;
	int NumActiveJobs( void ) const;
	bool GetStringList( StringList &sl ) const;
	double RunningJobLoad( void ) const;
	bool AddJob( 
		const char		*jobName,
		CronJob			*job
		);
	int DeleteJob( 
		const char		*jobName 
		);
	int ScheduleAll( void );
	int StartOnDemandJobs( void );
	CronJob *FindJob( const char *name );
	void ClearAllMarks( void );
	void DeleteUnmarked( void );

  private:
	std::list<CronJob *>		 m_job_list;
};

#endif /* _CONDOR_CRON_JOB_LIST_H */
