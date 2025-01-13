/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "misc_utils.h"
#include "condor_config.h"
#include "ResAttributes.h"
#include "condor_daemon_core.h"
#include "slot_builder.h"
#include "../condor_sysapi/sysapi.h"

bool string_to_BuildSlotFailureMode(const char * str, enum BuildSlotFailureMode & failmode)
{
	if (MATCH == strcasecmp("CLEAR", str)) { failmode = BuildSlotFailureMode::AllOrNothing; }
	else if (MATCH == strcasecmp("CONTINUE", str)) { failmode = BuildSlotFailureMode::BestEffort; }
	else if (MATCH == strcasecmp("ABORT", str)) { failmode = BuildSlotFailureMode::Except; }
	else { return false; }
	return true;
}

const char * BuildSlotFailureMode_to_string(BuildSlotFailureMode failmode)
{
	switch (failmode) {
	case BuildSlotFailureMode::AllOrNothing: return "CLEAR";
	case BuildSlotFailureMode::BestEffort: return "CONTINUE";
	case BuildSlotFailureMode::Except: return "ABORT";
	}
	return nullptr;
}

// ugly, but we can't include util.h without some refactoring, so...
bool	check_execute_dir_perms(char const *exec_path, bool abort_on_error);
extern bool execute_dir_checks_out; // set in startd_main for EXECUTE

static void
_checkInvalidParam( const char* name, bool except ) {
	char* tmp;
	if ((tmp = param(name))) {
		if (except) {
			EXCEPT( "Can't define %s in the config file", name );
		} else {
			dprintf( D_ALWAYS,
					 "Can't define %s in the config file, ignoring\n",
					 name );
		}
		free(tmp);
	}
}

// param for the execute dir and check to see if it exists, returns empty string on success
// and error string on failure
std::string GetConfigExecuteDir( int slot_id, std::string &execute_dir, std::string &partition_id )
{
	std::string ret;

	std::string slot_param;
	formatstr(slot_param,"SLOT%d_EXECUTE",slot_id);
	const char * execute_param = slot_param.c_str();
	auto_free_ptr execute_value(param(execute_param));
	if ( ! execute_value) {
		execute_param = "EXECUTE";
		execute_value.set(param("EXECUTE"));
	}
	if ( ! execute_value) {
		formatstr(ret, "EXECUTE (or %s) is not defined in the configuration.", slot_param.c_str());
		return ret;
	}

#if defined(WIN32)
	// switch delimiter char in exec path on WIN32
	for (char * ptr = execute_value.ptr(); *ptr; ++ptr) {
		if (*ptr == '/') { *ptr = '\\'; }
	}
#endif

	execute_dir = execute_value.ptr();
	dprintf(D_FULLDEBUG, "Got execute_dir = %s\n",execute_dir.c_str());

		// Get a unique identifier for the partition containing the execute dir
	auto_free_ptr partition_value;
	char * ptr = nullptr;
	bool partition_rc = sysapi_partition_id( execute_dir.c_str(), &ptr );
	partition_value.set(ptr);

	if( !partition_rc) {
		struct stat statbuf;
		if( stat(execute_dir.c_str(), &statbuf)!=0 ) {
			int stat_errno = errno;
			formatstr(ret, "Error accessing execute directory %s specified in the configuration setting %s: (errno=%d) %s",
				   execute_dir.c_str(), execute_param, stat_errno, strerror(stat_errno) );
			return ret;
		}
		formatstr(ret, "Failed to get partition id for %s=%s", execute_param, execute_dir.c_str());
		return ret;
	}

	if ( ! check_execute_dir_perms(execute_dir.c_str(), false)) {
		formatstr(ret, "Execute directory %s is invalid", execute_dir.c_str());
	}

	ASSERT( partition_value );
	partition_id = partition_value.ptr();
	return ret;
}

