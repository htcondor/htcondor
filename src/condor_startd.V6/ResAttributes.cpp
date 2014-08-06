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
#include "startd.h"
#include "my_popen.h"
#include <math.h>
#include "filesystem_remap.h"

#include <set>
#include <algorithm>

#ifdef WIN32
#include "winreg.windows.h"
#endif

MachAttributes::MachAttributes()
   : m_user_specified(NULL, ";"), m_user_settings_init(false), m_named_chroot()
{
	m_mips = -1;
	m_kflops = -1;
	m_last_benchmark = 0;
	m_last_keypress = time(0)-1;
	m_seen_keypress = false;

	m_arch = NULL;
	m_opsys = NULL;
	m_opsysver = 0;
	m_opsys_and_ver = NULL;
	m_opsys_major_ver = 0;
	m_opsys_name = NULL;
	m_opsys_long_name = NULL;
	m_opsys_short_name = NULL;
	m_opsys_legacy = NULL;
	m_uid_domain = NULL;
	m_filesystem_domain = NULL;
	m_idle_interval = -1;
	m_ckptpltfrm = NULL;

	m_clock_day = -1;
	m_clock_min = -1;
	m_condor_load = -1.0;
	m_console_idle = 0;
	m_idle = 0;
	m_load = -1.0;
	m_owner_load = -1.0;
	m_virt_mem = 0;

		// Number of CPUs.  Since this is used heavily by the ResMgr
		// instantiation and initialization, we need to have a real
		// value for this as soon as the MachAttributes object exists.
	bool count_hyper = param_boolean("COUNT_HYPERTHREAD_CPUS", true);
	int ncpus=0, nhyper_cpus=0;
	sysapi_ncpus_raw(&ncpus, &nhyper_cpus);
	m_num_real_cpus = count_hyper ? nhyper_cpus : ncpus;
	m_num_cpus = param_integer("NUM_CPUS");


		// The same is true of physical memory.  If we had an error in
		// sysapi_phys_memory(), we need to just EXCEPT with a message
		// telling the user to define MEMORY manually.
	m_phys_mem = sysapi_phys_memory();
	if( m_phys_mem <= 0 ) {
		dprintf( D_ALWAYS, 
				 "Error computing physical memory with calc_phys_mem().\n" );
		dprintf( D_ALWAYS | D_NOHEADER, 
				 "\t\tMEMORY parameter not defined in config file.\n" );
		dprintf( D_ALWAYS | D_NOHEADER, 
				 "\t\tTry setting MEMORY to the number of megabytes of RAM.\n"
				 );
		EXCEPT( "Can't compute physical memory." );
	}

	dprintf( D_FULLDEBUG, "Memory: Detected %d megs RAM\n", m_phys_mem );

	// identification of the checkpointing platform signature
	m_ckptpltfrm = strdup( sysapi_ckptpltfrm() );

	// temporary attributes for raw utsname info
	m_utsname_sysname = NULL;
	m_utsname_nodename = NULL;
	m_utsname_release = NULL;
	m_utsname_version = NULL;
	m_utsname_machine = NULL;


#if defined ( WIN32 )
	// Get the version information of the copy of Windows 
	// we are running
	ZeroMemory ( &m_window_version_info, sizeof ( OSVERSIONINFOEX ) );
	m_window_version_info.dwOSVersionInfoSize = 
		sizeof ( OSVERSIONINFOEX );	
	m_got_windows_version_info = 
		GetVersionEx ( (OSVERSIONINFO*) &m_window_version_info );
	if ( !m_got_windows_version_info ) {
		m_window_version_info.dwOSVersionInfoSize = 
			sizeof ( OSVERSIONINFO );	
		m_got_windows_version_info = 
			GetVersionEx ( (OSVERSIONINFO*) &m_window_version_info );
		if ( !m_got_windows_version_info ) {
			dprintf ( D_ALWAYS, "MachAttributes: failed to "
				"get Windows version information.\n" );
		}
	}
	m_local_credd = NULL;
	m_last_credd_test = 0;
    m_dot_Net_Versions = NULL;
#endif
}


MachAttributes::~MachAttributes()
{
	if( m_arch ) free( m_arch );
	if( m_opsys ) free( m_opsys );
	if( m_opsys_and_ver ) free( m_opsys_and_ver );
	if( m_opsys_name ) free( m_opsys_name );
	if( m_opsys_long_name ) free( m_opsys_long_name );
	if( m_opsys_short_name ) free( m_opsys_short_name );
	if( m_opsys_legacy ) free( m_opsys_legacy );
	if( m_uid_domain ) free( m_uid_domain );
	if( m_filesystem_domain ) free( m_filesystem_domain );
	if( m_ckptpltfrm ) free( m_ckptpltfrm );

	if( m_utsname_sysname ) free( m_utsname_sysname );
	if( m_utsname_nodename ) free( m_utsname_nodename );
	if( m_utsname_release ) free( m_utsname_release );
	if( m_utsname_version ) free( m_utsname_version );
	if( m_utsname_machine ) free( m_utsname_machine );

    AttribValue *val = NULL;
    m_lst_dynamic.Rewind();
    while ((val = m_lst_dynamic.Next()) ) {
       if (val) free (val);
       m_lst_dynamic.DeleteCurrent();
    }

    m_lst_static.Rewind();
    while ((val = m_lst_static.Next())) {
       if (val) free (val);
       m_lst_static.DeleteCurrent();
    }
    m_user_specified.clearAll();
#if defined(WIN32)
	if( m_local_credd ) free( m_local_credd );
    if( m_dot_Net_Versions ) free ( m_dot_Net_Versions );

    release_all_WinPerf_results();
#endif
}

