/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include <limits.h>
#include <string.h>
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"
#include "simplelist.h"
#include "condor_cron.h"
#include "condor_cronmgr.h"


// Class to parse the job lists
class JobListParser
{
public:
	JobListParser( const char *s );
	~JobListParser( );

	bool nextJob( void );
	const char *getName( void ) { return name; };
	const char *getPrefix( void ) { return prefix; };
	const char *getPath( void ) { return path; };
	const char *getPeriod( void ) { return period; };
	const char *getOption( void );
	const char *getJobString( void ) { return curJobStart; };

private:
	char	*jobListString;
	char	*curJobPointer;
	char	*curJobStart;
	char	*nextJobStart;

	const char	*name;
	const char	*prefix;
	const char	*path;
	const char	*period;

	const char *nextField( void );
	void skipJob( void );
};

// Job list parser constructor
JobListParser::JobListParser( const char *s )
{
	jobListString = strdup( s );
	nextJobStart = jobListString;
	curJobPointer = NULL;

	name = NULL;
	path = NULL;
	period = NULL;
}

// Job list parser destructor
JobListParser::~JobListParser( void )
{
	if ( jobListString ) {
		free( jobListString );
	}
}

// Job list parser: Get the next job
bool JobListParser::nextJob( void )
{
	// Keep looking til we find one or give up...
	while( ( NULL != curJobPointer ) || ( NULL != nextJobStart ) ) {

		// Skip to start of next job?
		if ( NULL == curJobPointer ) {
			curJobPointer = nextJobStart;
			nextJobStart = NULL;			// None found yet!
		}

		// Skip whitespace
		while( isspace( *curJobPointer ) ) {
			curJobPointer++;
		}

		// End of string?  Done
		if ( '\0' == *curJobPointer ) {
			curJobPointer = NULL;
			return false;
		}

		// Debug info...
		curJobStart = curJobPointer;
		dprintf( D_FULLDEBUG, "CronMgr: Trying to find a job in '%s'\n", curJobPointer );

		// Now, try to parse it...

		// Name: must exist, non-zero length
		name = nextField( );
		if (  ( NULL == name ) || ( '\0' == *name )  ){
			dprintf( D_ALWAYS, "CronMgr: Job parse error: Can't find a name\n" );
			skipJob( );
			continue;
		}

		// Prefix: must exist, no whitespace
		prefix = nextField( );
		const char *tmp = prefix;
		while( tmp && *tmp ) {
			if ( isspace( *tmp ) ) {
				dprintf( D_ALWAYS,
						 "CronMgr: '%s': Invalid prefix '%s': contains space\n",
						 name, prefix );
				prefix = NULL;
				break;
			}
			tmp++;
		}
		if ( NULL == prefix ) {
			dprintf( D_ALWAYS, "CronMgr: '%s': parse error: Can't find a prefix\n", name );
			skipJob( );
			continue;
		}

		// Path: must exist, non-zero length
		path = nextField( );
		if (  ( NULL == path ) || ( '\0' == *path )  ){
			dprintf( D_ALWAYS, "Cron: '%s': parse error: Can't find a path\n", name );
			skipJob( );
			continue;
		}

		// Period (not required)
		period = nextField( );
		return true;
	}

	// No valid jobs found
	return false;
}

// Extract the next 'option'
const char *JobListParser::getOption( void )
{

	// Parse out the next field
	const char *option = nextField( );

	// Handle special cases
	if ( NULL == option ) {
		skipJob( );
	} else if ( '\0' == *option ) {
		option = NULL;
	}

	// Done
	return option;
}

// Skip to the end of this job
void JobListParser::skipJob( void )
{
	const char	*label = curJobStart;

	while(  ( NULL != curJobPointer ) &&
			( '\0' != *curJobPointer ) &&
			( ! isspace( *curJobPointer ) ) ) {
		if ( NULL != label ) {
			dprintf( D_FULLDEBUG, "Skipping job '%s'\n", label );
			label = NULL;
		}
		nextField( );
	}
}