// called by ResMgr::init_resources during startup to build the initial slots
// 
CpuAttributes** buildCpuAttrs(
	MachAttributes *m_attr,
	int max_types,
	const std::vector<std::string>& type_strings,
	int total,
	int* type_num_array,
	bool *backfill_types,
	BuildSlotFailureMode failmode,
	std::vector<bool> & fail_types)
{
	int num;
	CpuAttributes* cap;
	CpuAttributes** cap_array;
	std::string logbuf; logbuf.reserve(200);  // buffer use for reporting resource totals

		// Available system resources.
	AvailAttributes avail( m_attr );
	AvailAttributes bkavail( m_attr );

	int d_except = (failmode == BuildSlotFailureMode::Except) ? D_EXCEPT : 0;

	cap_array = new CpuAttributes* [total];
	if( ! cap_array ) {
		EXCEPT( "Out of memory!" );
	}

	num = 0;
	for (int type_id = 0; type_id < max_types; ++type_id) {
		if (num >= total) break;

		for (int j=0; j < type_num_array[type_id]; j++) {
			if (num >= total) break;
			std::string execute_dir, partition_id;
			CpuAttributes::_slot_request request;
			if ( ! CpuAttributes::buildSlotRequest(request, m_attr, type_strings[type_id], type_id, failmode)) {
				// the failure has already been logged.
				fail_types[type_id] = true;
				if (failmode == BuildSlotFailureMode::AllOrNothing) {
					// Gracefully cleanup and abort
					for(int ii=0; ii<num; ii++ ) { delete cap_array[ii]; }
					delete [] cap_array;
					return NULL;
				}
				continue; // mode is not except or all-or-nothing so keep going.

			}
			std::string edir_errstr = GetConfigExecuteDir(num+1, execute_dir, partition_id);
			if ( ! edir_errstr.empty()) {
				if (failmode == BuildSlotFailureMode::Except) {
					EXCEPT("%s",edir_errstr.c_str());
				}
				dprintf(D_ERROR, "%s\n", edir_errstr.c_str());

				if (failmode == BuildSlotFailureMode::AllOrNothing) {
					for (int ii=0; ii<num; ++ii) { delete cap_array[ii]; }
					delete [] cap_array;
					fail_types[type_id] = true;
					return NULL;
				}
			}
			cap = new CpuAttributes(type_id, request, execute_dir, partition_id);
			bool fits = false;
			if (backfill_types[type_id]) {
				fits = bkavail.decrement(cap);
			} else {
				fits = avail.decrement(cap);
			}
			if (fits) {
				if ( ! edir_errstr.empty()) {
					logbuf = edir_errstr;
					cap->set_broken(BROKEN_CODE_NO_EXEC_DIR, logbuf);
				}
				cap_array[num] = cap;
				num++;
			} else {
				// We ran out of system resources.
				logbuf =    "\tRequesting: ";
				cap->cat_totals(logbuf);
				logbuf += "\n\tAvailable:  ";
				avail.cat_totals(logbuf, cap->executePartitionID());
				dprintf( D_ERROR | d_except,
							"ERROR: Can't allocate %s slot of type %d\n%s\n",
							num_string(j+1), type_id, logbuf.c_str() );
				if (failmode == BuildSlotFailureMode::Except) {
					EXCEPT( "Ran out of system resources" );
				} else if (failmode == BuildSlotFailureMode::AllOrNothing) {
					delete cap;	// This isn't in our array yet.
					// Gracefully cleanup and abort
					for (int ii=0; ii<num; ++ii) { delete cap_array[ii]; }
					delete [] cap_array;
					fail_types[type_id] = true;
					return NULL;
				} else if (j==0) {
					// for first slot of this type, make a partial slot
					delete cap;
					logbuf.clear();
					logbuf = "Not enough ";
					if (backfill_types[type_id]) {
						logbuf += "backfill ";
						bkavail.trim_request_to_fit(request, partition_id.c_str(), logbuf);
						cap = new CpuAttributes(type_id, request, execute_dir, partition_id);
						fits = bkavail.decrement(cap);
					} else {
						avail.trim_request_to_fit(request, partition_id.c_str(), logbuf);
						cap = new CpuAttributes(type_id, request, execute_dir, partition_id);
						fits = avail.decrement(cap);
					}

					if ( ! fits) {
						dprintf(D_ERROR, "ERROR: Can't allocate reduced slot of type %d\n", type_id);
						fail_types[type_id] = true;
						continue;
					}

					trim(logbuf);
					cap->set_broken(BROKEN_CODE_NO_RES, logbuf);
					cap_array[num] = cap;
					num++;
				} else {
					delete cap; // not the first slot of this type, just don't make it
					fail_types[type_id] = true;
				}
			}
		}
	}

		// gather remaining resources and counts of slots desiring auto shares
		// of user-defined resources into remain_cap, and remain_cnt.  these
		// will be use to distribute auto shares in the loop below.
	AvailAttributes::slotres_map_t remain_cap, remain_cnt, bkremain_cap, bkremain_cnt;
	avail.computeRemainder(remain_cap, remain_cnt);
	bkavail.computeRemainder(bkremain_cap, bkremain_cnt);

	// to make the output easier to read, only report the unallocated cap for each slot type once.
	const int d_level = D_ALWAYS;
	unsigned long report_auto = (unsigned long)-1;

		// now replace "auto" shares with final value
	for (int i=0; i<num; i++) {
		cap = cap_array[i];
		unsigned long bit = 1 << cap->type_id();
		if (report_auto & bit) {
			if (IsDebugLevel(d_level)) {
				logbuf.clear();
				cap->cat_totals(logbuf);
				dprintf(d_level, "Allocating auto shares for slot type %d: %s\n", cap->type_id(), logbuf.c_str());
			}
			report_auto &= ~bit;
		}
		bool backfill = backfill_types[cap->type_id()];
		bool fits;
		if (backfill) {
			fits = bkavail.computeAutoShares(cap, bkremain_cap, bkremain_cnt);
		} else {
			fits = avail.computeAutoShares(cap, remain_cap, remain_cnt);
		}
		if( !fits ) {

			// We ran out of system resources.
			logbuf =    "\tRequesting: ";
			cap->cat_totals(logbuf);
			logbuf += "\n\tAvailable:  ";
			if (backfill) {
				bkavail.cat_totals(logbuf, cap->executePartitionID());
			} else {
				avail.cat_totals(logbuf, cap->executePartitionID());
			}

			dprintf(D_ERROR,
					"ERROR: Can't allocate slot id %d (slot type %d) during auto allocation of resources\n%s\n",
					i+1, cap->type_id(), logbuf.c_str() );

			if (failmode == BuildSlotFailureMode::Except) {
				EXCEPT( "Ran out of system resources in auto allocation" );
			} else if (failmode == BuildSlotFailureMode::AllOrNothing) {
					// Gracefully cleanup and abort
				for (int j=0;  j<num;  j++) {
					delete cap_array[j];
				}
				fail_types[cap->type_id()] = true;
				delete [] cap_array;
				return NULL;
			} else {
				// this may overwrite an existing set_broken() value, but that's ok for now.
				cap->set_broken(BROKEN_CODE_NO_RES, logbuf);
				continue;
			}
		}

		if (IsDebugLevel(d_level)) {
			logbuf.clear();
			cap->cat_totals(logbuf);
			dprintf(d_level, "  slot type %d: %s\n", cap->type_id(), logbuf.c_str());
		}
	}

	for (int i=0; i<num; i++) {
		bool backfill = backfill_types[cap_array[i]->type_id()];
		if ( ! cap_array[i]->bind_DevIds(m_attr, i+1, 0, backfill, failmode == BuildSlotFailureMode::Except)) {
			// if we get here, bind_DevIds should have marked the slot as broken
			ASSERT(cap_array[i]->is_broken());
		}
	}

	if (num == 0) {
		delete [] cap_array;
		cap_array = nullptr;
	} else {
		for ( ; num < total; ++num) { cap_array[num] = nullptr; }
	}

	return cap_array;
}