void
MachAttributes::init_user_settings()
{
	m_user_settings_init = true;

	AttribValue *val = NULL;
	m_lst_dynamic.Rewind();
	while ((val = m_lst_dynamic.Next()))
    {
        if (val) free (val);
	    m_lst_dynamic.DeleteCurrent();
	}

	m_lst_static.Rewind();
	while ((val = m_lst_static.Next()))
    {
	    if (val) free (val);
	    m_lst_static.DeleteCurrent();
	}

	m_user_specified.clearAll();
	char * pszParam = NULL;
   #ifdef WIN32
	pszParam = param("STARTD_PUBLISH_WINREG");
   #endif
	if (pszParam)
    {
		m_user_specified.initializeFromString(pszParam);
		free(pszParam);
	}

	m_user_specified.rewind();
	while(char * pszItem = m_user_specified.next())
    {
		// if the reg_item is of the form attr_name=reg_path;
		// then skip over the attr_name and '=' and trailing
		// whitespace.  But if the = is after the first \, then
		// it's a part of the reg_path, so ignore it.
		//
		const char * pkey = strchr(pszItem, '=');
		const char * pbs  = strchr(pszItem, '\\');
		if (pkey && ( ! pbs || pkey < pbs)) {
			++pkey; // skip the '='
		} else {
			pkey = pszItem;
		}

		// skip any leading whitespace in the key
		while (isspace(pkey[0]))
			++pkey;

       #ifdef WIN32
		char * pszAttr = generate_reg_key_attr_name("WINREG_", pszItem);

		// if the keyname begins with 32 or 64, use that to designate either
		// the WOW64 or WOW32 view of the registry, but don't pass the
		// leading number on to the lower level code.
		int options = 0;
		if (isdigit(pkey[0])) {
			options = atoi(pkey);
			while (isdigit(pkey[0])) {
				++pkey;
			}
		}

		int ixStart = 0;
		int cch = lstrlen(pkey);
		HKEY hkey = parse_hive_prefix(pkey, cch, &ixStart);
		if (hkey == HKEY_PERFORMANCE_DATA)  // using dynamic data HIVE
        {
			// For dynamic data, we have to build a query
			// for the Windows Performance registry which we
			// will execute periodically to update the data.
			// the specific query is added to the dynamic list, and
			// the query Key (Memory, Processor, etc) is added
			// to the WinPerf list.
			//
            AttribValue * pav = add_WinPerf_Query(pszAttr, pkey+ixStart+1);
            if (pav)
		        m_lst_dynamic.Append(pav);
		}
        else
        {
			// for static registry data, get the current value from the 
			// registry and store it in an AttribValue in the static list.
			//
			char * value = get_windows_reg_value(pkey, NULL, options);
            AttribValue * pav = AttribValue::Allocate(pszAttr, value);
            if (pav)
            {
                pav->pquery = (void*)pkey;
				m_lst_static.Append(pav);
			}
		}

       #else
		// ! Win32.

       #endif // WIN32

	} // end while(m_user_specified.next());

    

#ifdef WIN32
    // build and save off the string of .NET versions.
    //
    char* detected_versions = param("DOT_NET_VERSIONS");
    if( ! detected_versions)
    {
        string s_dot_Net_Versions;

        static const struct {
            const char * pszKey;
            const char * pszValue;
            const char * pszVer;
        } aNet[] = {
           "HKLM\\SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v1.1.4322",  "Install", "1.1",
           "HKLM\\SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v2.0.50727", "Install", "2.0",
           "HKLM\\SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v3.0",       "Install", "3.0",
           "HKLM\\SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v3.5",       "Install", "3.5",
           "HKLM\\SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Client", "Install", "4.0Client",
           "HKLM\\SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full",   "Install", "4.0Full",
        };

       for (int ii = 0; ii < NUM_ELEMENTS(aNet); ++ii)
       {
           char* pszVal = get_windows_reg_value(aNet[ii].pszKey, aNet[ii].pszValue, 0);
           if (pszVal)
           {
               DWORD dw = atoi(pszVal);
               if(dw)
               {
                   if( ! s_dot_Net_Versions.empty())
                       s_dot_Net_Versions.append(",");
                  s_dot_Net_Versions.append(aNet[ii].pszVer);
               }
               free(pszVal);
           }
       }

       if ( ! s_dot_Net_Versions.empty())
           detected_versions = _strdup(s_dot_Net_Versions.c_str());
    }
    if(m_dot_Net_Versions) free(m_dot_Net_Versions);
    m_dot_Net_Versions = detected_versions;

#endif //WIN32

}


void
MachAttributes::init()
{
	this->init_user_settings();
	this->compute( A_ALL );
}


void
MachAttributes::compute( amask_t how_much )
{
	// the startd doesn't normally call the init() method (bug?),
	// it just starts calling compute, so in order to gurantee that
	// init happens, we put check here to see if user settings have been initialized
	if ( ! m_user_settings_init)
		init_user_settings();

	if( IS_STATIC(how_much) && IS_SHARED(how_much) ) {

			// Since we need real values for them as soon as a
			// MachAttributes object is instantiated, we handle number
			// of CPUs and physical memory in the constructor, not
			// here.  -Derek Wright 2/5/99 

			// Arch, OpSys, FileSystemDomain and UidDomain.  Note:
			// these will always return something, since config() will
			// insert values if we don't have them in the config file.
		if( m_arch ) {
			free( m_arch );
		}
		m_arch = param( "ARCH" );

		if( m_opsys ) {
			free( m_opsys );
		}
		m_opsys = param( "OPSYS" );
		m_opsysver = param_integer( "OPSYSVER", 0 );

		if( m_opsys_and_ver ) {
			free( m_opsys_and_ver );
		}
		m_opsys_and_ver = param( "OPSYSANDVER" );
		m_opsys_major_ver = param_integer( "OPSYSMAJORVER", 0 );

		if( m_opsys_name ) {
			free( m_opsys_name );
                } 
		m_opsys_name = param( "OPSYSNAME" );

		if( m_opsys_long_name ) {
			free( m_opsys_long_name );
                } 
		m_opsys_long_name = param( "OPSYSLONGNAME" );

		if( m_opsys_short_name ) {
			free( m_opsys_short_name );
                } 
		m_opsys_short_name = param( "OPSYSSHORTNAME" );

		if( m_opsys_legacy ) {
			free( m_opsys_legacy );
                } 
		m_opsys_legacy = param( "OPSYSLEGACY" );

       		// temporary attributes for raw utsname info
		if( m_utsname_sysname ) {
			free( m_utsname_sysname );
		}
		if( m_utsname_nodename ) {
			free( m_utsname_nodename );
		}
		if( m_utsname_release ) {
			free( m_utsname_release );
		}
		if( m_utsname_version ) {
			free( m_utsname_version );
		}
		if( m_utsname_machine ) {
			free( m_utsname_machine );
		}

       		m_utsname_sysname = param( "UTSNAME_SYSNAME" );
		m_utsname_nodename = param( "UTSNAME_NODENAME" );
		m_utsname_release = param( "UTSNAME_RELEASE" );
		m_utsname_version = param( "UTSNAME_VERSION" );
		m_utsname_machine = param( "UTSNAME_MACHINE" );

		if( m_uid_domain ) {
			free( m_uid_domain );
		}
		m_uid_domain = param( "UID_DOMAIN" );
		dprintf( D_FULLDEBUG, "%s = \"%s\"\n", ATTR_UID_DOMAIN,
				 m_uid_domain );

		if( m_filesystem_domain ) {
			free( m_filesystem_domain );
		}
		m_filesystem_domain = param( "FILESYSTEM_DOMAIN" );
		dprintf( D_FULLDEBUG, "%s = \"%s\"\n", ATTR_FILE_SYSTEM_DOMAIN,
				 m_filesystem_domain );

		m_idle_interval = param_integer( "IDLE_INTERVAL", -1 );

		// checkpoint platform signature
		if (m_ckptpltfrm) {
			free(m_ckptpltfrm);
		}

		m_ckptpltfrm = strdup( sysapi_ckptpltfrm() );

		pair_strings_vector root_dirs = root_dir_list();
		std::stringstream result;
		unsigned int chroot_count = 0;
		for (pair_strings_vector::const_iterator it=root_dirs.begin();
				it != root_dirs.end(); 
				++it, ++chroot_count) {
			if (chroot_count) {
				result << ", ";
			}
			result << it->first;
		}
		if (chroot_count > 1) {
			std::string result_str = result.str();
			dprintf(D_FULLDEBUG, "Named chroots: %s\n", result_str.c_str() );
			m_named_chroot = result_str;
		}

    }


	if( IS_UPDATE(how_much) && IS_SHARED(how_much) ) {

		m_virt_mem = sysapi_swap_space();
		dprintf( D_FULLDEBUG, "Swap space: %lld\n", m_virt_mem );

#if defined(WIN32)
		credd_test();
#endif
	}


	if( IS_TIMEOUT(how_much) && IS_SHARED(how_much) ) {
		m_load = sysapi_load_avg();

		sysapi_idle_time( &m_idle, &m_console_idle );

		time_t my_timer;
		struct tm *the_time;
		time( &my_timer );
		the_time = localtime(&my_timer);
		m_clock_min = (the_time->tm_hour * 60) + the_time->tm_min;
		m_clock_day = the_time->tm_wday;

		if (m_last_keypress < my_timer - m_idle) {
			if (m_idle_interval >= 0) {
				int duration = my_timer - m_last_keypress;
				if (duration > m_idle_interval) {
					if (m_seen_keypress) {
						dprintf(D_IDLE, "end idle interval of %d sec.\n",
								duration);
					} else {
						dprintf(D_IDLE,
								"first keyboard event %d sec. after startup\n",
								duration);
					}
				}
			}
			m_last_keypress = my_timer;
			m_seen_keypress = true;
		}

#ifdef WIN32
        update_all_WinPerf_results();
#endif

        AttribValue *pav = NULL;
        m_lst_dynamic.Rewind();
        while ((pav = m_lst_dynamic.Next()) ) {
           if (pav) {
             #ifdef WIN32
              if ( ! update_WinPerf_Value(pav))
                 pav->vtype = AttribValue_DataType_Max; // undefined vtype
             #else
              if (pav->pquery) {
                 // insert code to update pav from pav->pquery
              }
             #endif
           }
        }
	}

	if( IS_TIMEOUT(how_much) && IS_SUMMED(how_much) ) {
		m_condor_load = resmgr->sum( &Resource::condor_load );
		if( m_condor_load > m_load ) {
			m_condor_load = m_load;
		}
	}


}

