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
#include "condor_debug.h"
#include "hibernator.h"


/***************************************************************
 * Base Hibernator class
 ***************************************************************/

HibernatorBase::HibernatorBase () throw () 
		: m_states ( NONE )
{
}

HibernatorBase::~HibernatorBase () throw ()
{
}

bool 
HibernatorBase::doHibernate ( SLEEP_STATE level, bool force ) const
{
	if ( level == ( m_states & level ) ) {
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
HibernatorBase::getStates () const
{
	return m_states;
}

void
HibernatorBase::setStates ( unsigned short states )
{
	m_states = states;
}

void
HibernatorBase::addState ( SLEEP_STATE state )
{
	m_states |= state;
}

void
HibernatorBase::addState ( const char *statestr )
{
	SLEEP_STATE state = stringToSleepState ( statestr );
	m_states |= state;
}

HibernatorBase::SLEEP_STATE
HibernatorBase::setState ( SLEEP_STATE state )
{
	m_state = state;
}



/***************************************************************
 * Hibernator static members 
 ***************************************************************/

/* factory method */

HibernatorBase* 
HibernatorBase::createHibernator ( void )
{
	HibernatorBase *hibernator = NULL;
# if defined ( WIN32 )
	hibernator = new MsWindowsHibernator ();
# elif defined( HAVE_LINUX_HIBERNATION )

# endif
	return hibernator;
}

/* conversion methods */
struct HibernatorStates {
	int							number;
	HibernatorBase::SLEEP_STATE	state;
	char						*string;
	char						*string2;
};
static HibernatorStates const states[] =
{
	{ 0,  HibernatorBase::NONE, "NONE", NULL      },
	{ 1,  HibernatorBase::S1,   "S1",   "standby" },
	{ 2,  HibernatorBase::S2,   "S2",   NULL      },
	{ 3,  HibernatorBase::S3,   "S3",   "ram"     },
	{ 4,  HibernatorBase::S4,   "S4",   "disk"    },
	{ 5,  HibernatorBase::S5,   "S5",   NULL      },
	{ -1, HibernatorBase::NONE, NULL,   NULL      },
};

HibernatorBase::SLEEP_STATE 
HibernatorBase::intToSleepState ( int x )
{
	SLEEP_STATE state = NONE;
	if ( x > 0 && x <= 5 ) {
		state = states[x].state;
	}
	return state;
}

int 
HibernatorBase::sleepStateToInt ( HibernatorBase::SLEEP_STATE state )
{
	for( int i = 0;  states[i].string;  i++ ) {
		if ( states[i].state == state ) {
			return i;
		}
	}
	return 0;
}

char const* 
HibernatorBase::sleepStateToString ( HibernatorBase::SLEEP_STATE state )
{
	int index = sleepStateToInt ( state );
	return states[index].string;
}

HibernatorBase::SLEEP_STATE 
HibernatorBase::stringToSleepState ( char const* name )
{
	const HibernatorStates	*s = &states[0];

	for( int i = 0;  states[i].string;  i++ ) {
		s = &states[i];
		if ( (strcmp( s->string, name ) == 0) ||
			 ((s->string2) && (strcmp(s->string2, name) ) == 0) )
		{
			 break;
		}
	}
	return s->state;
}