void initTypes( int max_types, std::vector<std::string>& type_strings, BuildSlotFailureMode failmode, std::vector<bool>& fail_types )
{
	int i;
	std::string buf;

	_checkInvalidParam("SLOT_TYPE_0", failmode == BuildSlotFailureMode::Except);
		// CRUFT
	_checkInvalidParam("VIRTUAL_MACHINE_TYPE_0", failmode == BuildSlotFailureMode::Except);

		// Type 0 is the special type for evenly divided slots.
	type_strings[0] = "auto";
	fail_types[0] = false;

	for( i=1; i < max_types; i++ ) {
		type_strings[i].clear();
		fail_types[i] = false;
		formatstr(buf, "SLOT_TYPE_%d", i);
		param(type_strings[i], buf.c_str());
	}
}


int countTypes( int max_types, int num_cpus, int** array_ptr, bool** bkfill_ptr, BuildSlotFailureMode failmode )
{
	int i, num=0, num_set=0;
	std::string param_name;
	int* my_type_nums = new int[max_types];
	bool* my_bkfill_bools = new bool[max_types];

	if( ! array_ptr ) {
		EXCEPT( "ResMgr:countTypes() called with NULL array_ptr!" );
	}

		// Type 0 is special, user's shouldn't define it.
	_checkInvalidParam("NUM_SLOTS_TYPE_0", failmode == BuildSlotFailureMode::Except);

	for( i=1; i<max_types; i++ ) {
		formatstr(param_name, "NUM_SLOTS_TYPE_%d", i);
		my_type_nums[i] = param_integer(param_name.c_str(), 0);
		if (my_type_nums[i]) {
			num_set = 1;
			num += my_type_nums[i];
		}
		formatstr(param_name, "SLOT_TYPE_%d_BACKFILL", i);
		my_bkfill_bools[i] = param_boolean(param_name.c_str(), false);
	}

	if( num_set ) {
			// We found type-specific stuff, use that and set the number of type 0 slots to 0
		my_type_nums[0] = 0;
		my_bkfill_bools[0] = false;
	} else {
			// We haven't found any special types yet.  Therefore,
			// we're evenly dividing things, so we only have to figure
			// out how many nodes to advertise.  If the type0 slot is partitionable
			// we make 1 p-slot, otherwise we make as many slots as there are cpus
		bool pslot = param_boolean("SLOT_TYPE_0_PARTITIONABLE", true);
		my_type_nums[0] = param_integer("NUM_SLOTS", pslot ? 1 : num_cpus);
		num = my_type_nums[0];
		my_bkfill_bools[0] = param_boolean("SLOT_TYPE_0_BACKFILL", false);
	}
	*array_ptr = my_type_nums;
	*bkfill_ptr = my_bkfill_bools;
	return num;
}