// Allocate a user-defined resource deviceid to a slot
// when assign_to_sub > 0, then assign an unused resource from
// slot assigned_to to it's child dynamic slot assign_to_sub.
//
const char * MachAttributes::AllocateDevId(const std::string & tag, int assign_to, int assign_to_sub)
{
	if ( ! assign_to) return NULL;

	slotres_devIds_map_t::const_iterator f(m_machres_devIds_map.find(tag));
	if (f !=  m_machres_devIds_map.end()) {
		const slotres_assigned_ids_t & ids(f->second);
		slotres_assigned_id_owners_t & owners = m_machres_devIdOwners_map[tag];
		while (owners.size() < ids.size()) { owners.push_back(0); }

		// when the sub-id is positive, that means we are binding a dynamic slot
		// in that case we want to use only dev-ids that are already owned by the partitionable slot.
		// but not yet assigned to any of  it's dynamic children.
		//
		if (assign_to_sub > 0) {
			for (int ii = 0; ii < (int)ids.size(); ++ii) {
				if (owners[ii].id == assign_to && owners[ii].dyn_id == 0) {
					owners[ii].dyn_id = assign_to_sub;
					return ids[ii].c_str();
				}
			}
		} else {
			for (int ii = 0; ii < (int)ids.size(); ++ii) {
				if ( ! owners[ii].id) {
					owners[ii] = FullSlotId(assign_to, assign_to_sub);
					return ids[ii].c_str();
				}
			}
		}
	}
	return NULL;
}

bool MachAttributes::ReleaseDynamicDevId(const std::string & tag, const char * id, int was_assigned_to, int was_assign_to_sub)
{
	slotres_devIds_map_t::const_iterator f(m_machres_devIds_map.find(tag));
	if (f !=  m_machres_devIds_map.end()) {
		const slotres_assigned_ids_t & ids(f->second);
		slotres_assigned_id_owners_t & owners = m_machres_devIdOwners_map[tag];
		while (owners.size() < ids.size()) { owners.push_back(0); }

		for (int ii = 0; ii < (int)ids.size(); ++ii) {
			if (id == ids[ii]) {
				if (owners[ii].id == was_assigned_to && owners[ii].dyn_id == was_assign_to_sub) {
					owners[ii].dyn_id = 0;
					return true;
				}
				break;
			}
		}
	}
	return false;
}

// res_value is a string that contains either a number, which is the count of a
// fungable resource, or a list of ids of non-fungable resources.
// if res_value contains a list, then ids is set on exit from this function
// otherwise, ids empty.
static double parse_user_resource_config(const char * tag, const char * res_value, StringList & ids)
{
	ids.clearAll();
	double num = 0;
	char * pend = NULL;
	num = strtod(res_value, &pend);
	if (pend != res_value) { while(isspace(*pend)) { ++pend; } }
	bool is_simple_double = (pend && pend != res_value && !*pend);
	if ( ! is_simple_double) {
		// ok not a simple double, try evaluating it as a classad expression.
		ClassAd ad;
		if (ad.AssignExpr(tag,res_value) && ad.EvalFloat(tag, NULL, num)) {
			// it was an expression that evaluated to a double, so it's a simple double after all
			is_simple_double = true;
		} else {
			// not a simple double, so treat it as a list of id's, the number of resources is the number of ids
			ids.initializeFromString(res_value);
			num = ids.number();
		}
	}

	return num;
}

// run a script, and take use its output to configure a custom resource of type tag
//
double MachAttributes::init_machine_resource_from_script(const char * tag, const char * script_cmd) {

	ArgList args;
	MyString errors;
	if(!args.AppendArgsV1RawOrV2Quoted(script_cmd, &errors)) {
		printf("Can't append cmd %s(%s)\n", script_cmd, errors.Value());
		return -1;
	}

	double quantity = 0;
	int offline = 0;

	FILE * fp = my_popen(args, "r", FALSE);
	if ( ! fp) {
		//PRAGMA_REMIND("tj: should failure to run a res inventory script really bring down the startd?")
		EXCEPT("Failed to execute local resource '%s' inventory script \"%s\"\n", tag, script_cmd);
	} else {
		int error = 0;
		bool is_eof = false;
		ClassAd ad;
		int cAttrs = ad.InsertFromFile(fp, is_eof, error);
		if (cAttrs <= 0) {
			if (error) dprintf(D_ALWAYS, "Could not parse ClassAd for local resource '%s' (error %d) assuming quantity of 0\n", tag, error);
		} else {
			classad::Value value;
			MyString attr(ATTR_OFFLINE_PREFIX); attr += tag;
			MyString res_value;
			StringList offline_ids;
			if (ad.LookupString(attr.c_str(),res_value)) {
				offline = parse_user_resource_config(tag, res_value.c_str(), offline_ids);
			} else {
				attr = "OFFLINE_MACHINE_RESOURCE_"; attr += tag;
				if (param(res_value, attr.c_str()) && !res_value.empty()) {
					offline = parse_user_resource_config(tag, res_value.c_str(), offline_ids);
				}
			}

			attr = ATTR_DETECTED_PREFIX; attr += tag;
			if (ad.LookupString(attr.c_str(),res_value)) {
				StringList ids;
				quantity = parse_user_resource_config(tag, res_value.c_str(), ids);
				if ( ! ids.isEmpty()) {
					offline = 0; // we only want to count ids that match the detected list
					ids.rewind();
					while (const char* id = ids.next()) {
						if (offline_ids.contains(id)) {
							this->m_machres_offline_devIds_map[tag].push_back(id);
							++offline;
						} else {
							this->m_machres_devIds_map[tag].push_back(id);
						}
					}
				}
			} else if (ad.EvaluateAttr(attr, value) && value.IsNumber(quantity)) {
				// don't need to do anythin more here.
			}

			// make sure that the inventory ad doesn't have an attribute for the tag name
			// i.e. if the inventory is GPUS, then DetectedGPUs=N is required, but it cant have GPUs=N
			ad.Delete(tag);
			m_machres_attr.Update(ad);
		}
		my_pclose(fp);
	}

	return quantity - offline;
}

