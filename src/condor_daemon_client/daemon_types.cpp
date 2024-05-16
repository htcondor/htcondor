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
#include "daemon_types.h"
#include "condor_adtypes.h"
#include "stl_string_utils.h"
#include <array>
#include <map>
#include <algorithm>

struct _adtype_and_daemon_type { AdTypes at; daemon_t dt; };

 // Map of MyType of an ad to what daemon produces it
 // Primary user is python bindings code to initialize the Daemon class
 // But it might be generally useful.
 // This returns return a std::array at compile time that other
 // consteval functions can use as a lookup table
constexpr 
std::array<std::pair<const char *, _adtype_and_daemon_type>,23>
makeAdTypeToDaemonTable() {
	return {{ // yes, there needs to be 2 open braces here...
		{ ANY_ADTYPE,			{          ANY_AD, DT_ANY } },
		{ STARTD_SLOT_ADTYPE,	{         SLOT_AD, DT_STARTD } },
		{ STARTD_DAEMON_ADTYPE, {  STARTDAEMON_AD, DT_STARTD } },
		{ STARTD_OLD_ADTYPE,	{       STARTD_AD, DT_STARTD } },
		{ SCHEDD_ADTYPE,		{       SCHEDD_AD, DT_SCHEDD } },
		{ SUBMITTER_ADTYPE,		{    SUBMITTOR_AD, DT_SCHEDD } },
		{ MASTER_ADTYPE,		{       MASTER_AD, DT_MASTER } },
		{ COLLECTOR_ADTYPE,		{    COLLECTOR_AD, DT_COLLECTOR } },
		{ NEGOTIATOR_ADTYPE,	{   NEGOTIATOR_AD, DT_NEGOTIATOR } },
		{ ACCOUNTING_ADTYPE,	{   ACCOUNTING_AD, DT_NEGOTIATOR } },
		{ HAD_ADTYPE,			{          HAD_AD, DT_HAD } },
		{ REPLICATION_ADTYPE,	{      GENERIC_AD, DT_GENERIC } },	// Replocation ads go into the generic table in the collector
		{ CLUSTER_ADTYPE,		{      CLUSTER_AD, DT_CLUSTER } },
		{ GENERIC_ADTYPE,		{      GENERIC_AD, DT_GENERIC } },
		{ CREDD_ADTYPE,			{        CREDD_AD, DT_CREDD } },
		{ XFER_SERVICE_ADTYPE,	{ XFER_SERVICE_AD, DT_TRANSFERD } },
		{ LEASE_MANAGER_ADTYPE,	{LEASE_MANAGER_AD, DT_LEASE_MANAGER } },
		{ GRID_ADTYPE,			{         GRID_AD, DT_GRIDMANAGER } },
		{ DEFRAG_ADTYPE,		{       DEFRAG_AD, DT_GENERIC } },   // Defrag ads go into the generic table in the collector
		{ JOB_ROUTER_ADTYPE,	{      GENERIC_AD, DT_GENERIC } },	// Job_Router ads go into the generic table in the collector
		{ JOB_ADTYPE,			{           NO_AD, DT_SCHEDD } },	// should we have an AdTypes entry for jobs ?
		{ JOB_SET_ADTYPE,		{           NO_AD, DT_SCHEDD } },	// should we have an AdTypes entry for jobs ?
		{ OWNER_ADTYPE,			{           NO_AD, DT_SCHEDD } },	// should we have an AdTypes entry for jobs ?

		}};
}

template<size_t N> constexpr
auto sortByFirst(const std::array<std::pair<const char *, _adtype_and_daemon_type>, N> &table) {
	auto sorted = table;
	std::sort(sorted.begin(), sorted.end(),
		[](const std::pair<const char *, _adtype_and_daemon_type> &lhs,
			const std::pair<const char *, _adtype_and_daemon_type> &rhs) {
				return istring_view(lhs.first) < istring_view(rhs.first);
		});
	return sorted;
}

daemon_t
AdTypeStringToDaemonType(const char* adtype_string)
{
	constexpr static const auto table = sortByFirst(makeAdTypeToDaemonTable());
	auto it = std::lower_bound(table.begin(), table.end(), adtype_string,
		[](const std::pair<const char *, _adtype_and_daemon_type> &p, const char * name) {
			return istring_view(p.first) < istring_view(name);
		});;
	if ((it != table.end()) && (istring_view(it->first) == istring_view(adtype_string))) return it->second.dt;
	return DT_NONE;
}

AdTypes
AdTypeStringToAdType(const char* adtype_string)
{
	constexpr static const auto table = sortByFirst(makeAdTypeToDaemonTable());
	auto it = std::lower_bound(table.begin(), table.end(), adtype_string,
		[](const std::pair<const char *, _adtype_and_daemon_type> &p, const char * name) {
			return istring_view(p.first) < istring_view(name);
		});;
	if ((it != table.end()) && (istring_view(it->first) == istring_view(adtype_string))) return it->second.at;
	return NO_AD;
}

static const char* daemon_names[] = {
	"none",
	"any",
	"master",
	"schedd",
	"startd",
	"collector",
	"negotiator",
	"kbdd",
	"dagman", 
	"view_collector",
	"cluster_server",
	"shadow",
	"starter",
	"credd",
	"gridmanager",
	"transferd",
	"lease_manager",
	"had",
	"generic"
};

const char*
daemonString( daemon_t dt )
{	
	if( dt >= DT_NONE && dt < _dt_threshold_ ) {
		return daemon_names[dt];
	} else {
		return "Unknown";
	}
}

daemon_t
stringToDaemonType( const char* name )
{
	int i;
	for( i=0; i<_dt_threshold_; i++ ) {
		if( !strcasecmp(daemon_names[i], name) ) {
			return (daemon_t)i;
		}
	}
	return DT_NONE;
}


bool
convert_daemon_type_to_ad_type(daemon_t daemon_type, AdTypes & ad_type) {
	switch(daemon_type) {
		case DT_MASTER:
			ad_type = MASTER_AD;
			return true;
		case DT_STARTD:
			ad_type = STARTD_AD;
			return true;
		case DT_SCHEDD:
			ad_type = SCHEDD_AD;
			return true;
		case DT_NEGOTIATOR:
			ad_type = NEGOTIATOR_AD;
			return true;
		case DT_COLLECTOR:
			ad_type = COLLECTOR_AD;
			return true;
		case DT_GENERIC:
			ad_type = GENERIC_AD;
			return true;
		case DT_HAD:
			ad_type = HAD_AD;
			return true;
		case DT_CREDD:
			ad_type = CREDD_AD;
			return true;
		default:
			return false;
	}
}
