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
#include "string_list.h"
#include "condor_config.h"
#include "ResAttributes.h"
#include "condor_daemon_core.h"
#include "slot_builder.h"

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
void GetConfigExecuteDir( int slot_id, MyString *execute_dir, MyString *partition_id )
{
	MyString execute_param;
	char *execute_value = NULL;
	execute_param.formatstr("SLOT%d_EXECUTE",slot_id);
	execute_value = param( execute_param.Value() );
	if( !execute_value ) {
		execute_value = param( "EXECUTE" );
	}
	if( !execute_value ) {
		EXCEPT("EXECUTE (or %s) is not defined in the configuration.",
			   execute_param.Value());
	}

#if defined(WIN32)
	int i;
		// switch delimiter char in exec path on WIN32
	for (i=0; execute_value[i]; i++) {
		if (execute_value[i] == '/') {
			execute_value[i] = '\\';
		}
	}
#endif

	*execute_dir = execute_value;
	dprintf(D_FULLDEBUG, "Got execute_dir = %s\n",execute_dir->Value());
	free( execute_value );

		// Get a unique identifier for the partition containing the execute dir
	char *partition_value = NULL;
	bool partition_rc = sysapi_partition_id( execute_dir->Value(), &partition_value );
	if( !partition_rc ) {
		struct stat statbuf;
		if( stat(execute_dir->Value(), &statbuf)!=0 ) {
			int stat_errno = errno;
			EXCEPT("Error accessing execute directory %s specified in the configuration setting %s: (errno=%d) %s",
				   execute_dir->Value(), execute_param.Value(), stat_errno, strerror(stat_errno) );
		}
		EXCEPT("Failed to get partition id for %s=%s",
			   execute_param.Value(), execute_dir->Value());
	}
	ASSERT( partition_value );
	*partition_id = partition_value;
	free(partition_value);
}

CpuAttributes** buildCpuAttrs( MachAttributes *m_attr, int max_types, StringList **type_strings, int total, int* type_num_array, bool except )
{
	int num;
	CpuAttributes* cap;
	CpuAttributes** cap_array;

		// Available system resources.
	AvailAttributes avail( m_attr );

	cap_array = new CpuAttributes* [total];
	if( ! cap_array ) {
		EXCEPT( "Out of memory!" );
	}

	num = 0;
	for (int i=0; i<max_types; i++) {
		if( type_num_array[i] ) {
			for (int j=0; j<type_num_array[i]; j++) {
				cap = buildSlot( m_attr, num+1, type_strings[i], i, except );
				if( avail.decrement(cap) && num < total ) {
					cap_array[num] = cap;
					num++;
				} else {
						// We ran out of system resources.
					dprintf( D_ALWAYS,
							 "ERROR: Can't allocate %s slot of type %d\n",
							 num_string(j+1), i );
					dprintf( D_ALWAYS | D_NOHEADER, "\tRequesting: " );
					cap->show_totals( D_ALWAYS );
					dprintf( D_ALWAYS | D_NOHEADER, "\tAvailable:  " );
					avail.show_totals( D_ALWAYS, cap );
					delete cap;	// This isn't in our array yet.
					if( except ) {
						EXCEPT( "Ran out of system resources" );
					} else {
							// Gracefully cleanup and abort
						for(int ii=0; ii<num; ii++ ) {
							delete cap_array[ii];
						}
						delete [] cap_array;
						return NULL;
					}
				}
			}
		}
	}

		// gather remaining resources and counts of slots desiring auto shares
		// of user-defined resources into remain_cap, and remain_cnt.  these
		// will be use to distribute auto shares in the loop below.
	AvailAttributes::slotres_map_t remain_cap, remain_cnt;
	avail.computeRemainder(remain_cap, remain_cnt);

	// to make the output easier to read, only report the unallocated cap for each slot type once.
	const int d_level = D_ALWAYS;
	unsigned long report_auto = (unsigned long)-1;

		// now replace "auto" shares with final value
	for (int i=0; i<num; i++) {
		cap = cap_array[i];
		unsigned long bit = 1 << cap->type();
		if (report_auto & bit) {
			dprintf(d_level, "Allocating auto shares for ");
			cap->show_totals(d_level);
			report_auto &= ~bit;
		}
		if( !avail.computeAutoShares(cap, remain_cap, remain_cnt) ) {

				// We ran out of system resources.
			dprintf( D_ALWAYS,
					 "ERROR: Can't allocate slot id %d during auto "
					 "allocation of resources\n", i+1 );
			dprintf(D_ALWAYS | D_NOHEADER, "\tRequesting ");
			cap->show_totals(d_level);
			dprintf( D_ALWAYS | D_NOHEADER, "\tAvailable:  " );
			avail.show_totals( D_ALWAYS, cap );
			delete cap;	// This isn't in our array yet.
			if( except ) {
				EXCEPT( "Ran out of system resources in auto allocation" );
			} else {
					// Gracefully cleanup and abort
				for (int j=0;  j<num;  j++) {
					delete cap_array[j];
				}
				delete [] cap_array;
				return NULL;
			}
		}
		cap->show_totals(d_level);
	}

	for (int i=0; i<num; i++) {
		cap_array[i]->bind_DevIds(i+1, 0);
	}
	return cap_array;
}

