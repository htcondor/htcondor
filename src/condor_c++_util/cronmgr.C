/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include <limits.h>
#include <string.h>
#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"
#include "simplelist.h"
#include "condor_cron.h"
#include "condor_cronmgr.h"

// Basic constructor
CondorCronMgr::CondorCronMgr( const char *name )
{
	dprintf( D_FULLDEBUG, "CronMgr: Constructing '%s'\n", name );

	// Make sure that SetName doesn't try to free Name or ParamBase...
	Name = NULL;
	ParamBase = NULL;

	// Set 'em
	SetName( name, name, "_cron" );
}

// Basic destructor
CondorCronMgr::~CondorCronMgr( )
{
	// Reconfigure all running jobs
	Cron.KillAll( );

	// Log our death
	dprintf( D_FULLDEBUG, "CronMgr: bye\n" );
}

// Set new name..
int CondorCronMgr::SetName( const char *newName, 
							const char *newParamBase,
							const char *newParamExt )
{
	int		retval = 0;

	// Debug...
	dprintf( D_FULLDEBUG, "CronMgr: Setting name to '%s'\n", newName );
	if ( NULL != Name ) {
		free( (char *) Name );
	}

	// Copy it out..
	Name = strdup( newName );
	if ( NULL == Name ) {
		retval = -1;
	}

	// Set the parameter base name
	if ( NULL != newParamBase ) {
		retval = SetParamBase( newParamBase, newParamExt );
	}

	// Done
	return retval;
}

// Set new name..
int CondorCronMgr::SetParamBase( const char *newParamBase,
								 const char *newParamExt )
{
	dprintf( D_FULLDEBUG, "CronMgr: Setting parameter base to '%s'\n",
			 newParamBase );

	// Free the old one..
	if ( NULL != ParamBase ) {
		free( (void *) ParamBase );
	}

	// Default?
	if ( NULL == newParamBase ) {
		newParamBase = "CRON";
	}
	if ( NULL == newParamExt ) {
		newParamExt = "";
	}

	// Calc length & allocate
	int		len = strlen( newParamBase ) + strlen( newParamExt ) + 1;
	char *tmp = (char * ) malloc( len );
	if ( NULL == tmp ) {
		return -1;
	}

	// Copy it out..
	strcpy( tmp, newParamBase );
	strcat( tmp, newParamExt );
	ParamBase = tmp;

	return 0;
}

// Kill all running jobs
int
CondorCronMgr::KillAll( )
{
	// Log our death
	dprintf( D_FULLDEBUG, "CronMgr: Killing all jobs\n" );

	// Reconfigure all running jobs
	return Cron.KillAll( );
}

// Handle Reconfig
int
CondorCronMgr::Reconfig( void )
{
	char *paramBuf = GetParam( "JOBS" );

	// Find our environment variable, if it exits..
	if( paramBuf == NULL ) {
		dprintf( D_JOB, "CronMgr: No job list\n" );
		return 0;
	} else {
		ParseJobList( paramBuf );
		free( paramBuf );
	}

	// Reconfigure all running jobs
	Cron.Reconfig( );

	// Done
	return 0;
}

// Read a parameter
char *
CondorCronMgr::GetParam( const char *paramName, 
						 const char	*paramName2 )
{

	// Defaults...
	if ( NULL == paramName2 ) {
		paramName2 = "";
	}

	// Build the name of the parameter to read
	int len = ( strlen( ParamBase ) + 
				strlen( paramName ) +
				1 +
				strlen( paramName2 ) +
				1 );
	char *nameBuf = (char *) malloc( len );
	if ( NULL == nameBuf ) {
		return NULL;
	}
	strcpy( nameBuf, ParamBase );
	strcat( nameBuf, "_" );
	strcat( nameBuf, paramName );
	strcat( nameBuf, paramName2 );

	// Now, go read the actual parameter
	char *paramBuf = param( nameBuf );
	dprintf( D_ALWAYS, "Paraming for '%s'\n", nameBuf );
	free( nameBuf );

	// Done
	return paramBuf;
}

