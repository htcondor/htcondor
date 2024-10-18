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
#ifndef _CONDOR_CRON_JOB_PARAMS_H
#define _CONDOR_CRON_JOB_PARAMS_H

#include "condor_common.h"
#include "env.h"
#include "condor_cron_job_mode.h"
#include "condor_cron_param.h"

// Default job load
const double	CronJobDefaultLoad = 0.01;

class CronJob;
class CronJobMgr;
class CronJobParams : public CronParamBase
{
  public:
	CronJobParams( const char *job_name, const CronJobMgr &mgr );
	virtual ~CronJobParams( void );

	// Finish initialization
	virtual bool Initialize( void );
	virtual const CronJobMgr &GetMgr( void ) { return m_mgr; };

	// Force job recreation?
	bool Compatible( const CronJobParams &other ) const {
		return other.GetJobMode() == GetJobMode();
	};

	bool AddEnv( Env const &env );
	bool AddArgs( const ArgList &args );

	// Manipulate the job
	const char *GetName( void ) const { return m_name.c_str(); };
	const char *GetPrefix( void ) const { return m_prefix.c_str(); };
	const char *GetExecutable( void ) const { return m_executable.c_str(); };
	const ArgList &GetArgs( void ) const { return m_args; };
	const Env &GetEnv( void ) const { return m_env; };
	const char *GetCwd( void ) const { return m_cwd.c_str(); };
	time_t GetPeriod( void ) const { return m_period; };
	double GetJobLoad( void ) const { return m_jobLoad; };
	const ConstraintHolder & GetCondition( void ) const { return m_condition; };

	// Options
	bool OptKill( void ) const { return m_optKill; };
	bool OptReconfig( void ) const { return m_optReconfig; };
	bool OptReconfigRerun( void ) const { return m_optReconfigRerun; };
	bool OptIdle( void ) const { return m_optIdle; };

	// Default job mode
	virtual CronJobMode DefaultJobMode( void ) const {
		return CRON_PERIODIC;
	};

	// Mode information
	bool IsPeriodic( void ) const {
		return CRON_PERIODIC == m_mode;
	};
	bool IsWaitForExit( void ) const {
		return CRON_WAIT_FOR_EXIT == m_mode;
	};
	bool IsOneShot( void ) const {
		return CRON_ONE_SHOT == m_mode;
	};
	bool IsOnDemand( void ) const {
		return CRON_ON_DEMAND == m_mode;
	};
	CronJobMode GetJobMode( void ) const {
		return m_mode;
	};
	const char *GetModeString( void ) const;
	static bool IsValidMode( CronJobMode mode ) {
		return ( (mode >= CRON_PERIODIC) && (mode <= CRON_ON_DEMAND) );
	};

  private:
	virtual const char *GetParamName( const char *item ) const;
	bool InitArgs( const std::string &param );
	bool InitEnv( const std::string &param );
	bool InitPeriod( const std::string &period );

  protected:
	const CronJobMgr&m_mgr;				// My manager
	CronJobMode		 m_mode;			// Job's scheduling mode
	const char		*m_modestr;			// Mode's string
	CronJob			*m_job;				// The associated job
	std::string		 m_name;			// Logical name of the job
	std::string		 m_prefix;			// Publishing prefix
	std::string		 m_executable;		// Path to the executable
	ArgList          m_args;			// Arguments to pass it
	Env              m_env;				// Environment variables
	std::string		 m_cwd;				// Process's initial CWD
	time_t			 m_period;			// The configured period
	double			 m_jobLoad;			// Job's assigned load
	ConstraintHolder	 m_condition;			// should the job run?

	// Options
	bool			 m_optKill;			// Kill the job if before next run?
	bool			 m_optReconfig;		// Send the job a HUP for reconfig
	bool			 m_optReconfigRerun;// Rerun the job on reconfig?
	bool			 m_optIdle;			// Only run when idle
};

#endif /* _CONDOR_CRON_JOB_PARAMS_H */
