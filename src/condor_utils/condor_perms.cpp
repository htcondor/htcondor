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
			{ALLOW,                 "ALLOW\0Open to everyone"},
			{READ,                  "READ\0Able to read data"},
			{WRITE,                 "WRITE\0Able to modify data (submit jobs)"},
			{NEGOTIATOR,            "NEGOTIATOR\0From the negotiator"},
			{ADMINISTRATOR,         "ADMINISTRATOR\0Administrative cmds (on, off, etc)"},
			{CONFIG_PERM,           "CONFIG\0Changing config settings remotely"},
			{DAEMON,                "DAEMON\0Daemon to daemon communcation"},
			{SOAP_PERM,             "SOAP\0(deprecated) SOAP interface (http PUT)"},
			{DEFAULT_PERM,          "DEFAULT\0Default config settings"},
			{CLIENT_PERM,           "CLIENT\0Tools and client-side daemons"},
			{ADVERTISE_STARTD_PERM, "ADVERTISE_STARTD\0Able to advertise a STARTD ad"},
			{ADVERTISE_SCHEDD_PERM, "ADVERTISE_SCHEDD\0Able to advertise a SCHEDD ad"},
			{ADVERTISE_MASTER_PERM, "ADVERTISE_MASTER\0Able to advertise a MASTER ad"},
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

const char * PermDescription(DCpermission perm)
{
	constexpr static const auto table = sortByFirst(makePermStringTable());
	if (perm < 0 || (size_t)perm >= table.size()) {
		return nullptr;
	}
	ASSERT(table[perm].first == perm);
	return table[perm].second + strlen(table[perm].second)+1;
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
	return NOT_A_PERM;
}

// tables for iterating DCpermissionHierarchy::nextConfig
// each entry should have the value that will iterate next
// all paths should lead to LAST_PERM
// i.e.   aConfigNext[DAEMON] -> aConfigNext[DEFAULT] -> aConfigNext[LAST]
// the legacy table is the config order we used before recommended_v90 security
const DCpermission DCpermissionHierarchy::aConfigNext[] = {
	/*                ALLOW */ DEFAULT_PERM,
	/*                 READ */ DEFAULT_PERM,
	/*                WRITE */ DEFAULT_PERM,
	/*           NEGOTIATOR */ DEFAULT_PERM,
	/*        ADMINISTRATOR */ DEFAULT_PERM,
	/*               CONFIG */ DEFAULT_PERM,
	/*               DAEMON */ DEFAULT_PERM,
	/*                 SOAP */ DEFAULT_PERM,
	/*              DEFAULT */ LAST_PERM,
	/*               CLIENT */ DEFAULT_PERM,
	/*     ADVERTISE_STARTD */ DAEMON,
	/*     ADVERTISE_SCHEDD */ DAEMON,
	/*     ADVERTISE_MASTER */ DAEMON,
	/*     LAST_PERM        */ UNSET_PERM,
};
const DCpermission DCpermissionHierarchy::aConfigNextLegacy[] = {
	/*                ALLOW */ DEFAULT_PERM,
	/*                 READ */ DEFAULT_PERM,
	/*                WRITE */ DEFAULT_PERM,
	/*           NEGOTIATOR */ DEFAULT_PERM,
	/*        ADMINISTRATOR */ DEFAULT_PERM,
	/*               CONFIG */ DEFAULT_PERM,
	/*               DAEMON */ WRITE,
	/*                 SOAP */ DEFAULT_PERM,
	/*              DEFAULT */ LAST_PERM,
	/*               CLIENT */ DEFAULT_PERM,
	/*     ADVERTISE_STARTD */ DAEMON,
	/*     ADVERTISE_SCHEDD */ DAEMON,
	/*     ADVERTISE_MASTER */ DAEMON,
	/*     LAST_PERM        */ UNSET_PERM,
};

// table for iterating permissions implied by a given permission.
// all paths should lead to LAST_PERM
// aImpliedNext[DAEMON] -> aImpliedNext[WRITE] -> aImpliedNext[READ] -> aImpliedNext[LAST]
const DCpermission DCpermissionHierarchy::aImpliedNext[] = {
	/*                ALLOW */ LAST_PERM,
	/*                 READ */ LAST_PERM,
	/*                WRITE */ READ,
	/*           NEGOTIATOR */ READ,
	/*        ADMINISTRATOR */ WRITE,
	/*               CONFIG */ READ,
	/*               DAEMON */ WRITE,
	/*                 SOAP */ LAST_PERM,
	/*              DEFAULT */ LAST_PERM,
	/*               CLIENT */ LAST_PERM,
	/*     ADVERTISE_STARTD */ READ,
	/*     ADVERTISE_SCHEDD */ READ,
	/*     ADVERTISE_MASTER */ READ,
	/*     LAST_PERM        */ UNSET_PERM,
};
