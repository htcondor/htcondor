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
CondorCronMgr::
CondorCronMgr( const char *name )
{
	dprintf( D_FULLDEBUG, "CronMgr: Constructing '%s'\n", name );
	Name = strdup( name );
	NameLen = strlen( name );
}

// Basic destructor
CondorCronMgr::
~CondorCronMgr( )
{
	dprintf( D_FULLDEBUG, "CronMgr: bye\n" );
}

// Handle Reconfig
int
CondorCronMgr::
Reconfig( void )
{
	char *paramBuf = GetParam( "CRONJOBS" );

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
CondorCronMgr::
GetParam( const char *paramName, 
		  const char *paramNameSep,
		  const char *paramName2,
		  const char *paramName3 )
{
	// Build the name of the parameter to read
	int len = NameLen + strlen( paramName ) + 1;
	if ( paramNameSep && paramName2 ) {
		int		sepLen = strlen( paramNameSep );
		len += ( strlen( paramName2 ) + sepLen );
		if( paramName3 ) {
			len += strlen( paramName3 ) + sepLen;
		}
	}
	char *nameBuf = (char *) malloc( len );
	if ( NULL == nameBuf ) {
		return NULL;
	}
	strcpy( nameBuf, Name );
	strcat( nameBuf, paramName );
	if ( paramNameSep && paramName2 ) {
		strcat( nameBuf, paramNameSep );
		strcat( nameBuf, paramName2 );
		if( paramName3 ) {
			strcat( nameBuf, paramName3 );
		}
	}

	// Now, go read the actual parameter
	char *paramBuf = param( nameBuf );
	free( nameBuf );

	// Done
	return paramBuf;
}

// Basic constructor
int
CondorCronMgr::
ParseJobList( const char *jobString )
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

			// Force into "continuous mode" if time is zero
			if ( 0 == jobPeriod ) {
				jobMode = CRON_CONTINUOUS;
			}
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

		// Parse any remaining options
		while (  ( tmp = strtok( NULL, ":" ) ) != NULL ) {
			if ( !strcasecmp( tmp, "kill" ) ) {
				dprintf( D_FULLDEBUG, "Cron: '%s': Kill option ok\n",
						 jobName );
				job->SetKill( true );
			} else if ( !strcasecmp( tmp, "nokill" ) ) {
				dprintf( D_FULLDEBUG, "Cron: '%s': NoKill option ok\n",
						 jobName );
				job->SetKill( false );
			} else if ( !strcasecmp( tmp, "continuous" ) ) {
				dprintf( D_FULLDEBUG, "Cron: '%s': Continuous option ok\n",
						 jobName );
				jobMode = CRON_CONTINUOUS;
			} else {
				dprintf( D_ALWAYS, "Job '%s': Unknown option '%s'\n",
						 jobName, tmp );
			}
		}

		// Are there arguments for it?
		char *argBuf = GetParam( "CRON", "_", jobName, "ARGS" );

		// Special environment vars?
		char *envBuf = GetParam( "CRON", "_", jobName, "ENV" );

		// CWD?
		char *cwdBuf = GetParam( "CRON", "_", jobName, "CWD" );


		// Mark the job so that it doesn't get deleted (below)
		job->Mark( );

		// And, set it's characteristics
		job->SetPath( jobPath );
		job->SetPrefix( jobPrefix );
		job->SetArgs( argBuf );
		job->SetEnv( envBuf );
		job->SetCwd( cwdBuf );
		job->SetPeriod( jobMode, jobPeriod );

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
CondorCronMgr::
NewJob( const char *name )
{
	dprintf( D_FULLDEBUG, "*** Creating a Condor job '%s' ***\n", name );
	CondorCronJob *job = new CondorCronJob( name );
	return job;
}
