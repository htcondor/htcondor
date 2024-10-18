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
#include "condor_debug.h"
#include "condor_config.h"
#include "env.h"
#include "condor_cron_param.h"
#include "condor_cron_job_mgr.h"
#include "condor_cron_job_mode.h"
#include "condor_cron_job_params.h"

#include "classad/classad.h"

CronJobParams::CronJobParams( const char		*job_name,
							  const CronJobMgr	&mgr )
		: CronParamBase( *(mgr.GetParamBase()) ),
		  m_mgr( mgr ),
		  m_mode( CRON_ILLEGAL ),
		  m_modestr( nullptr ),
		  m_job( nullptr ),
		  m_name( job_name ),
		  m_period(std::numeric_limits<time_t>::max()),
		  m_jobLoad( CronJobDefaultLoad ),	// Default near zero
		  m_optKill( false ),
		  m_optReconfig( false ),
		  m_optReconfigRerun( false ),
		  m_optIdle( false )
{
}

CronJobParams::~CronJobParams( void )
{
}

const char *
CronJobParams::GetParamName( const char *item ) const
{
	// Build the name of the parameter to read
	size_t len = ( strlen( &m_base ) +
					 1 +		// '_'
					 m_name.length( ) +
					 1 +		// '_'
					 strlen( item ) +
					 1 );	// '\0'
	if ( len > sizeof(m_name_buf) ) {
		return NULL;
	}
	strcpy( m_name_buf, &m_base );
	strcat( m_name_buf, "_" );
	strcat( m_name_buf, m_name.c_str() );
	strcat( m_name_buf, "_" );
	strcat( m_name_buf, item );

	return m_name_buf;
}

bool
CronJobParams::Initialize( void )
{
	std::string param_prefix;
	std::string param_executable;
	std::string param_period;
	std::string param_mode;
	bool	 param_reconfig = false;
	bool	 param_reconfig_rerun = false;
	bool	 param_kill_mode = false;
	std::string param_args;
	std::string param_env;
	std::string param_cwd;
	double   param_job_load;
	std::string param_condition;

	Lookup( "PREFIX", param_prefix );
	Lookup( "EXECUTABLE", param_executable );
	Lookup( "PERIOD", param_period );
	Lookup( "MODE", param_mode );
	Lookup( "RECONFIG", param_reconfig );
	Lookup( "RECONFIG_RERUN", param_reconfig_rerun );
	Lookup( "KILL", param_kill_mode );
	Lookup( "ARGS", param_args );
	Lookup( "ENV", param_env );
	Lookup( "CWD", param_cwd );
	Lookup( "JOB_LOAD", param_job_load, 0.01, 0, 100.0 );
	Lookup( "CONDITION", param_condition );

	// Some quick sanity checks
	if ( param_executable.empty() ) {
		dprintf( D_ALWAYS, 
				 "CronJobParams: No path found for job '%s'; skipping\n",
				 GetName() );
		return false;
	}

	// Parse the job mode
	m_mode = DefaultJobMode( );
	if ( ! param_mode.empty() ) {
		const CronJobModeTable		&mt  = GetCronJobModeTable( );
		const CronJobModeTableEntry	*mte = mt.Find( param_mode.c_str() );
		if ( NULL == mte ) {
			dprintf( D_ALWAYS,
					 "CronJobParams: Unknown job mode for '%s'\n",
					 GetName() );
			return false;
		} else {
			m_mode = mte->Mode();
			m_modestr = mte->Name();
		}
	}

	// Initialize the period
	if ( !InitPeriod( param_period ) ) {
		dprintf( D_ALWAYS,
				 "CronJobParams: Failed to initialize period for job %s\n",
				 GetName() );
		return false;
	}

	// Are there arguments for it?
	if ( !InitArgs( param_args ) ) {
		dprintf( D_ALWAYS,
				 "CronJobParams: Failed to initialize arguments for job %s\n",
				 GetName() );
		return false;
	}


	// Parse the environment.
	if ( !InitEnv( param_env ) ) {
		dprintf( D_ALWAYS,
				 "CronJobParams: Failed to initialize environment for job %s\n",
				 GetName() );
		return false;
	}

	m_prefix = param_prefix;
	m_executable = param_executable;
	m_cwd = param_cwd;
	m_jobLoad = param_job_load;
	m_optKill = param_kill_mode;
	m_optReconfig = param_reconfig;
	m_optReconfigRerun = param_reconfig_rerun;

	// Parse the condition.
	if(! param_condition.empty()) {
		m_condition.set(strdup(param_condition.c_str()));
		if(! m_condition.Expr()) {
			dprintf( D_ALWAYS,
				 "CronJobParams: Failed to initialize condition '%s' for job %s\n",
				 param_condition.c_str(), GetName() );
			return false;
		}
		dprintf( D_FULLDEBUG, "CronJobParams(%s): CONDITION is (%s)\n", GetName(), param_condition.c_str() );
	}

	return true;
}

