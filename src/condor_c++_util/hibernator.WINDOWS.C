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
#include "hibernator.h"
#include "hibernator-win32.h"

/***************************************************************
 * MsWindowsHibernator class
 ***************************************************************/

#if defined ( WIN32 )

/* Remove me when NMI updates the SDKs.  Need for the XP SDKs which
   do NOT declare the functions as C functions... */
extern "C" {
#include <powrprof.h>
}

MsWindowsHibernator::MsWindowsHibernator () throw () 
: Hibernator () {
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
MsWindowsHibernator::tryShutdown ( bool force ) const {
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

bool 
MsWindowsHibernator::enterState ( SLEEP_STATE level, bool force ) const {
	bool ok = ( level == ( getStates () & level ) ); /* 1 and only 1 state */
	if ( ok ) {
		switch ( level ) {
			/* S[1-3] will all be treated as sleep */
		case S1:
		case S2:
		case S3: 		
			ok = ( 0 != SetSuspendState ( FALSE, force, FALSE ) );
			break;
			/* S4 will all be treated as hibernate */
		case S4:
			ok = ( 0 != SetSuspendState ( TRUE, force, FALSE ) );
			break;
			/* S5 will be treated as shutdown (soft-off) */		
		case S5:
			ok = tryShutdown ( force );
			break;
		default:
			/* should never happen */
			ok = false;
		}
	} 
	if ( !ok ) {
		dprintf ( D_ALWAYS, "Programmer Error: Unknown power "
			"state: 0x%x\n", level );
	}
	return ok;
}

#endif // WIN32