// this static callback for the param param iteration using foreach_param_*
// is called once for each param matching MACHINE_RESOURCE_*
// it extracts the resource tag from the param name, then fetches the resource quantity
// and initializes the m_machres_map 
bool MachAttributes::init_machine_resource(MachAttributes * pme, HASHITER & it) {
	const char * name = hash_iter_key(it);
	const char * rawval = hash_iter_value(it);
	if ( ! rawval || ! rawval[0])
		return true; // keep iterating

	char * res_value = param(name);
	if ( ! res_value)
		return true; // keep iterating

	double num = 0;
	double offline = 0;
	const char * tag = name + sizeof("MACHINE_RESOURCE_")-1;

	// MACHINE_RESOURCE_INVENTORY_* is a special case that runs a script that generates a classad.
	if (starts_with_ignore_case(tag, "INVENTORY_")) {
		tag += sizeof("INVENTORY_")-1;
		if (res_value) {
			num = pme->init_machine_resource_from_script(tag, res_value);
		}
	} else {
		StringList offline_ids;
		MyString off_name("OFFLINE_"); off_name += name;
		MyString my_value;
		if (param(my_value, off_name.c_str()) && !my_value.empty()) {
			offline = parse_user_resource_config(tag, my_value.c_str(), offline_ids);
		}

		// if the param value parses as a double, then we can handle it simply
		StringList ids;
		num = parse_user_resource_config(tag, res_value, ids);
		if ( ! ids.isEmpty()) {
			offline = 0; // we only want to count ids that match the detected list
			ids.rewind();
			while (const char* id = ids.next()) {
				if (offline_ids.contains(id)) {
					pme->m_machres_offline_devIds_map[tag].push_back(id);
					++offline;
				} else {
					pme->m_machres_devIds_map[tag].push_back(id);
				}
			}
		}
	}

	if (num < 0) num = 0;
	pme->m_machres_map[tag] = num - offline;
	free(res_value);

	return true; // keep iterating
}


void MachAttributes::init_machine_resources() {
	// defines the space of local machine resources, and their quantity
	m_machres_map.clear();
	// defines which local machine resource device IDs are in use
	m_machres_devIds_map.clear();
	m_machres_offline_devIds_map.clear();

	// this may be filled from resource inventory scripts
	m_machres_attr.Clear();

	Regex re;
	int err = 0;
	const char* pszMsg = 0;
	ASSERT(re.compile("^MACHINE_RESOURCE_[a-zA-Z0-9_]+", &pszMsg, &err, PCRE_CASELESS));
	const int iter_options = HASHITER_NO_DEFAULTS; // we can speed up iteration if there are no machine resources in the defaults table.
	foreach_param_matching(re, iter_options, (bool(*)(void*,HASHITER&))MachAttributes::init_machine_resource, this);

	StringList allowed;
	char* allowed_names = param("MACHINE_RESOURCE_NAMES");
	if (allowed_names && allowed_names[0]) {
		// if admin defined MACHINE_RESOURCE_NAMES, then use this list
		// as the source of expected custom machine resources
		allowed.initializeFromString(allowed_names);
		free(allowed_names);
	}

	StringList disallowed("CPU CPUS DISK SWAP MEM MEMORY RAM");

	for (slotres_map_t::iterator it(m_machres_map.begin());  it != m_machres_map.end();  ++it) {
		const char * tag = it->first.c_str();
		if ( ! allowed.isEmpty() && ! allowed.contains_anycase(tag)) {
			dprintf(D_ALWAYS, "Local machine resource %s is not in MACHINE_RESOURCE_NAMES so it will have no effect\n", tag);
			m_machres_map.erase(tag);
			continue;
		}
		if ( ! disallowed.isEmpty() && disallowed.contains_anycase(tag)) {
			EXCEPT("fatal error - MACHINE_RESOURCE_%s is invalid, '%s' is a reserved resource name\n", tag, tag);
			continue;
		}
		dprintf(D_ALWAYS, "Local machine resource %s = %g\n", tag, it->second);
	}
}

void
MachAttributes::final_idle_dprintf()
{
	if (m_idle_interval >= 0) {
		time_t my_timer = time(0);
		int duration = my_timer - m_last_keypress;
		if (duration > m_idle_interval) {
			dprintf(D_IDLE, "keyboard idle for %d sec. before shutdown\n",
					duration);
		}
	}
}