void initTypes( int max_types, StringList **type_strings, int except )
{
	int i;
	char* tmp;
	MyString buf;

	_checkInvalidParam("SLOT_TYPE_0", except);
		// CRUFT
	_checkInvalidParam("VIRTUAL_MACHINE_TYPE_0", except);

	if (! type_strings[0]) {
			// Type 0 is the special type for evenly divided slots.
		type_strings[0] = new StringList();
		type_strings[0]->initializeFromString("auto");
	}

	for( i=1; i < max_types; i++ ) {
		if( type_strings[i] ) {
			continue;
		}
		buf.formatstr("SLOT_TYPE_%d", i);
		tmp = param(buf.Value());
		if (!tmp) {
			if (param_boolean("ALLOW_VM_CRUFT", false)) {
				buf.formatstr("VIRTUAL_MACHINE_TYPE_%d", i);
				if (!(tmp = param(buf.Value()))) {
					continue;
				}
			} else {
				continue;
			}
		}
		type_strings[i] = new StringList();
		type_strings[i]->initializeFromString( tmp );
		free( tmp );
	}
}


int countTypes( int max_types, int num_cpus, int** array_ptr, bool except )
{
	int i, num=0, num_set=0;
    MyString param_name;
    MyString cruft_name;
	int* my_type_nums = new int[max_types];

	if( ! array_ptr ) {
		EXCEPT( "ResMgr:countTypes() called with NULL array_ptr!" );
	}

		// Type 0 is special, user's shouldn't define it.
	_checkInvalidParam("NUM_SLOTS_TYPE_0", except);
		// CRUFT
	_checkInvalidParam("NUM_VIRTUAL_MACHINES_TYPE_0", except);

	for( i=1; i<max_types; i++ ) {
		param_name.formatstr("NUM_SLOTS_TYPE_%d", i);
		if (param_boolean("ALLOW_VM_CRUFT", false)) {
			cruft_name.formatstr("NUM_VIRTUAL_MACHINES_TYPE_%d", i);
			my_type_nums[i] = param_integer(param_name.Value(),
											 param_integer(cruft_name.Value(),
														   0));
		} else {
			my_type_nums[i] = param_integer(param_name.Value(), 0);
		}
		if (my_type_nums[i]) {
			num_set = 1;
			num += my_type_nums[i];
		}
	}

	if( num_set ) {
			// We found type-specific stuff, use that.
		my_type_nums[0] = 0;
	} else {
			// We haven't found any special types yet.  Therefore,
			// we're evenly dividing things, so we only have to figure
			// out how many nodes to advertise.
		if (param_boolean("ALLOW_VM_CRUFT", false)) {
			my_type_nums[0] = param_integer("NUM_SLOTS",
										  param_integer("NUM_VIRTUAL_MACHINES",
														num_cpus));
		} else {
			my_type_nums[0] = param_integer("NUM_SLOTS", num_cpus);
		}
		num = my_type_nums[0];
	}
	*array_ptr = my_type_nums;
	return num;
}