// Basic constructor
int
CondorCronMgr::ParseJobList( const char *jobString )
{
	StringList	*jobList = new StringList( jobString, " \t\n," );

	// Debug
	dprintf( D_JOB, "CronMgr: Job string is '%s'\n", jobString );

	// Clear all marks
	Cron.ClearAllMarks( );

	// Walk through the job list
	jobList->rewind();
	char	*jobDescr;
	while (  ( jobDescr = jobList->next() ) != NULL ) {
		char		*jobName, *jobPath, *jobPrefix, *tmp, modifier = '\0';
		unsigned	jobPeriod = 0;
		CronJobMode	jobMode = CRON_PERIODIC;

		// Debug
		dprintf( D_FULLDEBUG, "Parsing job description '%s'\n", jobDescr );

		// Make a temp copy of the job description that can get trashed
		// by strtok()
		char *tmpDescr = strdup( jobDescr );

		// Parse it out; format is name:path:period[hms]
		if (  ( NULL == ( jobName = strtok( tmpDescr, ":" ) )  ) ||
			  ( '\0' == *jobName )  )
		{
			dprintf( D_ALWAYS, 
					 "CronMgr: skipping invalid job description '%s'"
					 " (No name)\n",
					 jobDescr );
			free( tmpDescr );
			continue;
		}

		// Parse out the prefix
		if (  ( NULL == ( jobPrefix = strtok( NULL, ":" ) )  ) ||
			  ( '\0' == *jobPrefix )  ) 
		{
			dprintf( D_ALWAYS, 
					 "CronMgr: skipping invalid job description '%s'"
					 " (No prefix)\n",
					 jobDescr );
			free( tmpDescr );
			continue;
		}

		// Parse out the path
		if (  ( NULL == ( jobPath = strtok( NULL, ":" ) )  ) ||
			  ( '\0' == *jobPath )  )
		{
			dprintf( D_ALWAYS, 
					 "CronMgr: skipping invalid job description '%s'"
					 " (No path)\n",
					 jobDescr );
			free( tmpDescr );
			continue;
		}

		// Pull out the period
		if ( NULL != ( tmp = strtok( NULL, ":" ) )  ) {
			if ( sscanf( tmp, "%d%c", &jobPeriod, &modifier ) < 1 )
			{
				dprintf( D_ALWAYS,
						 "CronMgr: skipping invalid job description '%s'"
						 " (Bad Period)\n",
						 jobDescr );
				free( tmpDescr );
				continue;
			}

			// Check the modifier
			modifier = toupper( modifier );
			if ( ( 0 == modifier ) || ( 'S' == modifier ) ) {		// Seconds
				// Do nothing
			} else if ( 'M' == modifier ) {
				jobPeriod *= 60;
			} else if ( 'H' == modifier ) {
				jobPeriod *= ( 60 * 60 );
			} else {
				dprintf( D_ALWAYS,
						 "CronMgr: Invalid period modifier '%c' "
						 "(Job = '%s')\n", modifier, jobDescr );
			}
		}

		// Parse any remaining options
		bool	killMode = false;
		while (  ( tmp = strtok( NULL, ":" ) ) != NULL ) {
			if ( !strcasecmp( tmp, "kill" ) ) {
				dprintf( D_FULLDEBUG, "Cron: '%s': Kill option ok\n",
						 jobName );
				killMode = true;
			} else if ( !strcasecmp( tmp, "nokill" ) ) {
				dprintf( D_FULLDEBUG, "Cron: '%s': NoKill option ok\n",
						 jobName );
				killMode = false;
			} else if ( !strcasecmp( tmp, "continuous" ) ) {
				dprintf( D_FULLDEBUG, "Cron: '%s': Continuous option ok\n",
						 jobName );
				jobMode = CRON_CONTINUOUS;
			} else {
				dprintf( D_ALWAYS, "Job '%s': Ignoring unknown option '%s'\n",
						 jobName, tmp );
			}
		}

		// Verify that period==0 is only used in "continuous mode"
		if ( ( 0 == jobPeriod ) && ( CRON_CONTINUOUS != jobMode ) ) {
			dprintf( D_ALWAYS,
					 "CronMgr: Job '%s', Period = 0, not continuous\n",
					 jobName );
			free( tmpDescr );
			continue;
		}

		// Create the job & add it to the list (if it's new)
		CondorCronJob *job = Cron.FindJob( jobName );
		if ( NULL == job ) {
			job = NewJob( jobName );
			if ( NULL == job ) {
				dprintf( D_ALWAYS, "Cron: Failed to allocate job '%s'\n",
						 jobName );
				free( tmpDescr );
				return -1;
			}

			// Put the job in the list
			if ( Cron.AddJob( jobName, job ) < 0 ) {
				dprintf( D_ALWAYS, "CronMgr: Error creating job '%s'\n", 
						 jobName );
				free( tmpDescr );
				return -1;
			}
		}

		// Set the "Kill" mode
		job->SetKill( killMode );

		// Are there arguments for it?
		// Force the first arg to be the "Job Name"..
		char *argBufTmp = GetParam( jobName, "_ARGS" );
		char *argBuf;
		if ( NULL == argBufTmp ) {
			argBuf = strdup( jobName );
		} else {
			int		len = strlen( jobName ) + 1 + strlen( argBufTmp ) + 1;
			argBuf = new char[len];
			strcpy( argBuf, jobName );
			strcat( argBuf, " " );
			strcat( argBuf, argBufTmp );
			free( argBufTmp );
		}

		// Special environment vars?
		char *envBuf = GetParam( jobName, "_ENV" );

		// CWD?
		char *cwdBuf = GetParam( jobName, "_CWD" );


		// Mark the job so that it doesn't get deleted (below)
		job->Mark( );

		// And, set it's characteristics
		job->SetPath( jobPath );
		job->SetPrefix( jobPrefix );
		job->SetArgs( argBuf );
		job->SetEnv( envBuf );
		job->SetCwd( cwdBuf );
		job->SetPeriod( jobMode, jobPeriod );

		// Free up memory from param()
		if ( argBuf ) {
			free( argBuf );
		}
		if ( envBuf ) {
			free( envBuf );
		}
		if ( cwdBuf ) {
			free( cwdBuf );
		}

		// Debug info
		dprintf( D_FULLDEBUG, "CronMgr: Done processing job '%s'\n", jobName );

		// We're done with the temp copy of the job description; set it free
		free( tmpDescr );
	}

	// Delete all jobs that didn't get marked
	Cron.DeleteUnmarked( );

	// All ok
	return 0;
}

// Create a new job
CondorCronJob *
CondorCronMgr::NewJob( const char *name )
{
	dprintf( D_FULLDEBUG, "*** Creating a Condor job '%s' ***\n", name );
	CondorCronJob *job = new CondorCronJob( name );
	return job;
}
