/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
 
/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "hibernator.tools.h"
#include "path_utils.h"
#include <condor_daemon_core.h>

/***************************************************************
 * UserDefinedToolsHibernator class
 ***************************************************************/

UserDefinedToolsHibernator::UserDefinedToolsHibernator () noexcept
	: HibernatorBase (),
	m_keyword ( "HIBERNATE" ),
	m_reaper_id ( -1 )
{
	for ( unsigned i = 0; i <= 10; ++i ) {
		m_tool_paths[i] = NULL;
	}
	configure ();
}

UserDefinedToolsHibernator::UserDefinedToolsHibernator ( const MyString &keyword ) noexcept 
	: HibernatorBase (),
	m_keyword ( keyword ),
	m_reaper_id ( -1 )
{
	for ( unsigned i = 0; i <= 10; ++i ) {
		m_tool_paths[i] = NULL;
	}
	configure ();
}

UserDefinedToolsHibernator::~UserDefinedToolsHibernator () noexcept 
{
	for ( unsigned i = 1; i <= 10; ++i ) {
		if ( NULL != m_tool_paths[i] ) {
			free ( m_tool_paths[i] );
			m_tool_paths[i] = NULL;
		}
	}
	if ( -1 != m_reaper_id ) {
		daemonCore->Cancel_Reaper ( m_reaper_id );
	}
}

const char*
UserDefinedToolsHibernator::getMethod ( void ) const
{
	return "user defined tools";
}

/** We define a 'C' style Reaper for use in configure() to eliminate
	the problem of a cast being made between different pointer to
	member representations (because of multiple inheritance--C++
	Reapers require the object to be of type Service); If we used a
	C++ style Reaper the compiler may generate incorrect code. */
int UserDefinedToolsHibernator::userDefinedToolsHibernatorReaper ( int pid, int )
{
	/** Make sure the hibernator didn't leak any processes */
	daemonCore->Kill_Family ( pid );
	return TRUE;
}

void UserDefinedToolsHibernator::configure ()
{

	MyString					name,
								error;
	unsigned					states			= HibernatorBase::NONE;
	const char					*description	= NULL;
	char						*arguments		= NULL;
	HibernatorBase::SLEEP_STATE state			= HibernatorBase::NONE;
	bool						ok				= false;

	/** There are no tools for S0, or "NONE" */
	m_tool_paths[0] = NULL;

	/** Pull the paths for the rest of the sleep states from
		the configuration file */
	for ( unsigned i = 1; i <= 10; ++i ) {
		
		/** Clean out the old path information */
		if ( NULL != m_tool_paths[i] ) {
			free ( m_tool_paths[i] );
			m_tool_paths[i] = NULL;
		}

		/** Convert the current index to the sleep state equivalent */
		state = HibernatorBase::intToSleepState ( i );
		
		if ( HibernatorBase::NONE == state ) {
			continue;
		}
		
		/** Convert the sleep state to a human consumable description */
		description = HibernatorBase::sleepStateToString ( state );

		if ( NULL == description ) {
			continue;
		}

		dprintf (
			D_FULLDEBUG,
			"UserDefinedToolsHibernator: state = %d, desc = %s\n",
			state,
			"S1" );
		
		/** Build the tool look-up parameter for the tool's path */
		name.formatstr ( 
			"%s_USER_%s_TOOL", 
			"HIBERNATE",
			"S1" );
		
		/** Grab the user defined executable path */
		m_tool_paths[i] = validateExecutablePath ( name.c_str () );
		
		if ( NULL != m_tool_paths[i] ) {

			/** Make the path the first argument to Create_Process */
			m_tool_args[i].AppendArg ( m_tool_paths[i] );

			/** Build the tool look-up parameter for the tool's
				argument list */
			name.formatstr ( 
				"%s_USER_%s_ARGS", 
				m_keyword.c_str(),
				description );

			/** Grab the command's arguments */
			arguments = param ( name.c_str () );

			if ( NULL != arguments ) {
			
				/** Parse the command-line arguments */
				ok = m_tool_args[i].AppendArgsV1WackedOrV2Quoted ( 
					arguments, 
					&error );

				if ( !ok ) {

					dprintf (
						D_FULLDEBUG,
						"UserDefinedToolsHibernator::configure: failed "
						"to parse the tool arguments defined in the "
						"configuration file: %s\n",
						error.c_str() );

				}

				/** Dump the param'd value */
				free ( arguments );

			}

			/** Tally the supported state */
			states |= state;

		} else {

			dprintf (
				D_FULLDEBUG,
				"UserDefinedToolsHibernator::configure: the executable "
				"(%s) defined in the configuration file is invalid.\n",
				name.c_str());

		}
	}
	
	/** Now set the supported states */
	setStates ( states );

	/** Finally, register the reaper that will clean up after the
		user defined tool and its children */
	m_reaper_id = daemonCore->Register_Reaper (
		"UserDefinedToolsHibernator Reaper",
		(ReaperHandlercpp) &UserDefinedToolsHibernator::userDefinedToolsHibernatorReaper,
		"UserDefinedToolsHibernator Reaper",
		NULL );

}

HibernatorBase::SLEEP_STATE 
UserDefinedToolsHibernator::enterState ( HibernatorBase::SLEEP_STATE state ) const
{
	
	/** Make sure a tool for this sleep state has been defined */
	unsigned index = sleepStateToInt ( state );

	if ( NULL == m_tool_paths[index] ) {
		dprintf ( 
			D_FULLDEBUG, 
			"Hibernator::%s tool not configured.\n",
			HibernatorBase::sleepStateToString ( state ) );
		return HibernatorBase::NONE;
	}

	/** Tell DaemonCore to register the process family so we can
		safely kill everything from the reaper */
	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer (
		"PID_SNAPSHOT_INTERVAL", 
		15 );

	/** Run the user tool */
	int pid = daemonCore->Create_Process (
		m_tool_paths[index], 
		m_tool_args[index], 
		PRIV_CONDOR_FINAL,
		m_reaper_id, 
		FALSE, 
		FALSE, 
		NULL, 
		NULL, 
		&fi );	

	if ( FALSE == pid ) {
		dprintf ( 
			D_ALWAYS, 
			"UserDefinedToolsHibernator::enterState: Create_Process() "
			"failed\n" );
		return HibernatorBase::NONE;
	}

	return state;

}

HibernatorBase::SLEEP_STATE
UserDefinedToolsHibernator::enterStateStandBy ( bool /*force*/ ) const
{
	return enterState ( HibernatorBase::S1 );
}

HibernatorBase::SLEEP_STATE
UserDefinedToolsHibernator::enterStateSuspend ( bool /*force*/ ) const
{
    return enterState ( HibernatorBase::S3 );
}

HibernatorBase::SLEEP_STATE
UserDefinedToolsHibernator::enterStateHibernate ( bool /*force*/ ) const
{
    return enterState ( HibernatorBase::S4 );
}

HibernatorBase::SLEEP_STATE
UserDefinedToolsHibernator::enterStatePowerOff ( bool /*force*/ ) const
{
    return enterState ( HibernatorBase::S5 );
}
