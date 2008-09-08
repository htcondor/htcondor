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
#include "stat_wrapper.h"
#include "hibernator.h"
#include "hibernator-linux.h"


# if HAVE_LINUX_SOCKIOS_H
#   include "linux/sockios.h"
# endif


/***************************************************************
 * Linux Hibernator class
 ***************************************************************/

# if HAVE_LINUX_HIBERNATION
const char *SYS_POWER_FILE =	"/sys/power/state";
const char *SYS_DISK_FILE =		"/sys/power/disk";
const char *PROC_POWER_FILE =	"/proc/acpi/sleep";

const char *PM_UTIL_CHECK =     "/usr/bin/pm-is-supported";
const char *PM_UTIL_SUSPEND =   "/usr/sbin/pm-suspend";
const char *PM_UTIL_HIBERNATE = "/usr/sbin/pm-hibernate";

LinuxHibernator::LinuxHibernator () throw () 
		: HibernatorBase ()
{
	initStates ();
}

LinuxHibernator::~LinuxHibernator () throw ()
{
}

void
LinuxHibernator::initStates ()
{
	FILE	*fp;
	char	buf[128];

	// set defaults: no sleep
	m_has_pm_utils = false;
	m_has_proc_if = false;
	m_has_sys_if = false;
	setStates ( NONE );

	// Do we have "pm-utils" installed?
	StatWrapper sw( PM_UTIL_CHECK );
	if ( sw.GetStatus() == 0 ) {
		m_has_pm_utils = true;
	}

	// Look at the "/sys" file(s)
	if ( (fp = safe_fopen_wrapper( SYS_POWER_FILE, "r" )) != NULL) {
		m_has_sys_if = true;
		if ( fgets( buf, sizeof(buf)-1, fp ) ) {
			char	*token, *save = NULL;

			token = strtok_r( buf, " ", &save );
			while( token ) {
				addState( token );
				token = strtok_r( NULL, " ", &save );
			}
		}
		fclose( fp );

		if ( (fp = safe_fopen_wrapper( SYS_DISK_FILE, "r" )) != NULL) {
			if ( fgets( buf, sizeof(buf)-1, fp ) ) {
				char	*token, *save = NULL;

				token = strtok_r( buf, " ", &save );
				while( token ) {
					int len = strlen( token );
					if ( token[0] == '[' && token[len] == ']' ) {
						token[len] = '\0';
						token++;
					}
					if ( strcmp( token, "platform" ) == 0 ) {
						addState( S4 );
					}
					else if ( strcmp( token, "shutdown" ) == 0 ) {
						addState( S5 );
					}
				}
			}
			fclose( fp );
		}
	}

	// Look in /proc
	memset( buf, 0, sizeof(buf) );
	if ( ( fp = safe_fopen_wrapper( PROC_POWER_FILE, "r" ) ) != NULL ) {
		m_has_proc_if = true;
		if ( fgets( buf, sizeof(buf)-1, fp ) ) {
			char	*token, *save = NULL;

			token = strtok_r( buf, " ", &save );
			while( token ) {
				addState( token );
				token = strtok_r( NULL, " ", &save );
			}
		}
		fclose( fp );
	}
}

bool
LinuxHibernator::tryShutdown ( bool force ) const
{
	bool	ok;
	if ( m_has_pm_utils ) {
		
	}
	else if ( m_has_sys_if ) {
	}
	else if ( m_has_proc_if ) {
	}
	return ok;
}

bool 
LinuxHibernator::enterState ( SLEEP_STATE level, bool force ) const
{

#if 0
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

#endif
}


# endif // HAVE_LINUX_HIBERNATION