void
MachAttributes::publish( ClassAd* cp, amask_t how_much) 
{
	if( IS_STATIC(how_much) || IS_PUBLIC(how_much) ) {

			// STARTD_IP_ADDR
		cp->Assign( ATTR_STARTD_IP_ADDR,
					daemonCore->InfoCommandSinfulString() );

        	cp->Assign( ATTR_ARCH, m_arch );

		cp->Assign( ATTR_OPSYS, m_opsys );
       	 	if (m_opsysver) {
			cp->Assign( ATTR_OPSYSVER, m_opsysver );
 	        }
		cp->Assign( ATTR_OPSYS_AND_VER, m_opsys_and_ver );

       	 	if (m_opsys_major_ver) {
			cp->Assign( ATTR_OPSYS_MAJOR_VER, m_opsys_major_ver );
        	}
        	if (m_opsys_name) {
			cp->Assign( ATTR_OPSYS_NAME, m_opsys_name );
	        }
        	if (m_opsys_long_name) {
			cp->Assign( ATTR_OPSYS_LONG_NAME, m_opsys_long_name );
	        }
        	if (m_opsys_short_name) {
			cp->Assign( ATTR_OPSYS_SHORT_NAME, m_opsys_short_name );
	        }
        	if (m_opsys_legacy) {
			cp->Assign( ATTR_OPSYS_LEGACY, m_opsys_legacy );
	        }

		cp->Assign( ATTR_UID_DOMAIN, m_uid_domain );

		cp->Assign( ATTR_FILE_SYSTEM_DOMAIN, m_filesystem_domain );

		cp->Assign( ATTR_HAS_IO_PROXY, true );

		cp->Assign( ATTR_CHECKPOINT_PLATFORM, m_ckptpltfrm );

#if defined ( WIN32 )
		// publish the Windows version information
		if ( m_got_windows_version_info ) {
			cp->Assign( ATTR_WINDOWS_MAJOR_VERSION, 
				(int)m_window_version_info.dwMajorVersion );
			cp->Assign( ATTR_WINDOWS_MINOR_VERSION, 
				(int)m_window_version_info.dwMinorVersion );
			cp->Assign( ATTR_WINDOWS_BUILD_NUMBER, 
				(int)m_window_version_info.dwBuildNumber );
			// publish the extended Windows version information if we
			// have it at our disposal
			if ( sizeof ( OSVERSIONINFOEX ) == 
				 m_window_version_info.dwOSVersionInfoSize ) {
				cp->Assign( ATTR_WINDOWS_SERVICE_PACK_MAJOR, 
					(int)m_window_version_info.wServicePackMajor );
				cp->Assign( ATTR_WINDOWS_SERVICE_PACK_MINOR, 
					(int)m_window_version_info.wServicePackMinor );
				cp->Assign( ATTR_WINDOWS_PRODUCT_TYPE, 
					(int)m_window_version_info.wProductType );
			}
		}


        /*
        Publish .Net versions installed on current machine assuming
        the option is turned on..
        */
        if(param_boolean("STARTD_PUBLISH_DOTNET", true))
        {
            if(m_dot_Net_Versions)
                cp->Assign(ATTR_DOTNET_VERSIONS, m_dot_Net_Versions);
        }

        // publish values from the window's registry as specified
		// in the STARTD_PUBLISH_WINREG param.
		//
		m_lst_static.Rewind();
		while (AttribValue *pav = m_lst_static.Next()) {
			if (pav) pav->AssignToClassAd(cp);
		}
        /*
     	char * pubreg_param = param("STARTD_PUBLISH_WINREG");
		if (pubreg_param) {
			StringList reg_list(pubreg_param, ";");
			reg_list.rewind();

			while(char * reg_item = reg_list.next()) {

				// if the reg_item is of the form attr_name=reg_path;
				// then skip over the attr_name and '=' and trailing
				// whitespace.  But if the = is after the first \, then
				// it's a part of the reg_path, so ignore it.
				//
				const char * pkey = strchr(reg_item, '=');
				const char * pbs  = strchr(reg_item, '\\');
				if (pkey && ( ! pbs || pkey < pbs)) {
					++pkey; // skip the '='
				} else {
					pkey = reg_item;
				}

				// skip any leading whitespace in the reg_path
				while (isspace(pkey[0]))
					++pkey;

				// if the keyname begins with 32 or 64, use that to designate either
				// the WOW64 or WOW32 view of the registry, but don't pass the
				// leading number on to the lower level code.
				int options = 0;
				if (isdigit(pkey[0])) {
					options = atoi(pkey);
					while (isdigit(pkey[0])) {
						++pkey;
					}
				}

				// get the registry value of the given key, and assign it 
				// to the given attribute name, or a generated name.
				//
				char * value = get_windows_reg_value(pkey, NULL, options);
				if (value) {
					char * attr_name = generate_reg_key_attr_name("WINREG_", reg_item);
					if (attr_name) {
						cp->Assign(attr_name, value);
						free (attr_name);
					}
					free(value);
				}
			}

			free (pubreg_param);
		}
        */
#endif // defined ( WIN32 )

	}

	if( IS_UPDATE(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_TOTAL_VIRTUAL_MEMORY, m_virt_mem );

		cp->Assign( ATTR_TOTAL_CPUS, m_num_cpus );

		cp->Assign( ATTR_TOTAL_MEMORY, m_phys_mem );

			// KFLOPS and MIPS are only conditionally computed; thus, only
			// advertise them if we computed them.
		if ( m_kflops > 0 ) {
			cp->Assign( ATTR_KFLOPS, m_kflops );
		}
		if ( m_mips > 0 ) {
			cp->Assign( ATTR_MIPS, m_mips );
		}

#if defined(WIN32)
		if ( m_local_credd != NULL ) {
			cp->Assign(ATTR_LOCAL_CREDD, m_local_credd);
		}
#endif

		string machine_resources = "Cpus Memory Disk Swap";
		// publish any local resources
		for (slotres_map_t::iterator j(m_machres_map.begin());  j != m_machres_map.end();  ++j) {
			const char * rname = j->first.c_str();
			string attr(ATTR_DETECTED_PREFIX); attr += rname;
			double ipart, fpart = modf(j->second, &ipart);
			if (fpart >= 0.0 && fpart <= 0.0) {
				cp->Assign(attr.c_str(), (long long)ipart);
			} else {
				cp->Assign(attr.c_str(), j->second);
			}
			attr = ATTR_TOTAL_PREFIX; attr += rname;
			if (fpart >= 0.0 && fpart <= 0.0) {
				cp->Assign(attr.c_str(), (long long)ipart);
			} else {
				cp->Assign(attr.c_str(), j->second);
			}
			// are there any offline ids for this resource?
			slotres_devIds_map_t::const_iterator k = m_machres_offline_devIds_map.find(j->first);
			if (k != m_machres_offline_devIds_map.end()) {
				if ( ! k->second.empty()) {
					attr = ATTR_OFFLINE_PREFIX; attr += k->first;
					string ids;
					join(k->second, ",", ids);
					cp->Assign(attr.c_str(), ids.c_str());
				}
			}
			machine_resources += " ";
			machine_resources += j->first;
			}
		cp->Assign(ATTR_MACHINE_RESOURCES, machine_resources.c_str());
	}

		// We don't want this inserted into the public ad automatically
	if( IS_UPDATE(how_much) || IS_TIMEOUT(how_much) ) {
		cp->Assign( ATTR_LAST_BENCHMARK, (unsigned)m_last_benchmark );
	}


	if( IS_TIMEOUT(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_TOTAL_LOAD_AVG, rint(m_load * 100) / 100.0);
		
		cp->Assign( ATTR_TOTAL_CONDOR_LOAD_AVG,
					rint(m_condor_load * 100) / 100.0);
		
		cp->Assign( ATTR_CLOCK_MIN, m_clock_min );

		cp->Assign( ATTR_CLOCK_DAY, m_clock_day );

		m_lst_dynamic.Rewind();
		while (AttribValue *pav = m_lst_dynamic.Next() ) {
			if (pav) pav->AssignToClassAd(cp);
		}
	}

        // temporary attributes for raw utsname info
	cp->Assign( ATTR_UTSNAME_SYSNAME, m_utsname_sysname );
	cp->Assign( ATTR_UTSNAME_NODENAME, m_utsname_nodename );
	cp->Assign( ATTR_UTSNAME_RELEASE, m_utsname_release );
	cp->Assign( ATTR_UTSNAME_VERSION, m_utsname_version );
	cp->Assign( ATTR_UTSNAME_MACHINE, m_utsname_machine );

	// Advertise chroot information
	if ( m_named_chroot.size() > 0 ) {
		cp->Assign( "NamedChroot", m_named_chroot.c_str() );
	}

#ifdef WIN32
// window's strtok_s is the 'safe' version of strtok, it's not identical to linx's strtok_r, but it's close enough.
#define strtok_r strtok_s
#endif

	// Advertise processor flags.
	const char * pflags = sysapi_processor_flags();
	if (pflags) {
		char * savePointer = NULL;
		char * processor_flags = strdup( pflags );
		char * flag = strtok_r( processor_flags, " ", & savePointer );
		std::string attributeName;
		while( flag != NULL ) {
			formatstr( attributeName, "has_%s", flag );
			cp->Assign( attributeName.c_str(), true );
			flag = strtok_r( NULL, " ", & savePointer );
		}
		free( processor_flags );
	}
}

void
MachAttributes::start_benchmarks( Resource* rip, int &count )
{
	count = 0;
	ClassAd* cp = rip->r_classad;

	if( rip->isSuspendedForCOD() ) {
			// if there's a COD job, we definitely don't want to run
			// benchmarks
		return;
	}

	// This should be a bool, but EvalBool() expects an int
	int run_benchmarks = 0;
	if ( cp->EvalBool( ATTR_RUN_BENCHMARKS, cp, run_benchmarks ) == 0 ) {
		run_benchmarks = 0;
	}
	if ( !run_benchmarks ) {
		return;
	}

	dprintf( D_ALWAYS,
			 "State change: %s is TRUE\n", ATTR_RUN_BENCHMARKS );

	if(  (rip->state() != unclaimed_state)  &&
		 (rip->activity() != idle_act)      ) {
		dprintf( D_ALWAYS,
				 "Tried to run benchmarks when not idle, aborting.\n" );
		return;
	}

	ASSERT( bench_job_mgr != NULL );

	// Enter benchmarking activity BEFORE invoking StartBenchmarks().
	// If StartBenchmarks() will return to idle activity upon failure
	// to launch benchmarks, or upon completion of benchmarks 
	// (in the reaper).
	rip->change_state( benchmarking_act );
	bench_job_mgr->StartBenchmarks( rip, count );
	// However, if StartBenchmarks set count to zero, that means
	// there are no benchmarks configured to run now. So set the activity
	// back to idle.
	if ( count == 0 ) {
		rip->change_state( idle_act );
	}

}

void
MachAttributes::benchmarks_finished( Resource* rip )
{
	// Update all ClassAds with this new value for LastBenchmark.
	m_last_benchmark = time(NULL);
	resmgr->refresh_benchmarks();

	dprintf( D_ALWAYS, "State change: benchmarks completed\n" );
	if( rip->activity() == benchmarking_act ) {
		rip->change_state( idle_act );
	}
}

