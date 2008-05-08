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
#include "power_hibernator.h"

/***************************************************************
 * Base Hibernator class
 ***************************************************************/

Hibernator::Hibernator () throw () 
: _states ( NONE ) {
}

Hibernator::~Hibernator () throw () {
}

bool 
Hibernator::doHibernate ( SLEEP_STATE level, bool force ) const {
	if ( level == ( _states & level ) ) {
		dprintf ( D_FULLDEBUG, "Hibernator: Entering sleep "
			"state '%s'.\n", sleepStateToString ( level ) );
		return enterState ( level, force );
	} else {
		dprintf ( D_FULLDEBUG, "Hibernator: This machine does not "
			"support low power state: x%x\n", level );
	}
	return false;
}

unsigned short 
Hibernator::getStates () const {
	return _states;
}

void
Hibernator::setStates ( unsigned short states ) {
	_states = states;
}

/***************************************************************
 * Hibernator static members 
 ***************************************************************/

/* factory method */

Hibernator* Hibernator::createHibernator () {
	Hibernator *hibernator = NULL;
#if defined ( WIN32 )
	hibernator = new MsWindowsHibernator ();
#endif
	return hibernator;
}

/* conversion methods */

Hibernator::SLEEP_STATE 
Hibernator::intToSleepState ( int x ) {
	static SLEEP_STATE const states[] = { 
		NONE, S1, S2, S3, S4, S5 };
	SLEEP_STATE state = NONE;
	if ( x > 0 && x <= 5 ) {
		state = states[x];		
	}
	return state;
}

int 
Hibernator::sleepStateToInt ( Hibernator::SLEEP_STATE state ) {
	int index = 0;
	if ( state > NONE && state <= S5 ) {
		/* sick switch statement... */
		switch ( state ) {
		case S1:
			index = 1;
			break;
		case S2:
			index = 2;
			break;
		case S3:
			index = 3;
			break;
		case S4:
			index = 4;
			break;
		case S5:
			index = 5;
			break;
		default:
			/* should not happen, but will, nevertheless,
			   default to Hibernator::NONE */
			break;
		}
	}
	return index;
}

char const* 
Hibernator::sleepStateToString ( Hibernator::SLEEP_STATE state ) {
	static char const *names[] = { 
		"NONE", "S1", "S2", "S3", "S4", "S5" };
	int index = sleepStateToInt ( state );
	return names[index];
}

#if 0
Hibernator::SLEEP_STATE 
Hibernator::stringToSleepState ( char const* name ) {
	static SLEEP_STATE const states[] = { 
		NONE, S1, S2, S3, S4, S5 };
	int first = ( '"' == name[0] ? 1 : 0 );
	SLEEP_STATE state = NONE;
	/* all strings are of the form S/n/ where n is in [1..5] */
	if ( name[first] == 'S' || name[first] == 's' ) {
		int index = name[first + 1] - '0';
		if ( index > 0 && index <= 5 ) {
			// index = 1 << ( index - 1 );
			state = states[index];
		}
	}
	return state;
}
#endif

/***************************************************************
 * Specialized classes 
 ***************************************************************/

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

