/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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
#include "hibernator.WINDOWS.h"
#include "security.WINDOWS.h"

/* Remove me when NMI updates the SDKs.  Need for the XP SDKs which
   do NOT declare the functions as C functions... */
extern "C" {
#include <powrprof.h>
}

/***************************************************************
 * MsWindowsHibernator class
 ***************************************************************/

MsWindowsHibernator::MsWindowsHibernator( void ) noexcept
		: HibernatorBase ()
{
}

MsWindowsHibernator::~MsWindowsHibernator( void ) noexcept
{
}

bool
MsWindowsHibernator::initialize ( void )
{

	LONG                      status;
	SYSTEM_POWER_CAPABILITIES capabilities;
	USHORT					  states		= NONE;
	BOOL					  privileged	= FALSE;

	/* allow Condor to shutdown the OS */
	privileged = ModifyPrivilege ( SE_SHUTDOWN_NAME, TRUE );

	if ( !privileged ) {

		dprintf (
			D_ALWAYS,
			"MsWindowsHibernator::initStates: Failed to grant "
			"Condor the ability to shutdown this machine. "
			"(last-error = %d)\n",
			GetLastError () );

		return false;

	}

	/* set defaults: no sleep */
	setStates ( NONE );

	/* retrieve power information */
	status = CallNtPowerInformation (
		SystemPowerCapabilities,
		NULL,
		0,
		&capabilities,
		sizeof ( SYSTEM_POWER_CAPABILITIES ) );

	if ( ERROR_SUCCESS != status ) {

		dprintf (
			D_ALWAYS,
			"MsWindowsHibernator::initStates: Failed to retrieve "
			"power information. (last-error = %d)\n",
			GetLastError () );

		return false;
	}

	/* Finally, set the remaining supported states */
	states |= capabilities.SystemS1;
	states |= capabilities.SystemS2 << 1;
	states |= capabilities.SystemS3 << 2;
	states |= capabilities.SystemS4 << 3;
	states |= capabilities.SystemS5 << 4;
	setStates ( states );

	setInitialized( true );
	return true;
}

bool
MsWindowsHibernator::tryShutdown ( bool force ) const
{
	#pragma warning(suppress: 28159) // suppress 're-architect to avoid reboot message'...
	bool ok = ( TRUE == InitiateSystemShutdownEx (
		NULL	/* local computer */,
		NULL	/* no warning message */,
		0		/* shutdown immediately */,
		force	/* should we force applications to close? */,
		FALSE	/* no reboot */,
		SHTDN_REASON_MAJOR_APPLICATION
		| SHTDN_REASON_FLAG_PLANNED ) );

	if ( !ok ) {

		DWORD last_error = GetLastError ();

		/* if the computer is already shutting down, we interpret
		   this as success... */
		if ( ERROR_SHUTDOWN_IN_PROGRESS == last_error ) {
			return true;
		}

		/* otherwise, it's an error and we'll tell the user so */
		dprintf (
			D_ALWAYS,
			"MsWindowsHibernator::tryShutdown(): Shutdown failed. "
			"(last-error = %d)\n",
			last_error );

	}

	return ok;
}

HibernatorBase::SLEEP_STATE
MsWindowsHibernator::enterStateStandBy ( bool force ) const
{
    if ( !SetSuspendState ( FALSE, force, FALSE ) ) {
        return HibernatorBase::NONE;
    }
	return HibernatorBase::S3;
}

HibernatorBase::SLEEP_STATE
MsWindowsHibernator::enterStateSuspend ( bool force ) const
{
    if ( !SetSuspendState ( FALSE, force, FALSE ) ) {
        return HibernatorBase::NONE;
    }
	return HibernatorBase::S3;
}

HibernatorBase::SLEEP_STATE
MsWindowsHibernator::enterStateHibernate ( bool force ) const
{
    if ( !SetSuspendState ( TRUE, force, FALSE ) ) {
        return HibernatorBase::NONE;
    }
	return HibernatorBase::S4;
}

HibernatorBase::SLEEP_STATE
MsWindowsHibernator::enterStatePowerOff ( bool force ) const
{
    if ( !tryShutdown ( force ) ) {
        return HibernatorBase::NONE;
    }
	return HibernatorBase::S5;
}

