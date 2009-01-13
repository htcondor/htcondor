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
#include "condor_cronjob.h"
#include "condor_cronjob_classad.h"
#include "condor_string.h"
#include "subsystem_info.h"

// Interface version number
#define	INTERFACE_VERSION	"1"

// CronJob constructor
ClassAdCronJob::ClassAdCronJob( const char *mgrName,
								const char *jobName ) :
		CronJobBase( mgrName, jobName )
{
	OutputAd = NULL;
	OutputAdCount = 0;

	// Build my interface version environment (but, I need a 'name' to do it)
	if ( mgrName && (*mgrName) ) {
		char	*nameUc = strdup( mgrName );
		char	*namePtr;
		for( namePtr = nameUc; *namePtr; namePtr++ ) {
			if ( islower ( *namePtr ) ) {
				*namePtr = toupper( *namePtr );
			}
		}
		mgrNameUc = nameUc;
		free( nameUc );

		MyString EnvName;

		EnvName = mgrNameUc;
		EnvName += "_INTERFACE_VERSION";
		classad_env.SetEnv( EnvName, INTERFACE_VERSION );

		EnvName = get_mySubSystem()->getName( );
		EnvName += "_CRON_NAME";
		classad_env.SetEnv( EnvName, mgrName );

	}
}

// StartdCronJob destructor
ClassAdCronJob::~ClassAdCronJob( )
{
	// Delete myself from the resource manager
	if ( NULL != OutputAd ) {
		delete( OutputAd );
	}
}

// StartdCronJob initializer
int
ClassAdCronJob::Initialize( )
{
	if ( configValProg.Length() && mgrNameUc.Length() ) {
		MyString	env_name;
		env_name = mgrNameUc;
		env_name += "_CONFIG_VAL";
		classad_env.SetEnv( env_name.Value(), configValProg );
	}

	char **env_array = classad_env.getStringArray();
	AddEnv( env_array );
	deleteStringArray(env_array);

	// And, run the "main" Initialize function
	return CronJobBase::Initialize( );
}

// Process a line of input
int
ClassAdCronJob::ProcessOutput( const char *line )
{
	if ( NULL == OutputAd ) {
		OutputAd = new ClassAd( );
	}

	// NULL line means end of list
	if ( NULL == line ) {
		// Publish it
		if ( OutputAdCount != 0 ) {

			// Insert the 'LastUpdate' field
			const char      *lu_prefix = GetPrefix( );
			if ( lu_prefix ) {
				MyString    Update;
				Update.sprintf( "%sLastUpdate = %ld", lu_prefix,
								(long) time( NULL ) );
				const char  *UpdateStr = Update.Value( );

				// Add it in
				if ( ! OutputAd->Insert( UpdateStr ) ) {
					dprintf( D_ALWAYS, "Can't insert '%s' into '%s' ClassAd\n",
							 UpdateStr, GetName() );
					// TodoWrite( );
				}
			}

			// Replace the old ClassAd now
			Publish( GetName( ), OutputAd );

			// I've handed it off; forget about it!
			OutputAd = NULL;
			OutputAdCount = 0;
		}
	} else {
		// Process this line!
		if ( ! OutputAd->Insert( line ) ) {
			dprintf( D_ALWAYS, "Can't insert '%s' into '%s' ClassAd\n",
					 line, GetName() );
			// TodoWrite( );
		} else {
			OutputAdCount++;
		}
	}
	return OutputAdCount;
}
