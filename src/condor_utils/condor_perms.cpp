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
#include "stl_string_utils.h"
#include <map>
#include <array>
#include <algorithm>

constexpr const
std::array<std::pair<DCpermission, const char *>, LAST_PERM> makePermStringTable() {
	return {{ // Yes, we need two...
			/** Open to everyone */                    {ALLOW,                 "ALLOW"},
			/** Able to read data */                   {READ,                  "READ"},
			/** Able to modify data (submit jobs) */   {WRITE,                 "WRITE"},
			/** From the negotiator */                 {NEGOTIATOR,            "NEGOTIATOR"},
			/** Administrative cmds (on, off, etc) */  {ADMINISTRATOR,         "ADMINISTRATOR"},
			/** Changing config settings remotely */   {CONFIG_PERM,           "CONFIG"},
			/** Daemon to daemon communcation     */   {DAEMON,                "DAEMON"},
			/** SOAP interface (http PUT) */           {SOAP_PERM,             "SOAP"},
			/** DEFAULT */                             {DEFAULT_PERM,          "DEFAULT"},
			/** CLIENT */                              {CLIENT_PERM,           "CLIENT"},
			/** startd ad */                           {ADVERTISE_STARTD_PERM, "ADVERTISE_STARTD"},
			/** schedd ad */                           {ADVERTISE_SCHEDD_PERM, "ADVERTISE_SCHEDD"},
			/** master ad */                           {ADVERTISE_MASTER_PERM, "ADVERTISE_MASTER"},
		}};
}

template<size_t N> constexpr
auto sortByFirst(const std::array<std::pair<DCpermission, const char *>, N> &table) {
	auto sorted = table;
	std::sort(sorted.begin(), sorted.end(),
		[](const std::pair<DCpermission, const char *> &lhs,
			const std::pair<DCpermission, const char *> &rhs) {
				return lhs.first < rhs.first;
		});
	return sorted;
}

template<size_t N> constexpr
auto sortBySecond(const std::array<std::pair<DCpermission, const char *>, N> &table) {
	std::array sorted = table;
	std::sort(sorted.begin(), sorted.end(),
		[](const std::pair<DCpermission, const char *> &lhs,
			const std::pair<DCpermission, const char *> &rhs) {
				return istring_view(lhs.second) < istring_view(rhs.second);
		});
	return sorted;
}


const char * PermString(DCpermission perm)
{
	constexpr static const auto table = sortByFirst(makePermStringTable());
	if (perm < 0 || (size_t)perm >= table.size()) {
		return nullptr;
	}
	ASSERT(table[perm].first == perm);
	return table[perm].second;
}

DCpermission
getPermissionFromString( const char * name )
{
	constexpr static const auto table = sortBySecond(makePermStringTable());
	auto it = std::lower_bound(table.begin(), table.end(), name,
		[](const std::pair<int, const char *> &p, const char *name) {
			return istring_view(p.second) < istring_view(name);
		});;
	if ((it != table.end()) && (istring_view(name) == istring_view(it->second))) return (DCpermission)it->first;
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
