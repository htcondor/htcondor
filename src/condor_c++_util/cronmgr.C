/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_string.h"


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
	if( s ) {
		jobListString = strdup( s );
	} else { 
		jobListString = NULL;
	}
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
		dprintf( D_FULLDEBUG, "CronMgr: Trying to find a job in '%s'\n",
				 curJobPointer );

		// Now, try to parse it...

		// Name: must exist, non-zero length
		name = nextField( );
		if (  ( NULL == name ) || ( '\0' == *name )  ){
			dprintf( D_ALWAYS,
					 "CronMgr: Job parse error: Can't find a name\n" );
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
			dprintf( D_ALWAYS,
					 "CronMgr: '%s': parse error: Can't find a prefix\n",
					 name );
			skipJob( );
			continue;
		}

		// Path: must exist, non-zero length
		path = nextField( );
		if (  ( NULL == path ) || ( '\0' == *path )  ){
			dprintf( D_ALWAYS,
					 "Cron: '%s': parse error: Can't find a path\n", name );
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
					nextJobStart = cur + 2;	// Point at char *after* separator
					curJobPointer = NULL;
					return start;
				}
				// Colon: Next field
				else if ( ':' ==  next ) {
					*cur = '\0';			// Terminate the string
					curJobPointer = cur + 2; // Point at char *after* separator
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
CronMgrBase::CronMgrBase( const char *name )
{
	dprintf( D_FULLDEBUG, "CronMgr: Constructing '%s'\n", name );

	// Make sure that SetName doesn't try to free Name or ParamBase...
	Name = NULL;
	ParamBase = NULL;

	// Set 'em
	SetName( name, name, "_cron" );
}

// Basic destructor
CronMgrBase::~CronMgrBase( )
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

// Handle initialization
int
CronMgrBase::Initialize( void )
{
	return DoConfig( true );
}

// Set new name..
int
CronMgrBase::SetName( const char *newName, 
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
int CronMgrBase::SetParamBase( const char *newParamBase,
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
CronMgrBase::KillAll( bool force)
{
	// Log our death
	dprintf( D_FULLDEBUG, "CronMgr: Killing all jobs\n" );

	// Kill all running jobs
	return Cron.KillAll( force );
}

// Check: Are we ready to shutdown?
bool
CronMgrBase::IsAllIdle( void )
{
	int		AliveJobs = Cron.NumAliveJobs( );

	dprintf( D_FULLDEBUG, "CronMgr: %d jobs alive\n", AliveJobs );
	return AliveJobs ? false : true;
}

// Handle Reconfig
int
CronMgrBase::Reconfig( void )
{
	return DoConfig( false );
}

// Handle configuration
int
CronMgrBase::DoConfig( bool initial )
{
	char *paramBuf;

	// Clear all marks
	Cron.ClearAllMarks( );

	// Look for _JOBS first (any job here will be overriden by the
	// corresponding entry in _JOBLIST
	paramBuf = GetParam( "JOBS" );
	if ( paramBuf ) {
		dprintf( D_ALWAYS,
				 "Warning: The \"%s_JOBS\" configuration syntax "
				 "is obsolete and\n",
				 ParamBase );
		dprintf( D_ALWAYS,
				 "         is being replaced by the \"%s_JOBLIST\" syntax.\n",
				 ParamBase );
		dprintf( D_ALWAYS,
				 "         See the Condor manual for more details\n" );
		ParseOldJobList( paramBuf );
		free( paramBuf );
	}

	// Look for _JOBLIST
	paramBuf = GetParam( "JOBLIST" );
	if ( paramBuf != NULL ) {
		ParseJobList( paramBuf );
		free( paramBuf );
	}

	// Delete all jobs that didn't get marked
	Cron.DeleteUnmarked( );

	// And, initialize all jobs (they ignore it if already initialized)
	Cron.InitializeAll( );

	// Find our environment variable, if it exits..
	dprintf( D_FULLDEBUG, "CronMgr: Doing config (%s)\n",
			 initial ? "initial" : "reconfig" );

	// Reconfigure all running jobs
	if ( ! initial ) {
		Cron.Reconfig( );
	}

	// Done
	return 0;
}

// Read a parameter
char *
CronMgrBase::GetParam( const char *paramName, 
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
CronMgrBase::ParseJobList( const char *jobListString )
{
	// Debug
	dprintf( D_JOB, "CronMgr: Job string is '%s'\n", jobListString );

	// Break it into a string list
	StringList	jobList( jobListString );
	jobList.rewind( );

	// Parse out the job names
	const char *jobName;
	while( ( jobName = jobList.next()) != NULL ) {
		dprintf( D_JOB, "CronMgr: Job name is '%s'\n", jobName );

		// Parse out the prefix
		MyString paramPrefix     = GetParam( jobName, "_PREFIX" );
		MyString paramExecutable = GetParam( jobName, "_EXECUTABLE" );
		MyString paramPeriod     = GetParam( jobName, "_PERIOD" );
		MyString paramMode       = GetParam( jobName, "_MODE" );
		MyString paramReconfig   = GetParam( jobName, "_RECONFIG" );
		MyString paramKill       = GetParam( jobName, "_KILL" );
		MyString paramOptions    = GetParam( jobName, "_OPTIONS" );
		MyString paramArgs       = GetParam( jobName, "_ARGS" );
		MyString paramEnv        = GetParam( jobName, "_ENV" );
		MyString paramCwd        = GetParam( jobName, "_CWD" );
		bool jobOk = true;

		// Some quick sanity checks
		if ( paramExecutable.IsEmpty() ) {
			dprintf( D_ALWAYS, 
					 "CronMgr: No path found for job '%s'; skipping\n",
					 jobName );
			jobOk = false;
		}

		// Pull out the period
		unsigned	jobPeriod = 0;
		if ( paramPeriod.IsEmpty() ) {
			dprintf( D_ALWAYS,
					 "CronMgr: No job period found for job '%s': skipping\n",
					 jobName );
			jobOk = false;
		} else {
			char	modifier;
			int		num = sscanf( paramPeriod.Value(), "%d%c",
								  &jobPeriod, &modifier );
			if ( num < 1 ) {
				dprintf( D_ALWAYS,
						 "CronMgr: Invalid job period found "
						 "for job '%s' (%s): skipping\n",
						 jobName, paramPeriod.Value() );
				jobOk = false;
			} else {
				// Check the modifier
				modifier = toupper( modifier );
				if ( ( 0 == modifier ) || ( 'S' == modifier ) ) {	// Seconds
					// Do nothing
				} else if ( 'M' == modifier ) {
					jobPeriod *= 60;
				} else if ( 'H' == modifier ) {
					jobPeriod *= ( 60 * 60 );
				} else {
					dprintf( D_ALWAYS,
							 "CronMgr: Invalid period modifier "
							 "'%c' for job %s (%s)\n",
							 modifier, jobName, paramPeriod.Value() );
					jobOk = false;
				}
			}
		}

		// Options
		CronJobMode	jobMode = CRON_PERIODIC;
		bool		jobReconfig = false;
		bool		jobKillMode = false;

		// Parse the job mode
		if ( ! paramMode.IsEmpty() ) {
			if ( ! strcasecmp( paramMode.Value(), "Periodic" ) ) {
				jobMode =  CRON_PERIODIC;
			} else if ( ! strcasecmp( paramMode.Value(), "WaitForExit" ) ) {
				jobMode = CRON_WAIT_FOR_EXIT;
			} else {
				dprintf( D_ALWAYS,
						 "CronMgr: Unknown job mode for '%s'\n",
						 jobName );
			}
		}
		if ( ! paramReconfig.IsEmpty() ) {
			if ( ! strcasecmp( paramReconfig.Value(), "True" ) ) {
				jobReconfig = true;
			} else {
				jobReconfig = false;
			}
		}
		if ( ! paramKill.IsEmpty() ) {
			if ( ! strcasecmp( paramKill.Value(), "True" ) ) {
				jobKillMode = true;
			} else {
				jobKillMode = false;
			}
		}

		// Parse the option string
		if ( ! paramOptions.IsEmpty() ) {
			StringList	list( paramOptions.Value(), " :," );
			list.rewind( );

			const char *option;
			while( ( option = list.next()) != NULL ) {

				// And, parse it
				if ( !strcasecmp( option, "kill" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': Kill option ok\n",
							 jobName );
					jobKillMode = true;
				} else if ( !strcasecmp( option, "nokill" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': NoKill option ok\n",
							 jobName );
					jobKillMode = false;
				} else if ( !strcasecmp( option, "reconfig" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': Reconfig option ok\n",
							 jobName );
					jobReconfig = true;
				} else if ( !strcasecmp( option, "noreconfig" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': NoReconfig option ok\n",
							 jobName );
					jobReconfig = false;
				} else if ( !strcasecmp( option, "WaitForExit" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': WaitForExit option ok\n",
							 jobName );
					jobMode = CRON_WAIT_FOR_EXIT;
				} else {
					dprintf( D_ALWAYS,
							 "CronMgr: Job '%s': "
							 "Ignoring unknown option '%s'\n",
							 jobName, option );
				}
			}
		}

		// Are there arguments for it?
		ArgList args;
		MyString args_errors;

		// Force the first arg to be the "Job Name"..
		args.AppendArg(jobName);

		if( !args.AppendArgsV1RawOrV2Quoted( paramArgs.Value(),
											 &args_errors ) ) {
			dprintf( D_ALWAYS,
					 "CronMgr: Job '%s': "
					 "Failed to parse arguments: '%s'\n",
					 jobName, args_errors.Value());
			jobOk = false;
		}

		// Parse the environment.
		Env envobj;
		MyString env_error_msg;

		if( !envobj.MergeFromV1RawOrV2Quoted( paramEnv.Value(),
											  &env_error_msg ) ) {
			dprintf( D_ALWAYS,
					 "CronMgr: Job '%s': "
					 "Failed to parse environment: '%s'\n",
					 jobName, env_error_msg.Value());
			jobOk = false;
		}


		// Create the job & add it to the list (if it's new)
		CronJobBase *job = NULL;
		if ( jobOk ) {
			bool add_it = false;
			job = Cron.FindJob( jobName );
			if ( NULL == job ) {
				job = NewJob( jobName );
				add_it = true;

				// Ok?
				if ( NULL == job ) {
					dprintf( D_ALWAYS,
							 "Cron: Failed to allocate job object for '%s'\n",
							 jobName );
				}
			}

			// Put the job in the list
			if ( NULL != job ) {
				if ( add_it ) {
					if ( Cron.AddJob( jobName, job ) < 0 ) {
						dprintf( D_ALWAYS,
								 "CronMgr: Error creating job '%s'\n", 
								 jobName );
						delete job;
						job = NULL;
					}
				} else {
					dprintf( D_FULLDEBUG,
							 "CronMgr: Not adding duplicate job '%s' (OK)\n",
							 jobName );
				}
			}
		}

		// Now fill in the job details
		if ( NULL == job ) {
			dprintf( D_ALWAYS,
					 "Cron: Can't create job for '%s'\n",
					 jobName );
		} else {
			job->SetKill( jobKillMode );
			job->SetReconfig( jobReconfig );

			// And, set it's characteristics
			job->SetPath( paramExecutable.Value() );
			job->SetPrefix( paramPrefix.Value() );
			job->SetArgs( args );
			job->SetCwd( paramCwd.Value() );
			job->SetPeriod( jobMode, jobPeriod );

			char **env_array = envobj.getStringArray();
			job->SetEnv( env_array );
			deleteStringArray(env_array);

			// Mark the job so that it doesn't get deleted (below)
			job->Mark( );
		}

		// Debug info
		dprintf( D_FULLDEBUG,
				 "CronMgr: Done processing job '%s'\n", jobName );

		// Job initialization is done by Cron::InitializeAll()
	}

	// All ok
	return 0;
}

// Parse the "Job List": Old format
int
CronMgrBase::ParseOldJobList( const char *jobString )
{
	// Debug
	dprintf( D_JOB, "CronMgr: Old job string is '%s'\n", jobString );

	// Walk through the job list
	JobListParser parser( jobString );
	while ( parser.nextJob() ) {
		unsigned	jobPeriod = 0;
		CronJobMode	jobMode = CRON_PERIODIC;

		// Parse it out; format is name:path:period[hms]
		const char *jobName = parser.getName( );
		if ( NULL == jobName ) {
			dprintf( D_ALWAYS, 
					 "CronMgr(old): skipping invalid job description '%s'"
					 " (No name)\n",
					 parser.getJobString( ) );
			continue;
		}

		// Parse out the prefix
		const char *jobPrefix = parser.getPrefix( );
		if ( NULL == jobPrefix ) {
			dprintf( D_ALWAYS,
					 "CronMgr(old): skipping invalid job description '%s'"
					 " (No prefix)\n",
					 parser.getJobString( ) );
			continue;
		}

		// Parse out the path
		const char *jobPath = parser.getPath( );
		if ( NULL == jobPath )
		{
			dprintf( D_ALWAYS, 
					 "CronMgr(old): skipping invalid job description '%s'"
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
						 "CronMgr(old): skipping invalid job description '%s'"
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
						 "CronMgr(old): '%s': Invalid period modifier '%c'\n",
						 parser.getJobString( ), modifier );
				continue;
			}
		}

		// Parse any remaining options
		bool	killMode = false;
		bool	reconfig = false;
		while ( 1 ) {
			// Extract an option
			const char *option = parser.getOption( );
			if ( NULL == option ) {
				break;
			}

			// And, parse it
			if ( !strcasecmp( option, "kill" ) ) {
				dprintf( D_FULLDEBUG,
						 "CronMgr(old): '%s': Kill option ok\n",
						 jobName );
				killMode = true;
			} else if ( !strcasecmp( option, "nokill" ) ) {
				dprintf( D_FULLDEBUG,
						 "CronMgr(old): '%s': NoKill option ok\n",
						 jobName );
				killMode = false;
			} else if ( !strcasecmp( option, "reconfig" ) ) {
				dprintf( D_FULLDEBUG,
						 "CronMgr(old): '%s': Reconfig option ok\n",
						 jobName );
				reconfig = true;
			} else if ( !strcasecmp( option, "noreconfig" ) ) {
				dprintf( D_FULLDEBUG,
						 "CronMgr(old): '%s': NoReconfig option ok\n",
						 jobName );
				reconfig = false;
			} else if ( !strcasecmp( option, "WaitForExit" ) ) {
				dprintf( D_FULLDEBUG,
						 "CronMgr(old): '%s': WaitForExit option ok\n",
						 jobName );
				jobMode = CRON_WAIT_FOR_EXIT;
			} else if ( !strcasecmp( option, "continuous" ) ) {
				dprintf( D_FULLDEBUG,
						 "CronMgr(old): '%s': Continuous option ok\n",
						 jobName );
				jobMode = CRON_WAIT_FOR_EXIT;
				reconfig = true;
			} else {
				dprintf( D_ALWAYS, "CronMgr(old): Job '%s':"
						 " Ignoring unknown option '%s'\n",
						 jobName, option );
			}
		}

		// we change period == 0 to period == 1 for continuous
		// (now called WaitForExit)
		// jobs, so Hawkeye doesn't busy-loop if the job fails to
		// execute for some reason; we mark it as an error for
		// other job modes, since it's an invalid period...
		if( jobPeriod == 0 ) {
			if ( jobMode == CRON_WAIT_FOR_EXIT ) {
				jobPeriod = 1;
				dprintf( D_ALWAYS, 
						 "CronMgr(old): "
						 "WARNING: Job '%s' period = 0, but this "
						 "can cause busy-loops; resetting to 1.\n",
						 jobName );
			} else {
				dprintf( D_ALWAYS,
						 "CronMgr(old): "
						 "ERROR: Job '%s' not 'WaitForExit', but "
						 "period = 0; ignoring Job.\n",
						 jobName );
				continue; 
			}
		}

		// Are there arguments for it?
		// Force the first arg to be the "Job Name"..
		char *paramArgs = GetParam( jobName, "_ARGS" );

		// Are there arguments for it?
		// Force the first arg to be the "Job Name"..
		ArgList args;
		MyString args_errors;

		// Force the first arg to be the "Job Name"..
		args.AppendArg(jobName);

		if(!args.AppendArgsV1RawOrV2Quoted(paramArgs,&args_errors)) {
			dprintf( D_ALWAYS,
					 "CronMgr(old): Job '%s': "
					 "Failed to parse arguments: %s\n",
					 jobName, args_errors.Value());
			free(paramArgs);
			continue;
		}
		free(paramArgs);

		// Special environment vars?
		char *paramEnv       = GetParam( jobName, "_ENV" );

		// Parse the environment.
		Env envobj;
		MyString env_error_msg;

		if( !envobj.MergeFromV1RawOrV2Quoted( paramEnv, &env_error_msg ) ) {
			dprintf( D_ALWAYS,
					 "CronMgr(old): Job '%s': "
					 "Failed to parse environment: '%s'\n",
					 jobName, env_error_msg.Value());
			free(paramEnv);
			continue;
		}
		free(paramEnv);

		// Create the job & add it to the list (if it's new)
		CronJobBase *job = Cron.FindJob( jobName );
		if ( NULL == job ) {
			job = NewJob( jobName );
			if ( NULL == job ) {
				dprintf( D_ALWAYS, "Cron: Failed to allocate job '%s'\n",
						 jobName );
				return -1;
			}

			// Put the job in the list
			if ( Cron.AddJob( jobName, job ) < 0 ) {
				dprintf( D_ALWAYS, "CronMgr(old): Error creating job '%s'\n", 
						 jobName );
			}
		}

		// Set the "Kill" mode
		job->SetKill( killMode );
		job->SetReconfig( reconfig );

		// CWD?
		char *cwdBuf = GetParam( jobName, "_CWD" );


		// Mark the job so that it doesn't get deleted (below)
		job->Mark( );

		// And, set it's characteristics
		job->SetPath( jobPath );
		job->SetPrefix( jobPrefix );
		job->SetArgs( args );
		job->SetCwd( cwdBuf );
		job->SetPeriod( jobMode, jobPeriod );

		char **env_array = envobj.getStringArray();
		job->SetEnv( env_array );
		deleteStringArray(env_array);

		// Free up memory from param()
		if ( cwdBuf ) {
			free( cwdBuf );
		}

		// Debug info
		dprintf( D_FULLDEBUG,
				 "CronMgr(old): Done processing job '%s'\n", jobName );

		// Job initialization is done by Cron::InitializeAll()
	}

	// All ok
	return 0;
}

// Parse the next tokenized 'chunk', sorta like strtok()
// Returns pointer to the next one..
char *
CronMgrBase::NextTok( char *cur, const char *tok )
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