const double MINIMAL_SHARE = -1.0/128;
typedef enum { SPECIAL_SHARE_NONE=0, SPECIAL_SHARE_AUTO=1, SPECIAL_SHARE_MINIMAL=2 } special_share_t;

/*
   Parse a string in one of a few formats, and return a float that
   represents the value.  Either it's "auto", "minimal" or it's a fraction, like
   "1/4", or it's a percent, like "25%" (these are equivalent),
   or it's a regular value, like "64"
   auto and fraction/percent are returned as negative values
*/
static double parse_share_value(const char* str, int type, BuildSlotFailureMode failmode, special_share_t & special)
{
	if( strcasecmp(str,"auto") == 0 || strcasecmp(str,"automatic") == 0 ) {
		special = SPECIAL_SHARE_AUTO;
		return AUTO_SHARE;
	}
	if (strcasecmp(str,"min") == 0 || strcasecmp(str,"minimal") == 0) {
		special = SPECIAL_SHARE_MINIMAL;
		return MINIMAL_SHARE;
	}
	special = SPECIAL_SHARE_NONE;

	const char * tmp = strchr(str, '%');
	if (tmp) {
			// It's a percent
		return -(atof(str) / 100.0);
	}

	tmp = strchr(str, '/');
	if (tmp) {
			// It's a ratio (i.e. a fraction)
		double denom = tmp[1] ? atof(&tmp[1]) : 0;
		if (denom <= 0) {
			dprintf( D_ALWAYS, "Can't parse attribute \"%s\" in description of slot type %d\n",
					 str, type );
			if (failmode == BuildSlotFailureMode::Except) {
				DC_Exit( 4 );
			} else {
				return 0;
			}
		}
		return -(atof(str) / denom);
	}

		// This must just be an absolute value.  Return it as a negative
		// value, so the caller knows it's not a fraction.
	return atof(str);
}


/*
  convert share value into a quantity if possible, positive values are
  absolute quantities, they are truncated to an int, but otherwise pass
  through this function.  Negative values should be between -1.0 and 0.0
  and are interpreted as a proportion of the total.

  a CPU.  However, we never want to advertise less than 1.
  Negative values between 0 and -1 are a fraction of the total
  Positive values are absolute values.
*/
template <class t>
static t compute_local_resource(t num_res, double share, t min_res = 0)
{
	if (IS_AUTO_SHARE(share)) return AUTO_RES;
	t res = (share < 0) ? t(-share * num_res) : t(share);
	return MAX(res, min_res);
}

const int UNSET_SHARE = -9998;
#define IS_UNSET_SHARE(share) ((int)share == UNSET_SHARE)