#if defined(WIN32)
void
MachAttributes::credd_test()
{
	// Attempt to perform a NOP on our CREDD_HOST. This will test
	// our ability to authenticate with DAEMON-level auth, and thus
	// fetch passwords. If we succeed, we'll advertise the CREDD_HOST

	char *credd_host = param("CREDD_HOST");

	if (credd_host == NULL) {
		if (m_local_credd != NULL) {
			free(m_local_credd);
			m_local_credd = NULL;
		}
		return;
	}

	if (m_local_credd != NULL) {
		if (strcmp(m_local_credd, credd_host) == 0) {
			free(credd_host);
			return;
		}
		else {
			free(m_local_credd);
			m_local_credd = NULL;
			m_last_credd_test = 0;
		}
	}

	time_t now = time(NULL);
	double thresh = (double)param_integer("CREDD_TEST_INTERVAL", 300);
	if (difftime(now, m_last_credd_test) > thresh) {
		Daemon credd(DT_CREDD);
		if (credd.locate()) {
			Sock *sock = credd.startCommand(CREDD_NOP, Stream::reli_sock, 20);
			if (sock != NULL) {
				sock->decode();
				if (sock->end_of_message()) {
					m_local_credd = credd_host;
				}
			}
		}
		m_last_credd_test = now;
	}
	if (credd_host != m_local_credd) {
		free(credd_host);
	}
}
#endif

CpuAttributes::CpuAttributes( MachAttributes* map_arg, 
							  int slot_type,
							  double num_cpus_arg,
							  int num_phys_mem,
							  double virt_mem_fraction,
							  double disk_fraction,
							  const slotres_map_t& slotres_map,
							  MyString &execute_dir,
							  MyString &execute_partition_id )
{
	map = map_arg;
	c_type = slot_type;
	c_num_slot_cpus = c_num_cpus = num_cpus_arg;
	c_allow_fractional_cpus = num_cpus_arg > 0 && num_cpus_arg < 0.9;
	c_slot_mem = c_phys_mem = num_phys_mem;
	c_virt_mem_fraction = virt_mem_fraction;
	c_disk_fraction = disk_fraction;
    c_slotres_map = slotres_map;
    c_slottot_map = slotres_map;
	c_execute_dir = execute_dir;
	c_execute_partition_id = execute_partition_id;
	c_idle = -1;
	c_console_idle = -1;
	c_slot_disk = c_disk = 0;
	c_total_disk = 0;

	c_condor_load = -1.0;
	c_owner_load = -1.0;
	c_virt_mem = 0;
	rip = NULL;
}

void
CpuAttributes::attach( Resource* res_ip )
{
	this->rip = res_ip;
}


void
CpuAttributes::bind_DevIds(int slot_id, int slot_sub_id) // bind non-fungable resource ids to a slot
{
	if ( ! map)
		return;

	for (slotres_map_t::iterator j(c_slotres_map.begin());  j != c_slotres_map.end();  ++j) {
		int cAssigned = int(j->second);

		// if this resource already has bound ids, don't bind again.
		slotres_devIds_map_t::const_iterator m(c_slotres_ids_map.find(j->first));
		if (m != c_slotres_ids_map.end() && cAssigned == (int)m->second.size())
			continue;

		slotres_devIds_map_t::const_iterator k(map->machres_devIds().find(j->first));
		if (k != map->machres_devIds().end()) {
			for (int ii = 0; ii < cAssigned; ++ii) {
				const char * id = map->AllocateDevId(j->first, slot_id, slot_sub_id);
				if ( ! id) {
					EXCEPT("Failed to bind local resource '%s' \n", j->first.c_str());
				} else {
					c_slotres_ids_map[j->first].push_back(id);
				}
			}
		}
	}

}

void
CpuAttributes::unbind_DevIds(int slot_id, int slot_sub_id) // release non-fungable resource ids
{
	if ( ! map) return;
	if ( ! slot_sub_id) return;

	for (slotres_map_t::iterator j(c_slotres_map.begin());  j != c_slotres_map.end();  ++j) {
		slotres_devIds_map_t::const_iterator k(c_slotres_ids_map.find(j->first));
		if (k != c_slotres_ids_map.end()) {
			slotres_assigned_ids_t & ids = c_slotres_ids_map[j->first];
			while ( ! ids.empty()) {
				map->ReleaseDynamicDevId(j->first, ids.back().c_str(), slot_id, slot_sub_id);
				ids.pop_back();
			}
		}
	}
}

void
CpuAttributes::publish( ClassAd* cp, amask_t how_much )
{
	if( IS_UPDATE(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_VIRTUAL_MEMORY, c_virt_mem );

		cp->Assign( ATTR_TOTAL_DISK, c_total_disk );

		cp->Assign( ATTR_DISK, c_disk );
		
	}

	if( IS_TIMEOUT(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_CONDOR_LOAD_AVG, rint(c_condor_load * 100) / 100.0 );

		cp->Assign( ATTR_LOAD_AVG, rint((c_owner_load + c_condor_load) * 100) / 100.0 );

		cp->Assign( ATTR_KEYBOARD_IDLE, c_idle );
  
			// ConsoleIdle cannot be determined on all platforms; thus, only
			// advertise if it is not -1.
		if( c_console_idle != -1 ) {
			cp->Assign( ATTR_CONSOLE_IDLE, c_console_idle );
		}
	}

	if( IS_STATIC(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_MEMORY, c_phys_mem );
		cp->Assign( ATTR_TOTAL_SLOT_MEMORY, c_slot_mem );
		cp->Assign( ATTR_TOTAL_SLOT_DISK, c_slot_disk );

		if (c_allow_fractional_cpus) {
			cp->Assign( ATTR_CPUS, c_num_cpus );
			cp->Assign( ATTR_TOTAL_SLOT_CPUS, c_num_slot_cpus );
		} else {
			cp->Assign( ATTR_CPUS, (int)(c_num_cpus + 0.1) );
			cp->Assign( ATTR_TOTAL_SLOT_CPUS, (int)(c_num_slot_cpus + 0.1) );
		}
		
		// publish local resource quantities for this slot
		for (slotres_map_t::iterator j(c_slotres_map.begin());  j != c_slotres_map.end();  ++j) {
			cp->Assign(j->first.c_str(), int(j->second));
			string attr = ATTR_TOTAL_SLOT_PREFIX; attr += j->first;
			cp->Assign(attr.c_str(), int(c_slottot_map[j->first]));
			slotres_devIds_map_t::const_iterator k(c_slotres_ids_map.find(j->first));
			if (k != c_slotres_ids_map.end()) {
				attr = "Assigned";
				attr += j->first;
				string ids;
				join(k->second, ",", ids);
				cp->Assign(attr.c_str(), ids.c_str());
			}
		}
	}
}


void
CpuAttributes::compute( amask_t how_much )
{
	double val;

	if( IS_UPDATE(how_much) && IS_SHARED(how_much) ) {

			// Shared attributes that we only get a fraction of
		val = map->virt_mem();
		if (!IS_AUTO_SHARE(c_virt_mem_fraction)) {
			val *= c_virt_mem_fraction;
		}
		c_virt_mem = (unsigned long)floor( val );
	}

	if( IS_TIMEOUT(how_much) && !IS_SHARED(how_much) ) {

		// Dynamic, non-shared attributes we need to actually compute
		c_condor_load = rip->compute_condor_load();

		c_total_disk = sysapi_disk_space(rip->executeDir());
		if (IS_UPDATE(how_much)) {
			dprintf(D_FULLDEBUG, "Total execute space: %lu\n", c_total_disk);
		}

		val = c_total_disk * c_disk_fraction;
		c_disk = (long long)floor(val);
		if (0 == (long long)c_slot_disk)
		{
		  // only use the 1st compute ignore subsequent.
		  c_slot_disk = c_disk; 
		}
	}	
}

