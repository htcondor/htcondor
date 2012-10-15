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

		// now replace "auto" shares with final value
	for (int i=0; i<num; i++) {
		cap = cap_array[i];
        cap->show_totals(D_ALWAYS);
		if( !avail.computeAutoShares(cap) ) {

				// We ran out of system resources.
			dprintf( D_ALWAYS,
					 "ERROR: Can't allocate slot id %d during auto "
					 "allocation of resources\n", i+1 );
			dprintf( D_ALWAYS | D_NOHEADER, "\tRequesting: " );
			cap->show_totals( D_ALWAYS );
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
        cap->show_totals(D_ALWAYS);
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
   "1/4", or it's a percent, like "25%" (in both cases we'd return
   .25), or it's a regular value, like "64", in which case, we return
   the negative value, so that our caller knows it's an absolute
   value, not a fraction.
*/
float parse_value( const char* str, int type, bool except )
{
	char *tmp, *foo = strdup( str );
	float val;
	if( strcasecmp(foo,"auto") == 0 || strcasecmp(foo,"automatic") == 0 ) {
		free( foo );
		return AUTO_SHARE;
	}
	else if( (tmp = strchr(foo, '%')) ) {
			// It's a percent
		*tmp = '\0';
		val = (float)atoi(foo) / 100;
		free( foo );
		return val;
	} else if( (tmp = strchr(foo, '/')) ) {
			// It's a fraction
		*tmp = '\0';
		if( ! tmp[1] ) {
			dprintf( D_ALWAYS, "Can't parse attribute \"%s\" in description of slot type %d\n",
					 foo, type );
			if( except ) {
				DC_Exit( 4 );
			} else {
				free( foo );
				return 0;
			}
		}
		val = (float)atoi(foo) / ((float)atoi(&tmp[1]));
		free( foo );
		return val;
	} else {
			// This must just be an absolute value.  Return it as a negative
			// float, so the caller knows it's not a fraction.
		val = -(float)atoi(foo);
		free( foo );
		return val;
	}
}


/*
  Generally speaking, we want to round down for fractional amounts of
  a CPU.  However, we never want to advertise less than 1.  Plus, if
  the share in question is negative, it means it's an absolute value, not a
  fraction.
*/
int compute_cpus( int num_cpus, float share )
{
	int cpus;
	if( IS_AUTO_SHARE(share) ) {
			// Currently, "auto" for cpus just means 1 cpu per slot.
		return 1;
	}
	if( share > 0 ) {
		cpus = (int)floor( share * num_cpus );
	} else {
		cpus = (int)floor( -share );
	}
	if( ! cpus ) {
		cpus = 1;
	}
	return cpus;
}


/*
  Generally speaking, we want to round down for fractional amounts of
  physical memory.  However, we never want to advertise less than 1.
  Plus, if the share in question is negative, it means it's an absolute
  value, not a fraction.
*/
int compute_phys_mem( MachAttributes *m_attr, float share )
{
	int phys_mem;
	if( IS_AUTO_SHARE(share) ) {
			// This will be replaced later with an even share of whatever
			// memory is left over.
		return AUTO_MEM;
	}
	if( share > 0 ) {
		phys_mem = (int)floor( share * m_attr->phys_mem() );
	} else {
		phys_mem = (int)floor( -share );
	}
	if( ! phys_mem ) {
		phys_mem = 1;
	}
	return phys_mem;
}


int compute_local_resource(const float share, const string& rname, const CpuAttributes::slotres_map_t& machres) {
    CpuAttributes::slotres_map_t::const_iterator f(machres.find(rname));
    if (f == machres.end()) {
        EXCEPT("Resource name %s was not defined in local machine resource table\n", rname.c_str());
    }
    if (IS_AUTO_SHARE(share)) return int(share);
    if (share > 0) return int(f->second * share);
    return int(-share);
}

CpuAttributes* buildSlot( MachAttributes *m_attr, int slot_id, StringList* list, int type, bool except )
{
    typedef CpuAttributes::slotres_map_t slotres_map_t;
	int cpus=0, ram=0;
	float disk=0, swap=0, share;
    slotres_map_t slotres;
	float default_share = AUTO_SHARE;

	MyString execute_dir, partition_id;
	GetConfigExecuteDir( slot_id, &execute_dir, &partition_id );

	if ( list == NULL) {
	  // give everything the default share and return

	  cpus = compute_cpus( m_attr->num_cpus(), default_share );
	  ram = compute_phys_mem( m_attr, default_share );
	  swap = default_share;
	  disk = default_share;

      for (slotres_map_t::const_iterator j(m_attr->machres().begin());  j != m_attr->machres().end();  ++j) {
          slotres[j->first] = default_share;
      }

	  return new CpuAttributes( m_attr, type, cpus, ram, swap, disk, slotres, execute_dir, partition_id );
	}
		// For this parsing code, deal with the following example
		// string list:
		// "c=1, r=25%, d=1/4, s=25%"
		// There may be a bare number (no equals sign) that specifies
		// the default share for any items not explicitly defined.  Example:
		// "c=1, 25%"

    for (slotres_map_t::const_iterator j(m_attr->machres().begin());  j != m_attr->machres().end();  ++j) {
        slotres[j->first] = AUTO_RES;
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
			default_share = parse_value(attr_expr.c_str(), type, except);
			if( default_share <= 0 && !IS_AUTO_SHARE(default_share) ) {
				dprintf( D_ALWAYS, "ERROR: Bad description of slot type %d: ",
						 type );
				dprintf( D_ALWAYS | D_NOHEADER,  "\"%s\" is invalid.\n", attr_expr.c_str() );
				dprintf( D_ALWAYS | D_NOHEADER,
						 "\tYou must specify a percentage (like \"25%%\"), " );
				dprintf( D_ALWAYS | D_NOHEADER, "a fraction (like \"1/4\"),\n" );
				dprintf( D_ALWAYS | D_NOHEADER,
						 "\tor list all attributes (like \"c=1, r=25%%, s=25%%, d=25%%\").\n" );
				dprintf( D_ALWAYS | D_NOHEADER,
						 "\tSee the manual for details.\n" );
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
			// case, parse_value() will return negative.
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
		share = parse_value(val.c_str(), type, except);

		// Figure out what attribute we're dealing with.
        string attr = attr_expr.substr(0, eqpos);
        slotres_map_t::const_iterator f(m_attr->machres().find(attr));
        if (f != m_attr->machres().end()) {
            slotres[f->first] = compute_local_resource(share, attr, m_attr->machres());
            continue;
        }
		switch( tolower(attr[0]) ) {
		case 'c':
			cpus = compute_cpus( m_attr->num_cpus(), share );
			break;
		case 'r':
		case 'm':
			ram = compute_phys_mem( m_attr, share );
			break;
		case 's':
		case 'v':
			if( share > 0 || IS_AUTO_SHARE(share) ) {
				swap = share;
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
			break;
		case 'd':
			if( share > 0 || IS_AUTO_SHARE(share) ) {
				disk = share;
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
			break;
		default:
			dprintf( D_ALWAYS, "Unknown attribute \"%s\" in slot type %d\n",
					 attr.c_str(), type );
			if( except ) {
				DC_Exit( 4 );
			} else {
				return NULL;
			}
			break;
		}
	}

		// We're all done parsing the string.  Any attribute not
		// listed will get the default share.
	if( ! cpus ) {
		cpus = compute_cpus( m_attr->num_cpus(), default_share );
	}
	if( ! ram ) {
		ram = compute_phys_mem( m_attr, default_share );
	}
	if( swap <= 0.0001 ) {
		swap = default_share;
	}
	if( disk <= 0.0001 ) {
		disk = default_share;
	}
    for (slotres_map_t::iterator j(slotres.begin());  j != slotres.end();  ++j) {
        if (int(j->second) == AUTO_RES) {
            j->second = compute_local_resource(default_share, j->first, m_attr->machres());
        }
    }

		// Now create the object.
	return new CpuAttributes( m_attr, type, cpus, ram, swap, disk, slotres, execute_dir, partition_id );
}

