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

#ifndef _SCHEDD_CRON_JOB_MGR_H
#define _SCHEDD_CRON_JOB_MGR_H

#include "condor_cron_job_mgr.h"

// Define a simple class to run child tasks periodically.
class ScheddCronJobMgr : public CronJobMgr
{
  public:
	ScheddCronJobMgr( void );
	~ScheddCronJobMgr( void );
	int Initialize( const char *name );
	int Shutdown( bool force );
	bool ShutdownOk( void );

  protected:
	CronJob *CreateJob( CronJobParams *params );

  private:
	bool m_shutting_down;
	
};

#endif /* _SCHEDD_CRON_JOB_MGR_H */
