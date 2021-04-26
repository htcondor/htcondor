/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "subsystem_info.h"
#include "env.h"
#include "condor_cron_job.h"
#include "condor_cron_job_mgr.h"
#include "classad_cron_job.h"

// Interface version number
#define	INTERFACE_VERSION	"1"

// ClassAd Cron Job parameters methods
ClassAdCronJobParams::ClassAdCronJobParams( const char *job_name,
											const CronJobMgr &mgr )
		: CronJobParams( job_name, mgr )
{
}

bool
ClassAdCronJobParams::Initialize( void )
{
	if ( !CronJobParams::Initialize() ) {
		return false;
	}

	const char *mgr_name = GetMgr().GetName( );
	if ( mgr_name && (*mgr_name) ) {
		char	*name_uc = strdup( mgr_name );
		char	*name_ptr;
		for( name_ptr = name_uc; *name_ptr; name_ptr++ ) {
			if ( islower ( (unsigned char) *name_ptr ) ) {
				*name_ptr = toupper( *name_ptr );
			}
		}
		m_mgr_name_uc = name_uc;
		free( name_uc );
	}
	Lookup( "CONFIG_VAL_PROG", m_config_val_prog );
	return true;
} 

// ClassadCronJob constructor
ClassAdCronJob::ClassAdCronJob( ClassAdCronJobParams *params,
								CronJobMgr &mgr )
		: CronJob( params, mgr ),
		  m_output_ad( NULL ),
		  m_output_ad_count( 0 )
{
}

// StartdCronJob destructor
ClassAdCronJob::~ClassAdCronJob( void )
{
	// Delete myself from the resource manager
	if ( NULL != m_output_ad ) {
		delete( m_output_ad );
	}
}

// StartdCronJob initializer
int
ClassAdCronJob::Initialize( void )
{
	// Build my interface version environment (but, I need a 'name' to do it)
	const MyString	&mgr_name_uc = Params().GetMgrNameUc( );
	if ( mgr_name_uc.length() ) {
		MyString env_name;

		env_name = Params().GetMgrNameUc();
		env_name += "_INTERFACE_VERSION";
		m_classad_env.SetEnv( env_name, INTERFACE_VERSION );

		env_name = get_mySubSystem()->getName( );
		env_name += "_CRON_NAME";
		m_classad_env.SetEnv( env_name, Mgr().GetName() );
	}

	if (  Params().GetConfigValProg().length() && mgr_name_uc.length()  ) {
		MyString	env_name;
		env_name = mgr_name_uc;
		env_name += "_CONFIG_VAL";
		m_classad_env.SetEnv( env_name, Params().GetConfigValProg() );
	}

	RwParams().AddEnv( m_classad_env );

	// And, run the "main" Initialize function
	return CronJob::Initialize( );
}


// Process (i.e. store) the separator args between output records. (i.e. everything after the -)
// so that the Publish method (including a derived one) can fetch and parse it.
int
ClassAdCronJob::ProcessOutputSep( const char *args )
{
	int status = 0;
	if ( NULL == args ) {
		m_output_ad_args.clear();
	} else {
		m_output_ad_args = args;
	}
	return status;
}


// Process a line of input
int
ClassAdCronJob::ProcessOutput( const char *line )
{
	if ( NULL == m_output_ad ) {
		m_output_ad = new ClassAd( );
	}

	// NULL line means end of list
	if ( NULL == line ) {
		// Publish it
		if ( m_output_ad_count != 0 ) {

			// Insert the 'LastUpdate' field
			const char      *lu_prefix = GetPrefix( );
			if ( lu_prefix ) {
				std::string attrn;
				formatstr(attrn, "%sLastUpdate", lu_prefix);

				// Add it in
				m_output_ad->Assign(attrn, time(NULL));
			}

			const char * ad_args = NULL;
			if ( ! m_output_ad_args.empty())
				ad_args = m_output_ad_args.c_str();

			// Replace the old ClassAd now
			Publish( GetName( ), ad_args, m_output_ad );

			// I've handed it off; forget about it!
			m_output_ad = NULL;
			m_output_ad_count = 0;
			m_output_ad_args.clear();
		}
	} else {
		// Process this line!
		if ( ! m_output_ad->Insert( line ) ) {
			dprintf( D_ALWAYS,
					 "Can't insert '%s' into '%s' ClassAd\n",
					 line, GetName() );
			// TodoWrite( );
		} else {
			m_output_ad_count++;
		}
	}
	return m_output_ad_count;
}