void
CpuAttributes::display( amask_t how_much )
{
	if( IS_UPDATE(how_much) ) {
		dprintf( D_KEYBOARD, 
				 "Idle time: %s %-8d %s %d\n",  
				 "Keyboard:", (int)c_idle, 
				 "Console:", (int)c_console_idle );

		dprintf( D_LOAD, 
				 "%s %.2f  %s %.2f  %s %.2f\n",  
				 "SystemLoad:", c_condor_load + c_owner_load,
				 "CondorLoad:", c_condor_load,
				 "OwnerLoad:", c_owner_load );
	} else {
		if( IsDebugVerbose( D_LOAD ) ) {
			dprintf( D_LOAD | D_VERBOSE,
					 "%s %.2f  %s %.2f  %s %.2f\n",  
					 "SystemLoad:", c_condor_load + c_owner_load,
					 "CondorLoad:", c_condor_load,
					 "OwnerLoad:", c_owner_load );
		}
		if( IsDebugVerbose( D_KEYBOARD ) ) {
			dprintf( D_KEYBOARD | D_VERBOSE,
					 "Idle time: %s %-8d %s %d\n",  
					 "Keyboard:", (int)c_idle, 
					 "Console:", (int)c_console_idle );
		}
	}
}	


void
CpuAttributes::show_totals( int dflag )
{
	::dprintf( dflag | D_NOHEADER, 
			 "slot type %d: " , c_type);

	
	if (IS_AUTO_SHARE(c_num_cpus)) {
		::dprintf( dflag | D_NOHEADER, 
			 "Cpus: auto");
	} else {
		::dprintf( dflag | D_NOHEADER, 
			 "Cpus: %f", c_num_cpus);
	}

	if (IS_AUTO_SHARE(c_phys_mem)) {
		::dprintf( dflag | D_NOHEADER, 
				   ", Memory: auto" );
	}
	else {
		::dprintf( dflag | D_NOHEADER, 
				   ", Memory: %d",c_phys_mem );
	}
	

	if( IS_AUTO_SHARE(c_virt_mem_fraction) ) {
		::dprintf( dflag | D_NOHEADER, 
				   ", Swap: auto" );
	}
	else {
		::dprintf( dflag | D_NOHEADER, 
				   ", Swap: %.2f%%", 100*c_virt_mem_fraction );
	}
	

	if( IS_AUTO_SHARE(c_disk_fraction) ) {
		::dprintf( dflag | D_NOHEADER, 
				   ", Disk: auto" );
	}
	else {
		::dprintf( dflag | D_NOHEADER, 
				   ", Disk: %.2f%%", 100*c_disk_fraction );
	}

    for (slotres_map_t::iterator j(c_slotres_map.begin());  j != c_slotres_map.end();  ++j) {
        if (IS_AUTO_SHARE(j->second)) {
            ::dprintf(dflag | D_NOHEADER, ", %s: auto", j->first.c_str()); 
        } else {
            ::dprintf(dflag | D_NOHEADER, ", %s: %d", j->first.c_str(), int(j->second));
        }
    }

    ::dprintf(dflag | D_NOHEADER, "\n");
}


void
CpuAttributes::dprintf( int flags, const char* fmt, ... )
{
    if (NULL == rip) {
        EXCEPT("CpuAttributes::dprintf() called with NULL resource pointer");
    }
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}

void
CpuAttributes::swap_attributes(CpuAttributes & attra, CpuAttributes & attrb, int flags)
{
	if (flags & 1) {
		// swap execute directories.
		MyString str = attra.c_execute_dir;
		attra.c_execute_dir = attrb.c_execute_dir;
		attrb.c_execute_dir = str;

		// swap execute partition ids
		str = attra.c_execute_partition_id;
		attra.c_execute_partition_id = attrb.c_execute_partition_id;
		attrb.c_execute_partition_id = str;

		// swap total disk since we swapped partition ids
		long long ll = attra.c_total_disk;
		attra.c_total_disk = attrb.c_total_disk;
		attrb.c_total_disk = ll;
	}
}


CpuAttributes&
CpuAttributes::operator+=( CpuAttributes& rhs )
{
	c_num_cpus += rhs.c_num_cpus;
	c_phys_mem += rhs.c_phys_mem;
	if (!IS_AUTO_SHARE(rhs.c_virt_mem_fraction) &&
		!IS_AUTO_SHARE(c_virt_mem_fraction)) {
		c_virt_mem_fraction += rhs.c_virt_mem_fraction;
	}
	if (!IS_AUTO_SHARE(rhs.c_disk_fraction) &&
		!IS_AUTO_SHARE(c_disk_fraction)) {
		c_disk_fraction += rhs.c_disk_fraction;
	}

    for (slotres_map_t::iterator j(c_slotres_map.begin());  j != c_slotres_map.end();  ++j) {
        j->second += rhs.c_slotres_map[j->first];
    }

	compute( A_TIMEOUT | A_UPDATE ); // Re-compute

	return *this;
}

CpuAttributes&
CpuAttributes::operator-=( CpuAttributes& rhs )
{
	c_num_cpus -= rhs.c_num_cpus;
	c_phys_mem -= rhs.c_phys_mem;
	if (!IS_AUTO_SHARE(rhs.c_virt_mem_fraction) &&
		!IS_AUTO_SHARE(c_virt_mem_fraction)) {
		c_virt_mem_fraction -= rhs.c_virt_mem_fraction;
	}
	if (!IS_AUTO_SHARE(rhs.c_disk_fraction) &&
		!IS_AUTO_SHARE(c_disk_fraction)) {
		c_disk_fraction -= rhs.c_disk_fraction;
	}

    for (slotres_map_t::iterator j(c_slotres_map.begin());  j != c_slotres_map.end();  ++j) {
        j->second -= rhs.c_slotres_map[j->first];
    }

	compute( A_TIMEOUT | A_UPDATE ); // Re-compute

	return *this;
}

AvailAttributes::AvailAttributes( MachAttributes* map ):
	m_execute_partitions(500,MyStringHash,updateDuplicateKeys)
{
	a_num_cpus = map->num_cpus();
	a_num_cpus_auto_count = 0;
	a_phys_mem = map->phys_mem();
	a_phys_mem_auto_count = 0;
	a_virt_mem_fraction = 1.0;
	a_virt_mem_auto_count = 0;
    a_slotres_map = map->machres();
    for (slotres_map_t::iterator j(a_slotres_map.begin());  j != a_slotres_map.end();  ++j) {
        a_autocnt_map[j->first] = 0;
    }
}

AvailDiskPartition &
AvailAttributes::GetAvailDiskPartition(MyString const &execute_partition_id)
{
	AvailDiskPartition *a = NULL;
	if( m_execute_partitions.lookup(execute_partition_id,a) < 0 ) {
			// No entry found for this partition.  Create one.
		m_execute_partitions.insert( execute_partition_id, AvailDiskPartition() );
		m_execute_partitions.lookup(execute_partition_id,a);
		ASSERT(a);
	}
	return *a;
}

