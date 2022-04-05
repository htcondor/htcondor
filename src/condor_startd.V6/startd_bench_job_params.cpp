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
#include "condor_config.h"
#include "startd_bench_job.h"
#include "startd_bench_job_mgr.h"
#include "startd_bench_job_params.h"

// Override the Job params class so that we can stuff in default values
StartdBenchJobParams::StartdBenchJobParams( const char *job_name,
											const CronJobMgr &mgr )
		: StartdCronJobParams( job_name, mgr ),
		  m_libexec( NULL )
{
}

bool
StartdBenchJobParams::Initialize( void )
{
	// Initialize libexec
	m_libexec = param( "LIBEXEC" );
	ASSERT( m_libexec != NULL );

	// Base class initializer
	if ( ! ClassAdCronJobParams::Initialize() ) {
		return false;
	}

	// Check the mode
	if ( m_mode != CRON_ON_DEMAND ) {
		dprintf( D_ALWAYS,
				 "Error: Benchmark job '%s' is not 'OnDemand': skipping!\n",
				 GetName() );
		return false;
	}

	// Dis-allow rerun-on-reconfig
	if ( m_optReconfigRerun ) {
		dprintf( D_ALWAYS,
				 "Warning: Benchmark job '%s' has option '%s' enabled -- disabling!\n",
				 GetName(), "RECONFIG_RERUN" );
		m_optReconfigRerun = false;
	}

	// Done
	return true;
};

StartdBenchJobParams::~StartdBenchJobParams( void )
{
	if ( m_libexec ) {
		free( const_cast<char *>(m_libexec) );
		m_libexec = NULL;
	}
}

char *
StartdBenchJobParams::GetDefault( const char *item ) const
{
	if ( !strcasecmp( item, "EXECUTABLE" ) ) {
		const char	*exe = NULL;
		if ( !strcasecmp( m_name.c_str(), "MIPS" ) ) {
			exe = "condor_mips";
		}
		else if ( !strcasecmp( m_name.c_str(), "KFLOPS" ) ) {
			exe = "condor_kflops";
		}
		else {
			return NULL;
		}

		unsigned  len = strlen(m_libexec) + strlen(exe) + 2;
		char	 *buf = (char *)malloc( len );
		if ( NULL == buf ) {
			return NULL;
		}
		strcpy( buf, m_libexec );
		strcat( buf, "/" );
		strcat( buf, exe );
		return buf;
	}
	else if ( !strcasecmp( item, "MODE" ) ) {
		return strdup( "OnDemand" );
	}
	else {
		return CronJobParams::GetDefault( item );
	}
}

void
StartdBenchJobParams::GetDefault( const char *item,
								  double &dv ) const
{
	if ( strcasecmp( item, "JOB_LOAD" ) ) {
		dv = 1.0;
	}
}
