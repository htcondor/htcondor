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
#include "condor_commands.h"
#include "command_strings.h"
#include "condor_perms.h"
#include "stl_string_utils.h"
#include <map>
#include <algorithm>

template<size_t N> constexpr
 auto sortByFirst(const std::array<std::pair<int, const char *>, N> &table) {
	auto sorted = table;
	std::sort(sorted.begin(), sorted.end(),
			[](const std::pair<int, const char *> &lhs,
			   const std::pair<int, const char *> &rhs) {
				return lhs.first < rhs.first;
			});
	return sorted;
}

template<size_t N> constexpr
auto sortBySecond(const std::array<std::pair<int, const char *>, N> &table) {
	std::array sorted = table;
	std::sort(sorted.begin(), sorted.end(),
			[](const std::pair<int, const char *> &lhs,
			   const std::pair<int, const char *> &rhs) {
				return istring_view(lhs.second) < istring_view(rhs.second);
			});
	return sorted;
}

const char * getCollectorCommandString(int id)
{
	constexpr static const auto table = sortByFirst(makeCollectorCommandTable());
	auto it = std::lower_bound(table.begin(), table.end(), id,
			[](const std::pair<int, const char *> &p, int e) {
				return p.first < e;
			});;
	if ((it != table.end()) && (it->first == id)) return it->second;
	return nullptr;
}

const char * getCommandString(int id)
{
	const char *p = getCollectorCommandString(id);
	if (p) return p;

	constexpr static const auto table = sortByFirst(makeCommandTable());
	auto it = std::lower_bound(table.begin(), table.end(), id,
			[](const std::pair<int, const char *> &p, int e) {
				return p.first < e;
			});;
	if ((it != table.end()) && (it->first == id)) return it->second;
	return nullptr;
}

int getCommandNum(const char * name)
{
	int r = getCollectorCommandNum(name);
	if (r > -1) return r;

	constexpr static const auto table = sortBySecond(makeCommandTable());
	auto it = std::lower_bound(table.begin(), table.end(), name,
			[](const std::pair<int, const char *> &p, const char *name) {
				return istring_view(p.second) < istring_view(name);
			});;
	if ((it != table.end()) && (istring_view(name) == istring_view(it->second))) return it->first;
	return -1;
}

int getCollectorCommandNum(const char * name)
{
	constexpr static const auto table = sortBySecond(makeCollectorCommandTable());
	auto it = std::lower_bound(table.begin(), table.end(), name,
			[](const std::pair<int, const char *> &p, const char *name) {
				return istring_view(p.second) < istring_view(name);
			});;
	if ((it != table.end()) && (istring_view(name) == istring_view(it->second))) return it->first;
	return -1;
}

// return the command name for known commmand or "command NNN" for unknown commands
// returned pointer is valid forever and the caller does not free it
const char*
getCommandStringSafe( int num )
{
	char const *result = getCommandString(num);
	if (result)
		return result;

	return getUnknownCommandString(num);
}

// return a string pointer representation of an unknown command id
// that can be cached by the caller for the lifetime of the process.
const char*
getUnknownCommandString(int num)
{
	static std::map<int, const char*> * pcmds = NULL;
	if ( ! pcmds) {
		pcmds = new std::map<int, const char*>();
		if ( ! pcmds) return "malloc-fail!";
	}

	std::map<int, const char*>::iterator it = pcmds->find(num);
	if (it != pcmds->end()) {
		return it->second;
	}

	static const char fmt[] = "command %u";
	size_t pstr_sz = sizeof(fmt)+8;
	char * pstr = (char*)malloc(pstr_sz); // max int string is 10 bytes (-2 for %d)
	if ( ! pstr) return "malloc-fail!";
	snprintf(pstr, pstr_sz, fmt, num);
	(*pcmds)[num] = pstr;
	return pstr;
}

int getSampleCommand( int authz_level ) {
        switch(authz_level) {
                case ALLOW:
                        return DC_NOP;
                case READ:
                        return DC_NOP_READ;
                case WRITE:
                        return DC_NOP_WRITE;
                case NEGOTIATOR:
                        return DC_NOP_NEGOTIATOR;
                case ADMINISTRATOR:
                        return DC_NOP_ADMINISTRATOR;
                case CONFIG_PERM:
                        return DC_NOP_CONFIG;
                case DAEMON:
                        return DC_NOP_DAEMON;
                case ADVERTISE_STARTD_PERM:
                        return DC_NOP_ADVERTISE_STARTD;
                case ADVERTISE_SCHEDD_PERM:
                        return DC_NOP_ADVERTISE_SCHEDD;
                case ADVERTISE_MASTER_PERM:
                        return DC_NOP_ADVERTISE_MASTER;
        }
        return -1;
}