bool
AvailAttributes::decrement( CpuAttributes* cap ) 
{
	int new_cpus, new_phys_mem;
	double new_virt_mem, new_disk, floor = -0.000001f;
	
	new_cpus = a_num_cpus;
	if ( ! IS_AUTO_SHARE(cap->c_num_cpus)) {
		if (cap->c_allow_fractional_cpus && (cap->c_num_cpus < .5)) {
			//PRAGMA_REMIND("TJ: account for fractional cpus properly...")
		} else {
			new_cpus -= cap->c_num_cpus;
		}
	}

	new_phys_mem = a_phys_mem;
	if ( ! IS_AUTO_SHARE(cap->c_phys_mem)) {
		new_phys_mem -= cap->c_phys_mem;
	}

	new_virt_mem = a_virt_mem_fraction;
	if( !IS_AUTO_SHARE(cap->c_virt_mem_fraction) ) {
		new_virt_mem -= cap->c_virt_mem_fraction;
	}

	AvailDiskPartition &partition = GetAvailDiskPartition( cap->executePartitionID() );
	new_disk = partition.m_disk_fraction;
	if( !IS_AUTO_SHARE(cap->c_disk_fraction) ) {
		new_disk -= cap->c_disk_fraction;
	}

    bool resfloor = false;
    slotres_map_t new_res(a_slotres_map);
    for (slotres_map_t::iterator j(new_res.begin());  j != new_res.end();  ++j) {
        if (!IS_AUTO_SHARE(cap->c_slotres_map[j->first])) {
            j->second -= cap->c_slotres_map[j->first];
        }
        if (j->second < floor) { 
            resfloor = true;
        }
    }

	if (resfloor || new_cpus < floor || new_phys_mem < floor || 
		new_virt_mem < floor || new_disk < floor) {
	    return false;
    }

    a_num_cpus = new_cpus;
    if (IS_AUTO_SHARE(cap->c_num_cpus)) {
        a_num_cpus_auto_count += 1;
    }

    a_phys_mem = new_phys_mem;
    if (IS_AUTO_SHARE(cap->c_phys_mem)) {
        a_phys_mem_auto_count += 1;
    }

    a_virt_mem_fraction = new_virt_mem;
    if( IS_AUTO_SHARE(cap->c_virt_mem_fraction) ) {
        a_virt_mem_auto_count += 1;
    }

    partition.m_disk_fraction = new_disk;
    if( IS_AUTO_SHARE(cap->c_disk_fraction) ) {
        partition.m_auto_count += 1;
    }

    for (slotres_map_t::iterator j(a_slotres_map.begin());  j != a_slotres_map.end();  ++j) {
        j->second = new_res[j->first];
        if (IS_AUTO_SHARE(cap->c_slotres_map[j->first])) {
            a_autocnt_map[j->first] += 1;
        }
    }

	return true;
}

// in order to consume all of a user-defined resource and distribute it among slots,
// we have to keep track of how many slots have auto shares and how much of a resource
// remains that is to be used by auto shares.  To do this, we need to capture the
// auto capacity and slot count into variables that can be decremented as we hand out
// auto shares in the function below this one.
//
bool AvailAttributes::computeRemainder(slotres_map_t & remain_cap, slotres_map_t & remain_cnt)
{
	remain_cap.clear();
	remain_cnt.clear();
	for (slotres_map_t::iterator j(a_slotres_map.begin());  j != a_slotres_map.end();  ++j) {
		remain_cap[j->first] = j->second;
	}
	for (slotres_map_t::iterator j(a_autocnt_map.begin());  j != a_autocnt_map.end();  ++j) {
		remain_cnt[j->first] = j->second;
	}
	return true;
}

// hand out cpu, mem, disk, swap and user defined resource auto shares
// user defined resource auto shares are handed out from the count of resources
// and auto sharing slots passed in.
// Note: that remain_cap and remain_cnt are modified by this method,
//  the initial value of remain_cap and remain_cnt are calculated by the computeRemainder
// method of this class.  It is the callers responsiblity to call computerRemaineder and
// to pass the result into this function.
//
bool
AvailAttributes::computeAutoShares( CpuAttributes* cap, slotres_map_t & remain_cap, slotres_map_t & remain_cnt)
{
	if (IS_AUTO_SHARE(cap->c_num_cpus)) {
		ASSERT( a_num_cpus_auto_count > 0 );
		double new_value = a_num_cpus / a_num_cpus_auto_count;
		if (cap->c_allow_fractional_cpus) {
			if ( new_value <= 0.0)
				return false;
		} else if( new_value < 1 ) {
			return false;
		}
		cap->c_num_slot_cpus = cap->c_num_cpus = new_value;
	}

	if (IS_AUTO_SHARE(cap->c_phys_mem)) {
		ASSERT( a_phys_mem_auto_count > 0 );
		int new_value = a_phys_mem / a_phys_mem_auto_count;
		if( new_value < 1 ) {
			return false;
		}
		cap->c_slot_mem = cap->c_phys_mem = new_value;
	}

	if( IS_AUTO_SHARE(cap->c_virt_mem_fraction) ) {
		ASSERT( a_virt_mem_auto_count > 0 );
		double new_value = a_virt_mem_fraction / a_virt_mem_auto_count;
		if( new_value <= 0 ) {
			return false;
		}
		cap->c_virt_mem_fraction = new_value;
	}

	if( IS_AUTO_SHARE(cap->c_disk_fraction) ) {
		AvailDiskPartition &partition = GetAvailDiskPartition( cap->c_execute_partition_id );
		ASSERT( partition.m_auto_count > 0 );
		double new_value = partition.m_disk_fraction / partition.m_auto_count;
		if( new_value <= 0 ) {
			return false;
		}
		cap->c_disk_fraction = new_value;
	}

	for (slotres_map_t::iterator j(a_slotres_map.begin());  j != a_slotres_map.end();  ++j) {
		if (IS_AUTO_SHARE(cap->c_slotres_map[j->first])) {
			// replace auto share vals with final values:
			// if there are less than 1 of the resource remaining, hand out 0
			ASSERT(a_autocnt_map[j->first] > 0) // since this is auto, we expect that the count of autos is at least 1
			if (j->second < 1) {
				cap->c_slotres_map[j->first] = 0;
			} else {
				#if 1
				// distribute integral values evenly among slots, with excess going to first slots(s)
				long long new_value = (long long)ceil(remain_cap[j->first] / remain_cnt[j->first]);
				#else
				// distribute integral values evenly among slots with excess being unused
				int new_value = int(j->second / a_autocnt_map[j->first]);
				#endif
				if (new_value < 0) return false;
				cap->c_slotres_map[j->first] = new_value;
				remain_cap[j->first] -= cap->c_slotres_map[j->first];
			}
			remain_cnt[j->first] -= 1;
		}
		// save off slot resource totals once we have final value:
		cap->c_slottot_map[j->first] = cap->c_slotres_map[j->first];
	}

	return true;
}


void
AvailAttributes::show_totals( int dflag, CpuAttributes *cap )
{
	AvailDiskPartition &partition = GetAvailDiskPartition( cap->c_execute_partition_id );
	::dprintf( dflag | D_NOHEADER, 
			 "Slot #%d: Cpus: %d, Memory: %d, Swap: %.2f%%, Disk: %.2f%%",
			 cap->c_type, a_num_cpus, a_phys_mem, 100*a_virt_mem_fraction,
			 100*partition.m_disk_fraction );
    for (slotres_map_t::iterator j(a_slotres_map.begin());  j != a_slotres_map.end();  ++j) {
        ::dprintf(dflag | D_NOHEADER, ", %s: %d", j->first.c_str(), int(j->second));
    }
    ::dprintf(dflag | D_NOHEADER, "\n");
}