bool
CronJobParams::InitArgs( const std::string &param_args )
{
	ArgList args;
	std::string args_errors;

	// Force the first arg to be the "Job Name"..
	m_args.Clear();
	if( !args.AppendArgsV1RawOrV2Quoted( param_args.c_str(),
										 args_errors ) ) {
		dprintf( D_ALWAYS,
				 "CronJobParams: Job '%s': "
				 "Failed to parse arguments: '%s'\n",
				 GetName(), args_errors.c_str());
		return false;
	}
	return AddArgs( args );
}

bool
CronJobParams::InitEnv( const std::string &param_env )
{
	Env			env_object;
	std::string env_error_msg;
	m_env.Clear();
	if( !env_object.MergeFromV1RawOrV2Quoted( param_env.c_str(),
										  env_error_msg ) ) {
		dprintf( D_ALWAYS,
				 "CronJobParams: Job '%s': "
				 "Failed to parse environment: '%s'\n",
				 GetName(), env_error_msg.c_str());
		dprintf(D_ERROR, "CronJobParams: Invalid %s_ENV: %s\n", GetName(), param_env.c_str());
		return false;
	}
	return AddEnv( env_object );
}

// Set job's period
bool
CronJobParams::InitPeriod( const std::string &param_period )
{
	// Find the job period
	m_period = 0;
	if ( ( m_mode == CRON_ONE_SHOT) || ( m_mode == CRON_ON_DEMAND) ) {
		if ( ! param_period.empty() ) {
			dprintf( D_ALWAYS,
					 "CronJobParams: Warning:"
					 "Ignoring job period specified for '%s'\n",
					 GetName() );
			return true;
		}
	}
	else if ( param_period.empty() ) {
		dprintf( D_ALWAYS,
				 "CronJobParams: No job period found for job '%s': skipping\n",
				 GetName() );
		return false;
	} else {
		char	modifier = 'S';
		int		num = sscanf( param_period.c_str(), "%ld%c",
							  &m_period, &modifier );
		if ( num < 1 ) {
			dprintf( D_ALWAYS,
					 "CronJobParams: Invalid job period found "
					 "for job '%s' (%s): skipping\n",
					 GetName(), param_period.c_str() );
			return false;
		} else {
			// Check the modifier
			modifier = toupper( modifier );
			if ( ( 'S' == modifier ) ) {	// Seconds
				// Do nothing
			} else if ( 'M' == modifier ) {
				m_period *= 60;
			} else if ( 'H' == modifier ) {
				m_period *= ( 60 * 60 );
			} else {
				dprintf( D_ALWAYS,
						 "CronJobParams: Invalid period modifier "
						 "'%c' for job %s (%s)\n",
						 modifier, GetName(), param_period.c_str() );
				return false;
			}
		}
	}

	// Verify that the mode seleted is valid
	if (  IsPeriodic() && (0 == m_period)  ) {
		dprintf( D_ALWAYS, 
				 "Cron: Job '%s'; Periodic requires non-zero period\n",
				 GetName() );
		return false;
	}

	// Verify that the mode seleted is valid
	if (  IsPeriodic() && (0 == m_period)  ) {
		dprintf( D_ALWAYS, 
				 "Cron: Job '%s'; Periodic requires non-zero period\n",
				 GetName() );
		return false;
	}
	return true;
}

bool
CronJobParams::AddEnv( Env const &env )
{
	m_env.MergeFrom( env );
	return true;
}

// Add to the job's arg list
bool
CronJobParams::AddArgs( const ArgList &new_args )
{
	m_args.AppendArgsFromArgList(new_args);
	return true;
}

const char *
CronJobParams::GetModeString( void ) const
{
	return m_modestr;
}
