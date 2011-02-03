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

#ifndef _STARTD_BENCH_JOB_PARAMS_H
#define _STARTD_BENCH_JOB_PARAMS_H

#include "startd_cron_job_params.h"

// Override the Job params class so that we can stuff in default values
class StartdBenchJobMgr;
class StartdBenchJobParams : public StartdCronJobParams
{
public:
	StartdBenchJobParams( const char *job_name,
						  const CronJobMgr &mgr );
	~StartdBenchJobParams( void );
	bool Initialize( void );

private:
	const char	*m_libexec;
	const char *GetDefault( const char *item ) const;
	void GetDefault( const char *item, double &dv ) const;
};

#endif /* _STARTD_BENCH_JOB_PARAMS_H */