// Look for a ":" separator, or whitespace; handle quotes.
const char *JobListParser::nextField( void )
{
	char	quote = '\0';
	char	*start;
	char	*cur = curJobPointer;

	// Make sure that curJobPointer is valid..
	if ( NULL == curJobPointer ) {
		return NULL;
	}

	// Look for an openning quote
	if ( ( '\'' == *cur ) || ( '\"' == *cur ) ) {
		quote = *cur;
		cur++;
	}

	// Keep looking 'til end...
	start = cur;
	while( cur ) {

		// First, is this the end of a quoted chunk?
		if ( '\0' != quote )
		{
			if ( *cur == quote ) {
				char	next = *(cur+1);

				// If next is end, ok...
				if ( '\0' == next ) {
					*cur = '\0';			// Kill the quote
					curJobPointer = NULL;	// There is no next one
					return start;			// Done
				}
				// Whitespace: new job
				else if ( isspace( next ) ) {
					*cur = '\0';			// Terminate the string
					nextJobStart = cur + 2;	// Point at char *after* the separator
					curJobPointer = NULL;
					return start;
				}
				// Colon: Next field
				else if ( ':' ==  next ) {
					*cur = '\0';			// Terminate the string
					curJobPointer = cur + 2;	// Point at char *after* the separator
					return start;
				}
				// If next is not a space or :, badness 10000
				return NULL;

				// Ok, next is either a space or a :
			} else {
				cur++;
			}
		} else if ( '\0' != *cur ) {
			// Not quoted; separator?
			if ( ':' == *cur ) {
				*cur = '\0';
				curJobPointer = cur + 1;
				return start;
			}
			else if ( isspace( *cur ) )  {
				*cur = '\0';
				nextJobStart = cur + 1;
				curJobPointer = NULL;
				return start;
			} else {
				cur++;
			}
		} else {
			// End of string
			curJobPointer = NULL;
			nextJobStart = NULL;
			return start;
		}
	}

	// End of string
	curJobPointer = NULL;
	nextJobStart = NULL;
	return NULL;
}

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
	// Kill all running jobs
	Cron.DeleteAll( );

	// Free up name, etc. buffers
	if ( NULL != Name ) {
		free( (char *) Name );
	}
	if ( NULL != ParamBase ) {
		free( (void *) ParamBase );
	}

	// Log our death
	dprintf( D_FULLDEBUG, "CronMgr: bye\n" );
}

