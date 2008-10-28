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
#include "hibernator.WINDOWS.h"

/* Remove me when NMI updates the SDKs.  Need for the XP SDKs which
   do NOT declare the functions as C functions... */
extern "C" {
#include <powrprof.h>
}

/***************************************************************
 * MsWindowsHibernator class
 ***************************************************************/

MsWindowsHibernator::MsWindowsHibernator () throw () 
: HibernatorBase () {
	initStates ();
}

MsWindowsHibernator::~MsWindowsHibernator () throw () {
}

void
MsWindowsHibernator::initStates () {
	
	LONG                      status;
	SYSTEM_POWER_CAPABILITIES capabilities;
	USHORT					  states = NONE;

	/* set defaults: no sleep */
	setStates ( NONE );

	/* retrieve power information */
	status = CallNtPowerInformation ( SystemPowerCapabilities, 
		NULL, 0, &capabilities, sizeof ( SYSTEM_POWER_CAPABILITIES ) );
	if ( ERROR_SUCCESS != status ) {
		printf ( "Failed to retrieve power information\n" );
		return;
	}

	/* S4 requires that the Hibernation file exist as well */
	capabilities.SystemS4 &= capabilities.HiberFilePresent;

	/* Finally, set supported states */
	states |= capabilities.SystemS1;
	states |= capabilities.SystemS2 << 1;
	states |= capabilities.SystemS3 << 2;
	states |= capabilities.SystemS4 << 3;
	states |= capabilities.SystemS5 << 4;
	setStates ( states );

}

bool
MsWindowsHibernator::tryShutdown ( bool force ) const
{
	bool ok = ( TRUE == InitiateSystemShutdownEx (
		NULL	/* local computer */, 
		NULL	/* no warning message */, 
		0		/* shutdown immediately */, 
		force	/* should we force applications to close? */, 
		FALSE	/* no reboot */, 
		SHTDN_REASON_MAJOR_APPLICATION 
		| SHTDN_REASON_FLAG_PLANNED ) );
	if ( !ok ) {
		/* if the computer is already shutting down, we interpret 
		   this as success... is this correct? */
		if ( ERROR_SHUTDOWN_IN_PROGRESS == GetLastError () ) {
			ok = true;
		}
	}
	return ok;
}

HibernatorBase::SLEEP_STATE
MsWindowsHibernator::enterStateStandBy ( bool force ) const
{
    if ( !SetSuspendState ( FALSE, force, FALSE ) ) {
        return SLEEP_STATE::S3;
    }
	return SLEEP_STATE::NONE;
}

HibernatorBase::SLEEP_STATE
MsWindowsHibernator::enterStateSuspend ( bool force ) const
{
    if ( !SetSuspendState ( FALSE, force, FALSE ) ) {
        return SLEEP_STATE::S3;
    }
	return SLEEP_STATE::NONE;
}

HibernatorBase::SLEEP_STATE
MsWindowsHibernator::enterStateHibernate ( bool force ) const
{
    if ( !SetSuspendState ( TRUE, force, FALSE ) ) {
        return SLEEP_STATE::S4;
    }
	return SLEEP_STATE::NONE;
}

HibernatorBase::SLEEP_STATE
MsWindowsHibernator::enterStatePowerOff ( bool force ) const
{
    if ( !tryShutdown ( force ) ) {
        return SLEEP_STATE::S4;
    }
	return SLEEP_STATE::NONE;
}

