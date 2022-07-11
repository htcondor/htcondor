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
#include "condor_perms.h"
#include "condor_config.h"

const char*
PermString( DCpermission perm )
{
	switch( perm ) {
	case ALLOW:
		return "ALLOW";
	case READ:
		return "READ";
	case WRITE:
		return "WRITE";
	case NEGOTIATOR:
		return "NEGOTIATOR";
	case ADMINISTRATOR:
		return "ADMINISTRATOR";
	case CONFIG_PERM:
		return "CONFIG";
    case DAEMON:
        return "DAEMON";
	case SOAP_PERM:
		return "SOAP";
	case DEFAULT_PERM:
		return "DEFAULT";
	case CLIENT_PERM:
		return "CLIENT";
	case ADVERTISE_STARTD_PERM:
		return "ADVERTISE_STARTD";
	case ADVERTISE_SCHEDD_PERM:
		return "ADVERTISE_SCHEDD";
	case ADVERTISE_MASTER_PERM:
		return "ADVERTISE_MASTER";
	default:
		return "Unknown";
	}
};


DCpermission
getPermissionFromString( const char * permstring )
{
	for ( DCpermission i = FIRST_PERM; i < LAST_PERM; i = NEXT_PERM(i) ) {
		if (strcasecmp(permstring, PermString(i)) == 0) {
			// match
			return i;
		}
	}

	return (DCpermission)-1;
}


DCpermissionHierarchy::
DCpermissionHierarchy(DCpermission perm) {
	m_base_perm = perm;
	unsigned int i = 0;

	m_implied_perms[i++] = m_base_perm;

		// Add auth levels implied by specified perm
	bool done = false;
	while(!done) {
		switch( m_implied_perms[i-1] ) {
		case DAEMON:
		case ADMINISTRATOR:
			m_implied_perms[i++] = WRITE;
			break;
		case WRITE:
		case NEGOTIATOR:
		case CONFIG_PERM:
		case ADVERTISE_STARTD_PERM:
		case ADVERTISE_SCHEDD_PERM:
		case ADVERTISE_MASTER_PERM:
			m_implied_perms[i++] = READ;
			break;
		default:
				// end of hierarchy
			done = true;
			break;
		}
	}
	m_implied_perms[i] = LAST_PERM;

	i=0;
	switch(m_base_perm) {
	case READ:
		m_directly_implied_by_perms[i++] = WRITE;
		m_directly_implied_by_perms[i++] = NEGOTIATOR;
		m_directly_implied_by_perms[i++] = CONFIG_PERM;
		m_directly_implied_by_perms[i++] = ADVERTISE_STARTD_PERM;
		m_directly_implied_by_perms[i++] = ADVERTISE_SCHEDD_PERM;
		m_directly_implied_by_perms[i++] = ADVERTISE_MASTER_PERM;
		break;
	case WRITE:
		m_directly_implied_by_perms[i++] = ADMINISTRATOR;
		m_directly_implied_by_perms[i++] = DAEMON;
		break;
	default:
		break;
	}
	m_directly_implied_by_perms[i] = LAST_PERM;

	i=0;
	m_config_perms[i++] = m_base_perm;
	done = false;
	while( !done ) {
		switch(m_config_perms[i-1]) {
		case DAEMON:
			if (param_boolean("LEGACY_ALLOW_SEMANTICS", false)) {
				m_config_perms[i++] = WRITE;
			} else {
				done = true;
			}
			break;
		case ADVERTISE_STARTD_PERM:
		case ADVERTISE_SCHEDD_PERM:
		case ADVERTISE_MASTER_PERM:
			m_config_perms[i++] = DAEMON;
			break;
		default:
				// end of config hierarchy
			done = true;
			break;
		}
	}
	m_config_perms[i++] = DEFAULT_PERM;
	m_config_perms[i] = LAST_PERM;
}