// Set new name..
int
CondorCronMgr::SetName( const char *newName, 
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
CondorCronMgr::KillAll( bool force)
{
	// Log our death
	dprintf( D_FULLDEBUG, "CronMgr: Killing all jobs\n" );

	// Reconfigure all running jobs
	return Cron.KillAll( force );
}

// Check: Are we ready to shutdown?
bool
CondorCronMgr::IsAllIdle( void )
{
	int		AliveJobs = Cron.NumAliveJobs( );

	dprintf( D_FULLDEBUG, "CronMgr: %d jobs alive\n", AliveJobs );
	return AliveJobs ? false : true;
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
	free( nameBuf );

	// Done
	return paramBuf;
}

// Parse the "Job List"
int
CondorCronMgr::ParseJobList( const char *jobString )
{
	// Debug
	dprintf( D_JOB, "CronMgr: Job string is '%s'\n", jobString );

	// Clear all marks
	Cron.ClearAllMarks( );

	// Walk through the job list
	JobListParser parser( jobString );
	while ( parser.nextJob() ) {
		unsigned	jobPeriod = 0;
		CronJobMode	jobMode = CRON_PERIODIC;

		// Parse it out; format is name:path:period[hms]
		const char *jobName = parser.getName( );
		if ( NULL == jobName ) {
			dprintf( D_ALWAYS, 
					 "CronMgr: skipping invalid job description '%s'"
					 " (No name)\n",
					 parser.getJobString( ) );
			continue;
		}

		// Parse out the prefix
		const char *jobPrefix = parser.getPrefix( );
		if ( NULL == jobPrefix ) {
			dprintf( D_ALWAYS,
					 "CronMgr: skipping invalid job description '%s'"
					 " (No prefix)\n",
					 parser.getJobString( ) );
			continue;
		}

		// Parse out the path
		const char *jobPath = parser.getPath( );
		if ( NULL == jobPath )
		{
			dprintf( D_ALWAYS, 
					 "CronMgr: skipping invalid job description '%s'"
					 " (No path)\n",
					 parser.getJobString( ) );
			continue;
		}

		// Pull out the period
		const char *jobPeriodStr = parser.getPeriod( );
		if ( NULL != jobPeriodStr ) {
			char	modifier;
			if ( sscanf( jobPeriodStr, "%d%c", &jobPeriod, &modifier ) < 1 ) {
				dprintf( D_ALWAYS,
						 "CronMgr: skipping invalid job description '%s'"
						 " (Bad Period '%s')\n",
						 parser.getJobString( ), jobPeriodStr );
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
						 "CronMgr: '%s': Invalid period modifier '%c'\n",
						 parser.getJobString( ), modifier );
				continue;
			}
		}

		// Parse any remaining options
		bool	killMode = false;
		while ( 1 ) {
			// Extract an option
			const char *option = parser.getOption( );
			if ( NULL == option ) {
				break;
			}

			// And, parse it
			if ( !strcasecmp( option, "kill" ) ) {
				dprintf( D_FULLDEBUG, "CronMgr: '%s': Kill option ok\n",
						 jobName );
				killMode = true;
			} else if ( !strcasecmp( option, "nokill" ) ) {
				dprintf( D_FULLDEBUG, "CronMgr: '%s': NoKill option ok\n",
						 jobName );
				killMode = false;
			} else if ( !strcasecmp( option, "continuous" ) ) {
				dprintf( D_FULLDEBUG, "CronMgr: '%s': Continuous option ok\n",
						 jobName );
				jobMode = CRON_CONTINUOUS;
			} else {
				dprintf( D_ALWAYS, "CronMgr: Job '%s':"
						 " Ignoring unknown option '%s'\n",
						 jobName, option );
			}
		}

		// we change period == 0 to period == 1 for continuous
		// jobs, so Hawkeye doesn't busy-loop if the job fails to
		// execute for some reason; we mark it as an error for
		// other job modes, since it's an invalid period...
		if( jobPeriod == 0 ) {
		  if ( jobMode == CRON_CONTINUOUS ) {
		    jobPeriod = 1;
		    dprintf( D_ALWAYS, 
			     "CronMgr: WARNING: Job '%s' period = 0, but this "
			     "can cause busy-loops; resetting to 1.\n",
			     jobName );
		  }
		  else {
		    dprintf( D_ALWAYS,
			     "CronMgr: ERROR: Job '%s' not continuous, but "
			     "period = 0; ignoring Job.\n", jobName );
		    continue; 
		  }
		}

		// Create the job & add it to the list (if it's new)
		CondorCronJob *job = Cron.FindJob( jobName );
		if ( NULL == job ) {
			job = NewJob( jobName );
			if ( NULL == job ) {
				dprintf( D_ALWAYS, "Cron: Failed to allocate job '%s'\n",
						 jobName );
				return -1;
			}

			// Put the job in the list
			if ( Cron.AddJob( jobName, job ) < 0 ) {
				dprintf( D_ALWAYS, "CronMgr: Error creating job '%s'\n", 
						 jobName );
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

		// Finally, have the job finish it's initialization
		job->Initialize( );
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
	CondorCronJob *job = new CondorCronJob( GetName(), name );
	return job;
}

// Parse the next tokenized 'chunk', sorta like strtok()
// Returns pointer to the next one..
char *
CondorCronMgr::NextTok( char *cur, const char *tok )
{
	// Look for the _next_ occurance
	char	*tmp = strstr( cur, tok );
	if ( NULL != tmp ) {
		*tmp++ = '\0';
		if ( '\0' == *tmp ) {
			tmp = NULL;
		}
	}

	// Done
	return tmp;
}

// Handles the death of a child (does nothing for now)
#if 0
int
CondorCronMgr::JobDied( CondorCronJob *DeadJob )
{
	(void) DeadJob;
	return 0;
}
#endif