/*
   Parse a string in one of a few formats, and return a float that
   represents the value.  Either it's "auto", or it's a fraction, like
   "1/4", or it's a percent, like "25%" (these are equivalent),
   or it's a regular value, like "64"
   auto and fraction/percent are returned as negative values
*/
static double parse_share_value( const char* str, int type, bool except )
{
	if( strcasecmp(str,"auto") == 0 || strcasecmp(str,"automatic") == 0 ) {
		return AUTO_SHARE;
	}

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
			if( except ) {
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

CpuAttributes* buildSlot( MachAttributes *m_attr, int slot_id, StringList* list, int type, bool except )
{
    typedef CpuAttributes::slotres_map_t slotres_map_t;
	int cpus = UNSET_SHARE;
	int ram = UNSET_SHARE; // this is in MB so an int is enough
	double disk_fraction = UNSET_SHARE;
	double swap_fraction = UNSET_SHARE;
	slotres_map_t slotres;
	double default_share = AUTO_SHARE;

	MyString execute_dir, partition_id;
	GetConfigExecuteDir( slot_id, &execute_dir, &partition_id );

	if ( list == NULL) {
	  // give everything the default share and return

	  cpus = compute_local_resource(m_attr->num_cpus(), default_share, 1);
	  ram = compute_local_resource(m_attr->phys_mem(), default_share, 1);

      for (slotres_map_t::const_iterator j(m_attr->machres().begin());  j != m_attr->machres().end();  ++j) {
          slotres[j->first] = default_share;
      }

	  return new CpuAttributes( m_attr, type, cpus, ram, AUTO_SHARE, AUTO_SHARE, slotres, execute_dir, partition_id );
	}
		// For this parsing code, deal with the following example
		// string list:
		// "c=1, r=25%, d=1/4, s=25%"
		// There may be a bare number (no equals sign) that specifies
		// the default share for any items not explicitly defined.  Example:
		// "c=1, 25%"

    for (slotres_map_t::const_iterator j(m_attr->machres().begin());  j != m_attr->machres().end();  ++j) {
        slotres[j->first] = UNSET_SHARE;
    }

	list->rewind();
	while (char* attrp = list->next()) {
        string attr_expr = attrp;
        string::size_type eqpos = attr_expr.find('=');
		if (string::npos == eqpos) {
				// There's no = in this description, it must be one
				// percentage or fraction for all attributes.
				// For example "1/4" or "25%".  So, we can just parse
				// it as a percentage and use that for everything.
			default_share = parse_share_value(attr_expr.c_str(), type, except);
			if (!IS_AUTO_SHARE(default_share) && (default_share < -1 || default_share > 0)) {
				dprintf( D_ALWAYS, "ERROR: Bad description of slot type %d: "
						"\"%s\" is invalid.\n"
						"\tYou must specify a percentage (like \"25%%\"), "
						"a fraction (like \"1/4\"),\n"
						"\tor list all attributes (like \"c=1, r=25%%, s=25%%, d=25%%\").\n"
						"\tSee the manual for details.\n",
						 type, attr_expr.c_str());
				if( except ) {
					DC_Exit( 4 );
				} else {
					return NULL;
				}
			}
			continue;
		}

			// If we're still here, this is part of a string that
			// lists out seperate attributes and the share for each one.

			// Get the value for this attribute.  It'll either be a
			// percentage, or it'll be a distinct value (in which
			// case, parse_share_value() will return negative.
        string val = attr_expr.substr(1+eqpos);
		if (val.empty()) {
			dprintf(D_ALWAYS, "Can't parse attribute \"%s\" in description of slot type %d\n",
					attr_expr.c_str(), type);
			if( except ) {
				DC_Exit( 4 );
			} else {
				return NULL;
			}
		}
		double share = parse_share_value(val.c_str(), type, except);

		// Figure out what attribute we're dealing with.
		string attr = attr_expr.substr(0, eqpos);
		trim(attr);
		slotres_map_t::const_iterator f(m_attr->machres().find(attr));
		if (f != m_attr->machres().end()) {
			slotres[f->first] = compute_local_resource(f->second, share);
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
			cpus = compute_local_resource(m_attr->num_cpus(), share, 1);
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
			// because we don't know how much disk is available until later.
			if (share <= 0 || IS_AUTO_SHARE(share)) {
				swap_fraction = compute_local_resource(1.0, share);
			} else {
				dprintf( D_ALWAYS,
						 "You must specify a percent or fraction for swap in slot type %d\n",
						 type );
				if( except ) {
					DC_Exit( 4 );
				} else {
					return NULL;
				}
			}
		} else if (
			MATCH == strncasecmp(attr.c_str(), "disk", cattr))
		{
			// for now, we can only tolerate swap expressed a 'auto' or a proportion
			// because we don't know how much disk is available until later.
			if (share <= 0 || IS_AUTO_SHARE(share)) {
				disk_fraction = compute_local_resource(1.0, share);
			} else {
				dprintf( D_ALWAYS,
						 "You must specify a percent or fraction for disk in slot type %d\n",
						type );
				if( except ) {
					DC_Exit( 4 );
				} else {
					return NULL;
				}
			}

		} else {

			// at this point in the code a non-zero amount of the resource is requested only if share > 0.
			// requesting non-zero amounts of *unknown* resources is a fatal error
			// but requesting 0 or a share of the remainder isn't.  this makes it possible
			// to have a common config between machines that can be used to express things
			// like - "if this machine has GPUs, I don't want this slot to get any"..
			if (share > 0) {
				dprintf( D_ALWAYS, "Unknown attribute \"%s\" in slot type %d\n", attr.c_str(), type);
				if( except ) {
					DC_Exit( 4 );
				} else {
					return NULL;
				}
			} else {
				dprintf( D_ALWAYS | D_FULLDEBUG, "Unknown attribute \"%s\" in slot type %d, no resources will be allocated\n", attr.c_str(), type);
			}
		}
	}

		// We're all done parsing the string.  Any attribute not
		// listed will get the default share.
	if (IS_UNSET_SHARE(cpus)) {
		cpus = compute_local_resource(m_attr->num_cpus(), default_share, 1);
	}
	if (IS_UNSET_SHARE(ram)) {
		ram = compute_local_resource(m_attr->phys_mem(), default_share, 1);
	}
	if (IS_UNSET_SHARE(swap_fraction)) {
		swap_fraction = compute_local_resource(1.0, default_share);
	}
	if (IS_UNSET_SHARE(disk_fraction)) {
		disk_fraction = compute_local_resource(1.0, default_share);
	}
	for (slotres_map_t::iterator j(slotres.begin());  j != slotres.end();  ++j) {
		if (IS_UNSET_SHARE(j->second)) {
			slotres_map_t::const_iterator f(m_attr->machres().find(j->first));
			// it shouldn't be possible to get here with keys in slotres that aren't in machres
			// but if it should happen it's a benign error, just allocate 0 of the unknown resource.
			double avail = (f != m_attr->machres().end()) ? f->second : 0;
			j->second = compute_local_resource(avail, default_share);
		}
	}

		// Now create the object.
	return new CpuAttributes( m_attr, type, cpus, ram, swap_fraction, disk_fraction, slotres, execute_dir, partition_id );
}