/*static*/ bool CpuAttributes::buildSlotRequest(
	_slot_request & request,
	MachAttributes *m_attr,
	const std::string& list,
	unsigned int type_id,
	BuildSlotFailureMode failmode)
{
	typedef CpuAttributes::slotres_map_t slotres_map_t;
	special_share_t default_share_special = SPECIAL_SHARE_AUTO;
	double default_share = AUTO_SHARE;

	if ( list.empty()) {
		// give everything the default share and return

		request.num_cpus = compute_local_resource(m_attr->num_cpus(), default_share, 1.0);
		request.num_phys_mem = compute_local_resource(m_attr->phys_mem(), default_share, 1);
		request.virt_mem_fraction = AUTO_SHARE;
		request.disk_fraction = AUTO_SHARE;

		for (slotres_map_t::const_iterator j(m_attr->machres().begin());  j != m_attr->machres().end();  ++j) {
			request.slotres[j->first] = default_share;
		}

		return true;
	}

	double cpus = UNSET_SHARE;
	int ram = UNSET_SHARE; // this is in MB so an int is enough
	double disk_fraction = UNSET_SHARE;
	double swap_fraction = UNSET_SHARE;

	// For this parsing code, deal with the following example
	// string list:
	// "c=1, r=25%, d=1/4, s=25%"
	// There may be a bare number (no equals sign) that specifies
	// the default share for any items not explicitly defined.  Example:
	// "c=1, 25%"

	for (slotres_map_t::const_iterator j(m_attr->machres().begin());  j != m_attr->machres().end();  ++j) {
		request.slotres[j->first] = UNSET_SHARE;
	}

	// In a multi-line value, '\n' is the only delimiter
	const char* delim = strchr(list.c_str(), '\n') ? "\n" : ", \t\r\n";

	for (const auto& attr_expr: StringTokenIterator(list, delim)) {
		std::string::size_type eqpos = attr_expr.find('=');
		if (std::string::npos == eqpos) {
			// There's no = in this description, it must be one
			// percentage or fraction for all attributes.
			// For example "1/4" or "25%".  So, we can just parse
			// it as a percentage and use that for everything.
			default_share = parse_share_value(attr_expr.c_str(), type_id, failmode, default_share_special);
			if (default_share_special == SPECIAL_SHARE_NONE && (default_share < -1 || default_share > 0)) {
				::dprintf( D_ALWAYS, "ERROR: Bad description of slot type %d: "
					"\"%s\" is invalid.\n"
					"\tYou must specify a percentage (like \"25%%\"), "
					"a fraction (like \"1/4\"),\n"
					"\tor list all attributes (like \"c=1, r=25%%, s=25%%, d=25%%\").\n"
					"\tSee the manual for details.\n",
					type_id, attr_expr.c_str());
				if (failmode == BuildSlotFailureMode::Except) {
					DC_Exit( 4 );
				} else {
					return false;
				}
			}
			if (default_share_special == SPECIAL_SHARE_MINIMAL) request.allow_fractional_cpus = true;
			continue;
		}

		// If we're still here, this is part of a string that
		// lists out separate attributes and the share for each one.

		// Get the value for this attribute.  It'll either be a
		// percentage, or it'll be a distinct value (in which
		// case, parse_share_value() will return negative.
		std::string val = attr_expr.substr(1+eqpos); trim(val);
		std::string constr;
		if (val.empty()) {
			::dprintf(D_ALWAYS, "Can't parse attribute \"%s\" in description of slot type %d\n",
				attr_expr.c_str(), type_id);
			if (failmode == BuildSlotFailureMode::Except) {
				DC_Exit( 4 );
			} else {
				return false;
			}
		}
		std::string::size_type colonpos = val.find(':');
		if (std::string::npos != colonpos) {
			constr = val.substr(colonpos + 1);
			trim(constr);
			val = val.substr(0, colonpos);
			trim(val);
		}
		special_share_t share_special = SPECIAL_SHARE_NONE;
		double share = parse_share_value(val.c_str(), type_id, failmode, share_special);

		// Figure out what attribute we're dealing with.
		std::string attr = attr_expr.substr(0, eqpos);
		trim(attr);
		slotres_map_t::const_iterator f(m_attr->machres().find(attr));
		if (f != m_attr->machres().end()) {
			request.slotres[f->first] = compute_local_resource(f->second, share);
			if ( ! constr.empty()) {
				request.slotres_constr[f->first] = constr;
			}
			continue;
		}

		// before 8.1.4, only the first character of standard resource types
		// was looked at (case insensitively). This prevented us from detecting
		// typos in resource type names that start with c,r,m,s,v or d.
		// This code now compares the whole resource type name
		// But to preserve (most) of the pre 8.1.4 we will tolerate
		// any case-insensitive substring match of cpus, ram, memory, swap, virtualmemory, or disk
		int cattr = int(attr.length());
		if (MATCH == strncasecmp(attr.c_str(), "cpus", cattr))
		{
			double lower_bound = (request.allow_fractional_cpus) ? 0.0 : 1.0;
			cpus = compute_local_resource(m_attr->num_cpus(), share, lower_bound);
		} else if (
			MATCH == strncasecmp(attr.c_str(), "ram", cattr) ||
			MATCH == strncasecmp(attr.c_str(), "memory", cattr))
		{
			ram = compute_local_resource(m_attr->phys_mem(), share, 1);
		} else if (
			MATCH == strncasecmp(attr.c_str(), "swap", cattr) ||
			MATCH == strncasecmp(attr.c_str(), "virtualmemory", cattr))
		{
			// for now, we can only tolerate swap expressed a 'auto' or a proportion
			// because we don't know how much swap is available until later.
			if (share <= 0 || IS_AUTO_SHARE(share)) {
				swap_fraction = compute_local_resource(1.0, share);
			} else {
				::dprintf( D_ALWAYS,
					"You must specify a percent or fraction for swap in slot type %d\n",
					type_id );
				if (failmode == BuildSlotFailureMode::Except) {
					DC_Exit( 4 );
				} else {
					return false;
				}
			}
		} else if (
			MATCH == strncasecmp(attr.c_str(), "disk", cattr))
		{
			// for now, we can only tolerate disk expressed a 'auto' or a proportion
			// because we don't know how much disk is available until later.
			if (share <= 0 || IS_AUTO_SHARE(share)) {
				disk_fraction = compute_local_resource(1.0, share);
			} else {
				::dprintf( D_ALWAYS,
					"You must specify a percent or fraction for disk in slot type %d\n",
					type_id );
				if (failmode == BuildSlotFailureMode::Except) {
					DC_Exit( 4 );
				} else {
					return false;
				}
			}

		} else {

			// at this point in the code a non-zero amount of the resource is requested only if share > 0.
			// requesting non-zero amounts of *unknown* resources is a fatal error
			// but requesting 0 or a share of the remainder isn't.  this makes it possible
			// to have a common config between machines that can be used to express things
			// like - "if this machine has GPUs, I don't want this slot to get any"..
			if (share > 0) {
				::dprintf( D_ALWAYS, "Unknown attribute \"%s\" in slot type %d\n", attr.c_str(), type_id);
				if (failmode == BuildSlotFailureMode::Except) {
					DC_Exit( 4 );
				} else {
					return false;
				}
			} else {
				::dprintf( D_ALWAYS | D_FULLDEBUG, "Unknown attribute \"%s\" in slot type %d, no resources will be allocated\n", attr.c_str(), type_id);
			}
		}
	}

	int std_resource_lower_bound = (request.allow_fractional_cpus) ? 0 : 1;

	// We're all done parsing the string.  Any attribute not
	// listed will get the default share.
	if (IS_UNSET_SHARE(cpus)) {
		cpus = compute_local_resource(m_attr->num_cpus(), default_share, (double)std_resource_lower_bound);
	}
	request.num_cpus = cpus;

	if (IS_UNSET_SHARE(ram)) {
		ram = compute_local_resource(m_attr->phys_mem(), default_share, std_resource_lower_bound);
	}
	request.num_phys_mem = ram;

	if (IS_UNSET_SHARE(swap_fraction)) {
		swap_fraction = compute_local_resource(1.0, default_share);
	}
	request.virt_mem_fraction = swap_fraction;

	if (IS_UNSET_SHARE(disk_fraction)) {
		disk_fraction = compute_local_resource(1.0, default_share);
	}
	request.disk_fraction = disk_fraction;

	for (slotres_map_t::iterator j(request.slotres.begin());  j != request.slotres.end();  ++j) {
		if (IS_UNSET_SHARE(j->second)) {
			slotres_map_t::const_iterator f(m_attr->machres().find(j->first));
			// it shouldn't be possible to get here with keys in slotres that aren't in machres
			// but if it should happen it's a benign error, just allocate 0 of the unknown resource.
			double avail = (f != m_attr->machres().end()) ? f->second : 0;
			j->second = compute_local_resource(avail, default_share);
		}
	}

	return true;
}

