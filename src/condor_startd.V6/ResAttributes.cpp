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
#include "classad_helpers.h" // for cleanStringForUseAsAttr
#include "../condor_sysapi/sysapi.h"

#include <set>
#include <algorithm>
#include <sstream>
#include <charconv>

#ifdef WIN32
#include "winreg.windows.h"
#endif

#include "docker-api.h"

MachAttributes::MachAttributes()
   : m_user_settings_init(false), m_named_chroot()
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

	m_clock_day = -1;
	m_clock_min = -1;
	m_condor_load = -1.0;
	m_console_idle = 0;
	m_idle = 0;
	m_load = -1.0;
	m_owner_load = -1.0;
	m_virt_mem = 0;
	m_docker_cached_image_size = -1;
	m_docker_cached_image_size_time = time(nullptr) - docker_cached_image_size_interval;

		// Number of CPUs.  Since this is used heavily by the ResMgr
		// instantiation and initialization, we need to have a real
		// value for this as soon as the MachAttributes object exists.
	bool count_hyper = param_boolean("COUNT_HYPERTHREAD_CPUS", true);
	int ncpus=0, nhyper_cpus=0;
	sysapi_ncpus_raw(&ncpus, &nhyper_cpus);
	m_num_real_cpus = count_hyper ? nhyper_cpus : ncpus;
	m_num_cpus = param_integer("NUM_CPUS");

	// recompute disk is false if DISK is configured to a static value
	// otherwise we recompute based on a knob. the actual live disk computation
	// happens in the Resource class if recompute_disk is true
	m_always_recompute_disk = false;
	m_total_disk = -1;
	auto_free_ptr disk(param("DISK"));
	if (!disk || ! string_is_long_param(disk, m_total_disk)) {
		m_always_recompute_disk = param_boolean("STARTD_RECOMPUTE_DISK_FREE", false);
	}

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

	// temporary attributes for raw utsname info
	m_utsname_sysname = NULL;
	m_utsname_nodename = NULL;
	m_utsname_release = NULL;
	m_utsname_version = NULL;
	m_utsname_machine = NULL;


#if defined ( WIN32 )
#pragma warning(push)
#pragma warning(disable : 4996)
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
#pragma warning(pop)

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

	if( m_utsname_sysname ) free( m_utsname_sysname );
	if( m_utsname_nodename ) free( m_utsname_nodename );
	if( m_utsname_release ) free( m_utsname_release );
	if( m_utsname_version ) free( m_utsname_version );
	if( m_utsname_machine ) free( m_utsname_machine );

	for (auto *val : m_lst_dynamic) {
       if (val) free (val);
	}
	m_lst_dynamic.clear();

	for (auto *val: m_lst_static) {
       if (val) free (val);
    }
	m_lst_static.clear();

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

	for (auto *val : m_lst_dynamic) {
        if (val) free (val);
	}
	m_lst_dynamic.clear();

	for (auto *val: m_lst_static) {
	    if (val) free (val);
	}
	m_lst_static.clear();

	m_user_specified.clear();

#ifdef WIN32
	char * pszParam = NULL;
	pszParam = param("STARTD_PUBLISH_WINREG");
	if (pszParam)
    {
		m_user_specified = split(pszParam, ";");
		free(pszParam);
	}
#endif

	for (const auto& pszItem: m_user_specified)
    {
		// if the reg_item is of the form attr_name=reg_path;
		// then skip over the attr_name and '=' and trailing
		// whitespace.  But if the = is after the first \, then
		// it's a part of the reg_path, so ignore it.
		//
		const char * pkey = strchr(pszItem.c_str(), '=');
		const char * pbs  = strchr(pszItem.c_str(), '\\');
		if (pkey && ( ! pbs || pkey < pbs)) {
			++pkey; // skip the '='
		} else {
			pkey = pszItem.c_str();
		}

		// skip any leading whitespace in the key
		while (isspace(pkey[0]))
			++pkey;

       #ifdef WIN32
		char * pszAttr = generate_reg_key_attr_name("WINREG_", pszItem.c_str());

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
		        m_lst_dynamic.emplace_back(pav);
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
				m_lst_static.emplace_back(pav);
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
	this->compute_config();
	this->compute_for_update();
	this->compute_for_policy();
}

void
MachAttributes::compute_config()
{
	// the startd doesn't normally call the init() method (bug?),
	// it just starts calling compute, so in order to gurantee that
	// init happens, we put check here to see if user settings have been initialized
	if ( ! m_user_settings_init)
		init_user_settings();

	{ // formerly IS_STATIC(how_much) && IS_SHARED(how_much)

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
}

void
MachAttributes::compute_for_update()
{

	{  // formerly IS_UPDATE(how_much) && IS_SHARED(how_much)

		m_virt_mem = sysapi_swap_space();
		dprintf( D_FULLDEBUG, "Swap space: %lld\n", m_virt_mem );

		time_t now = resmgr->now();
		time_t interval = (now - m_docker_cached_image_size_time);
		if (docker_cached_image_size_interval && (interval >= docker_cached_image_size_interval)) {
			m_docker_cached_image_size_time = now;
			int64_t size_in_bytes = DockerAPI::imageCacheUsed();
			if (size_in_bytes >= 0) {
				const int64_t ONEMB =  (int64_t)1024 * 1024;
				m_docker_cached_image_size = (DockerAPI::imageCacheUsed() + ONEMB/2) / ONEMB;
			} else {
				// negative values returned above indicate failure to fetch the imageSize
				// negative values here suppress advertise of the attribute
				m_docker_cached_image_size = -1;
			}
		}

#if defined(WIN32)
		credd_test();
#endif
	}
}

void
MachAttributes::compute_for_policy()
{
	{ // formerly IS_TIMEOUT(how_much) && IS_SHARED(how_much)
		m_load = sysapi_load_avg();


		sysapi_idle_time( &m_idle, &m_console_idle );

		//dprintf(D_ALWAYS | D_BACKTRACE, "MachAttributes::compute_for_policy idle=%d\n", m_idle);

		time_t my_timer;
		struct tm *the_time;
		time( &my_timer );
		the_time = localtime(&my_timer);
		m_clock_min = (the_time->tm_hour * 60) + the_time->tm_min;
		m_clock_day = the_time->tm_wday;

		if (m_last_keypress < my_timer - m_idle) {
			if (m_idle_interval >= 0) {
				time_t duration = my_timer - m_last_keypress;
				if (duration > m_idle_interval) {
					if (m_seen_keypress) {
						dprintf(D_IDLE, "end idle interval of %lld sec.\n",
								(long long)duration);
					} else {
						dprintf(D_IDLE,
								"first keyboard event %lld sec. after startup\n",
								(long long)duration);
					}
				}
			}
			m_last_keypress = my_timer;
			m_seen_keypress = true;
		}

#ifdef WIN32
        update_all_WinPerf_results();
#endif

		for (auto *pav : m_lst_dynamic) {
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
}

// Check to see if the device at index ixid for resource tag matches
// the given constraint.  Constraint can be null, in which case everything matches
// Constraint must otherwise be an expression.  If the expression is 
//   a bare number then it is presumed to be the matching dev index.
//   a bare string is presumed to be the matching devid
//   an expresion is evaluated against the dev property classad
//
bool MachAttributes::DevIdMatches(
	const NonFungibleType & nft,
	int ixid,
	ConstraintHolder & require)
{
	if (require.empty()) return true;

	// special case if requirement is a bare integer, the integer is the
	// index into the assigned_ids vector for the given resource tag
	long long lval = -1;
	if (ExprTreeIsLiteralNumber(require.Expr(), lval) && lval == ixid) {
		return true;
	}

	if (ixid < 0 || ixid >= (int)nft.ids.size()) return false;
	NonFungibleRes nfr = nft.ids[ixid];

	// special case if requirement is a string, then it matches if it matches the resource id
	const char * cstr = NULL;
	if (ExprTreeIsLiteralString(require.Expr(), cstr) && YourStringNoCase(cstr) == nfr.id) {
		return true;
	}

	// the general case, evaluate the constraint expression against the properties of the resource
	auto it(nft.props.find(nfr.id));
	if (it != nft.props.end()) {
		ClassAd & ad = const_cast<ClassAd&>(it->second);
		if (EvalExprBool(&ad, require.Expr())) {
			return true;
		}
	}

	return false;
}

// Allocate a user-defined resource deviceid to a slot
// when assign_to_sub > 0, then assign an unused resource from
// slot assigned_to to it's child dynamic slot assign_to_sub.
//
const char * MachAttributes::AllocateDevId(const std::string & tag, const char * request, int assign_to, int assign_to_sub, bool backfill)
{
	if ( ! assign_to) return NULL;

	auto & offline_ids(m_machres_runtime_offline_ids_map[tag]);

	auto found(m_machres_nft_map.find(tag));
	if (found != m_machres_nft_map.end()) {
		auto &[restag, nft] = *found;

		ConstraintHolder require;
		if (request) { require.set(strdup(request)); }

		// when the sub-id is positive, that means we are binding a dynamic slot
		// in that case we want to use only dev-ids that are already owned by the partitionable slot.
		// but not yet assigned to any of  it's dynamic children.
		//
		int cur_id = (assign_to_sub > 0) ? assign_to : 0;
		if (backfill) {
			// bind backfill resources in reverse order to minimize the conflicts with primary slot usage
			for (int ixid = (int)nft.ids.size()-1; ixid >= 0; --ixid) {
				NonFungibleRes & nfr = nft.ids[ixid];
				if (offline_ids.count(nfr.id) > 0) continue; // don't bind offline ids
				// don't bind to a backfill d-slot an id that a primary d-slot is using
				// TODO: handle the case of a static primary slot that is claimed
				if (assign_to_sub > 0 && nfr.owner.dyn_id > 0) continue;
				if (nfr.bkowner.id == cur_id && nfr.bkowner.dyn_id == 0 && DevIdMatches(nft, ixid, require)) {
					nfr.bkowner.id = assign_to;
					nfr.bkowner.dyn_id = assign_to_sub;
					return nfr.id.c_str();
				}
			}
		} else {
			for (int ixid = 0; ixid < (int)nft.ids.size(); ++ixid) {
				NonFungibleRes & nfr = nft.ids[ixid];
				if (offline_ids.count(nfr.id) > 0) continue;  // don't bind offline ids
				if (nfr.owner.id == cur_id && nfr.owner.dyn_id == 0 && DevIdMatches(nft, ixid, require)) {
					nfr.owner.id = assign_to;
					nfr.owner.dyn_id = assign_to_sub;
					return nfr.id.c_str();
				}
			}
		}
	}
	return NULL;
}

bool MachAttributes::ReleaseDynamicDevId(const std::string & tag, const char * id, int was_assigned_to, int was_assign_to_sub, int new_sub)
{
	auto found(m_machres_nft_map.find(tag));
	if (found != m_machres_nft_map.end()) {
		auto &[restag, nft] = *found;

		for (int ixid = 0; ixid < (int)nft.ids.size(); ++ixid) {
			NonFungibleRes & nfr = nft.ids[ixid];
			if (nfr.id == id) {
				if (nfr.owner.id == was_assigned_to && nfr.owner.dyn_id == was_assign_to_sub) {
					nfr.owner.dyn_id = new_sub;
					return true;
				}
				if (nfr.bkowner.id == was_assigned_to && nfr.bkowner.dyn_id == was_assign_to_sub) {
					nfr.bkowner.dyn_id = new_sub;
					return true;
				}
			}
		}
	}
	return false;
}


// log level for custom resource device id (e.g. CUDA0) management
static int d_log_devids = D_FULLDEBUG;

const char * MachAttributes::DumpDevIds(std::string & buf, const char * tag, const char * sep)
{
	for (auto &[restag, nft] : m_machres_nft_map) {
		// if a tag was provided, ignore entries that don't match it.
		if (tag && strcasecmp(tag, restag.c_str())) continue;

		const std::set<std::string> & offline_ids = m_machres_runtime_offline_ids_map[restag];
		buf += restag;
		buf += ":{";
		for (auto & nfr : nft.ids) {
			if (offline_ids.count(nfr.id)) buf += "!";
			buf += nfr.id;
			formatstr_cat(buf, "=%d", nfr.owner.id);
			if (nfr.owner.dyn_id) formatstr_cat(buf, "_%d", nfr.owner.dyn_id);
			if (nfr.bkowner.id) {
				formatstr_cat(buf, "+%d", nfr.bkowner.id);
				if (nfr.bkowner.dyn_id) formatstr_cat(buf, "_%d", nfr.bkowner.dyn_id);
			}
			buf += ", ";
		}
		buf += "}";
		if (tag && sep) buf += sep;
	}
	return buf.c_str();
}

// This is a bit complicated, because we actually keep two sets of offline ids.
// one that is set on startup from the resource ads (m_machres_offline_devIds_map) 
// and another that is set on reconfig (m_machres_runtime_offline_ids_map)
// The first one is a map of resource tag to vector of string devids, the vector may contain duplicates:   map[GPUs]={CUDA0, CUDA0, CUDA1}
// the second is a map of resource tag to a set of string unique string devids, map[GPUs][CUDA0]
//
// Then there are three collections that show the active devids:
//    m_machres_map  - a map of resource tag to number of active (i.e. non-offline) devids  map[Gpus] = N
//    m_machres_devIds_map  - a map of resource tag to vector of devids that were active at startup. this preserves devid assigment
//    m_machres_devIdOwners_map - a map of resource tag to vector of slot-ids to which they are assigned.  this is a parallel vector to the above vector
//
// When a devid is marked offline at startup, it is put into the m_machres_offline_devIds_map and NOT into the m_machres_devIds_map
// but when a devid is marked offline on reconfig, it is not removed from the m_machres_devIds_map, nor is it added to the m_machres_offline_devIds_map
// instead, it is added to the m_machres_runtime_offline_ids_map, and that map is checked each time we want to make a d-slot.
// The advertised value for the resource Tag (e.g. GPUs) should be the number of items in the m_machres_devIds_map vector that
// are not in the m_machres_runtime_offline_ids_map, and are bound to that slot
//
// Thus a static slot that has an assigned GPU, may have a GPUs value of 0, if that assigned GPU is in the runtime_offline_ids collection.
// This can happen to a slot that is running jobs if a reconfig happens while the job is running.
//
// The job of ReconfigOfflineDevIds is to:
//    build the m_machres_runtime_offline_ids_map, so that is has all of the unique DevIds that are offline
//    Update the m_machres_map quantities so that only active resources are counted
// This does not directly affect the attributes advertised in the slot, but this information is used by RefreshDevIds
// which is called for each slot on reconfig after ReconfigOfflineDevIds is called.
//
void MachAttributes::ReconfigOfflineDevIds()
{
	// we will rebuild this to contain only offline ids that were not offline'd at startup
	m_machres_runtime_offline_ids_map.clear();

	std::string knob;
	std::vector<std::string> offline_ids;
	for (auto &[restag, nft] : m_machres_nft_map) {
		const char * tag = restag.c_str();
		knob = "OFFLINE_MACHINE_RESOURCE_"; knob += restag;
		param_and_insert_unique_items(knob.c_str(), offline_ids);

		// we've got offline resources, check to see if any of them are assigned

		int offline = 0;
		// look through assigned resource list and see if we have offlined any of them.
		for (int ii = (int)nft.ids.size()-1; ii >= 0; --ii) {
			const char * id = nft.ids[ii].id.c_str();
			const FullSlotId & owner = nft.ids[ii].owner;
			if (contains(offline_ids, id)) {
				dprintf(D_ALWAYS, "%s %s is now marked offline\n", tag, id);
				if (owner.dyn_id) {
					// currently assigned to a dynamic slot
					dprintf(D_ALWAYS, "%s %s is currently assigned to slot %d_%d\n",
						tag, id, owner.id, owner.dyn_id);
				} else {
					// currently assigned to a static slot or p-slot
					dprintf(D_ALWAYS, "%s %s is currently assigned to slot %d\n",
						tag, id, owner.id);
				}

				// we found an Assignd<Tag> devid that should be offline,
				// so it needs to be in the dynamic_offline_ids collecton so we can check at runtime
				m_machres_runtime_offline_ids_map[restag].emplace(id);
				++offline;
			}

			// update quantites to count only assigned, non-offline devids
			m_machres_map[restag] = nft.ids.size() - offline;
		}
	}
}

// refresh the assigned ids for this slot, and update the count so that only non-offline ids are counted
// we do this on reconfig, incase we have changed the offline list
int MachAttributes::RefreshDevIds(
	const std::string & tag,
	slotres_assigned_ids_t & devids,
	int assign_to,
	int assign_to_sub)
{
	devids.clear();

	auto found = machres_devIds().find(tag);
	if (found == machres_devIds().end()) {
		return 0;
	}
	auto & offline_ids(m_machres_runtime_offline_ids_map[tag]);

	// for p-slots & static slots, we ignore the sub-id when building the Assigned list (slot_res_devids)
	// but not when counting the number of resources
	// thus in slot1 we can have 
	//   AssignedGPUs = "CUDA0,CUDA1,CUDA2"  (slot_res_devids)
	//   OfflineGPUs = "CUDA2"               (offline_ids)
	//   GPUs = 1                            (slot_res)
	// while in slot1_1 we have 
	//   AssignedGPUs = "CUDA0"
	//   Gpus = 1

	int num_res = 0;
	for (const auto & nfr : found->second.ids) {
		if (nfr.owner.id == assign_to && (assign_to_sub == 0 || nfr.owner.dyn_id == assign_to_sub)) {
			devids.emplace_back(nfr.id);
			if (offline_ids.count(nfr.id)) {
				// don't count this one even though it is assigned
			} else if (nfr.owner.dyn_id == assign_to_sub) {
				num_res += 1;
			}
		}
	}
	return num_res;
}

// return a list of devids that are assigned to the given broken id
int MachAttributes::ReportBrokenDevIds(const std::string & tag, slotres_assigned_ids_t & devids, int broken_sub_id)
{
	devids.clear();

	auto found = machres_devIds().find(tag);
	if (found == machres_devIds().end()) {
		return 0;
	}

	int num_res = 0;
	for (const auto & nfr : found->second.ids) {
		if (nfr.owner.dyn_id == broken_sub_id) {
			devids.emplace_back(nfr.id);
			num_res += 1;
		}
	}
	return num_res;
}


// calculate an aggregate properties classad for the given resource tag from the given resource ids
// this classad will be merged into the slot classad that has those resources assigned.
// We do this to allow negotiator matchmaking against the properties of assigned custom resources
bool MachAttributes::ComputeDevProps(
	ClassAd & ad,
	const std::string & tag,
	const slotres_assigned_ids_t & ids)
{
	std::string attr;
	ad.Clear();
	if (ids.empty()) {
		return false;
	}
	auto found = m_machres_nft_map.find(tag);
	if (found == m_machres_nft_map.end()) {
		return false;
	}
	auto &[restag, nft] = *found;
	if (nft.props.empty()) {
		return false;
	}

	// TODO: move knobs out of this function
	bool nested_props = param_boolean("USE_NESTED_CUSTOM_RESOURCE_PROPS", true);
	bool verbose_nested_props = param_boolean("VERBOSE_NESTED_CUSTOM_RESOURCE_PROPS", nested_props);

	// we begin by assuming that the device 0 props are common to all
	auto ip = nft.props.find(ids.front());
	if (ip != nft.props.end()) {
		ad.Update(ip->second);
		if ( ! verbose_nested_props && ids.size() == 1) {
			return true;
		}
	}

	// build a temporary map of common properties using device 0 properties
	// note that this map does not own the exprtrees it holds.
	std::map<std::string, classad::ExprTree*, classad::CaseIgnLTStr> common;
	for (auto & kvp : ip->second) { common[kvp.first] = kvp.second; }

	// remove items from the common props map that do not have the same value for all devices
	// and build a map of property lists for the non-common properties. like the common list above
	// this map does not own the exptrees it holds
	std::map<std::string, std::vector<classad::ExprTree*>, classad::CaseIgnLTStr> props;
	for (size_t ix = 1; ix < ids.size(); ++ix) {
		ip = nft.props.find(ids[ix]);
		if (ip == nft.props.end()) continue;

		// walk the remaining common props, looking for mis-matches
		// if we find a mismatch, remove it from the common list
		// and reserve space in the array of non-common props
		ClassAd & ad1 = ip->second;
		std::map<std::string, classad::ExprTree*, classad::CaseIgnLTStr>::iterator ct = common.begin();
		while (ct != common.end()) {
			auto et = ct++;
			auto it = ad1.find(et->first);
			if (it != ad1.end() && *it->second == *et->second) {
				// it matches, so it may be a common attribute
			} else {
				// no match, remove from the common list, and reserve space in the props vector
				ad.Delete(et->first);
				props[et->first].reserve(ids.size());
				common.erase(et);
			}
		}

		if (common.empty()) break;
	}

	// to publish verbose nested props, we don't do any duplicate elimination of the nested props
	// so just add a nested ad for each assigned id that is a full copy of
	// the device props ad plus an attribute that is the id itself
	if (verbose_nested_props) {
		for (auto & id : ids) {
			auto ip = nft.props.find(id);
			if (ip == nft.props.end()) {
				continue;
			}
			ClassAd * props_ad = (ClassAd*)ip->second.Copy();
			props_ad->Assign("Id", id);
			std::string attr(id); 
			cleanStringForUseAsAttr(attr, '_');
			ad.Insert(attr, props_ad);
		}
		return true;
	}

	// at this point, the common props map has only props that are the same across all assigned devices
	// and the props map has empty vectors for each of the non-common properties
	// create list or nested ad items for the non-common properties
	// and stuff them into the output classad
	if (nested_props) {
		for (const auto& id : ids) {
			ip = nft.props.find(id);
			if (ip == nft.props.end()) {
				continue;
			}
			if ( ! props.empty()) {
				ClassAd * props_ad = new ClassAd();
				for (auto & propit : props) {
					ClassAd & ad1 = ip->second;
					auto it = ad1.find(propit.first);
					if (it != ad1.end()) {
						props_ad->Insert(propit.first, it->second->Copy());
					}
				}
				if (props_ad->size() <= 0) {
					delete props_ad;
				} else {
					std::string attr(id); 
					cleanStringForUseAsAttr(attr, '_');
					ad.Insert(attr, props_ad);
				}
			}
		}
	} else {
		for (auto & propit : props) {
			for (auto & id : ids) {
				ip = nft.props.find(id);
				if (ip == nft.props.end()) {
					continue;
				}
				ClassAd & ad1 = ip->second;
				auto it = ad1.find(propit.first);
				if (it != ad1.end()) {
					// copy the expr tree, which right now is a pointer into a m_machres_devProps_map ClassAd
					// we will insert the copies into an exprList and insert that into the output ad
					propit.second.push_back(it->second->Copy());
				}
			}
			if ( ! propit.second.empty()) {
				// this transfers ownership of the ExprTrees, which we created as copies of the 
				// original properties for that purpose.
				attr = propit.first; attr += "List";
				ad.Insert(attr, classad::ExprList::MakeExprList(propit.second));
			}
		}
	}
	return true;
}



// res_value is a string that contains either a number, which is the count of a
// fungable resource, or a list of ids of non-fungable resources.
// if res_value contains a list, then ids is set on exit from this function
// otherwise, ids empty.
static double parse_user_resource_config(const char * tag, const char * res_value, std::vector<std::string> & ids)
{
	ids.clear();
	double num = 0;
	char * pend = NULL;
	num = strtod(res_value, &pend);
	if (pend != res_value) { while(isspace(*pend)) { ++pend; } }
	bool is_simple_double = (pend && pend != res_value && !*pend);
	if ( ! is_simple_double) {
		// ok not a simple double, try evaluating it as a classad expression.
		ClassAd ad;
		if (ad.AssignExpr(tag,res_value) && ad.LookupFloat(tag, num)) {
			// it was an expression that evaluated to a double, so it's a simple double after all
			is_simple_double = true;
		} else {
			// not a simple double, so treat it as a list of id's, the number of resources is the number of ids
			for (const auto& item: StringTokenIterator(res_value)) {
				ids.push_back(item);
			}
			num = ids.size();
		}
	}

	return num;
}

// run a script, and take use its output to configure a custom resource of type tag
//
double MachAttributes::init_machine_resource_from_script(const char * tag, const char * script_cmd) {

	ArgList args;
	std::string errors;
	if(!args.AppendArgsV1RawOrV2Quoted(script_cmd, errors)) {
		printf("Can't append cmd %s(%s)\n", script_cmd, errors.c_str());
		return -1;
	}

	double quantity = 0;
	int offline = 0;

	FILE * fp = my_popen(args, "r", FALSE);
	if ( ! fp) {
		//PRAGMA_REMIND("tj: should failure to run a res inventory script really bring down the startd?")
		EXCEPT("Failed to execute local resource '%s' inventory script \"%s\"", tag, script_cmd);
	} else {
		int error = 0;
		bool is_eof = false;
		ClassAd ad;
		int cAttrs = InsertFromFile(fp, ad, is_eof, error);
		if (cAttrs <= 0) {
			if (error) dprintf(D_ALWAYS, "Could not parse ClassAd for local resource '%s' (error %d) assuming quantity of 0\n", tag, error);
		} else {
			std::set<std::string> unique_ids;
			classad::Value value;
			std::string attr(ATTR_OFFLINE_PREFIX); attr += tag;
			std::string res_value;
			std::vector<std::string> offline_ids;
			if (ad.LookupString(attr,res_value)) {
				offline = parse_user_resource_config(tag, res_value.c_str(), offline_ids);
			} else {
				attr = "OFFLINE_MACHINE_RESOURCE_"; attr += tag;
				if (param(res_value, attr.c_str()) && !res_value.empty()) {
					offline = parse_user_resource_config(tag, res_value.c_str(), offline_ids);
				}
			}

			attr = ATTR_DETECTED_PREFIX; attr += tag;
			if (ad.LookupString(attr,res_value)) {
				std::vector<std::string> ids;
				quantity = parse_user_resource_config(tag, res_value.c_str(), ids);
				if ( ! ids.empty()) {
					offline = 0; // we only want to count ids that match the detected list
					for (const auto& id: ids) {
						if (std::ranges::find(offline_ids,id) != offline_ids.end()) {
							unique_ids.insert(id);
							this->m_machres_offline_devIds_map[tag].emplace_back(id);
							++offline;
						} else {
							unique_ids.insert(id);
							this->m_machres_nft_map[tag].ids.emplace_back(id);
						}
					}
				}
			} else if (ad.EvaluateAttr(attr, value) && value.IsNumber(quantity)) {
				// don't need to do anythin more here.
			}

			// find attributes that are nested ads, we want to store those as properties
			// rather than leaving them in the ad
			std::map<std::string, classad::ClassAd*, classad::CaseIgnLTStr> nested;
			for (auto & it : ad) {
				if (it.second->GetKind() == classad::ExprTree::CLASSAD_NODE) {
					nested[it.first] = dynamic_cast<classad::ClassAd*>(it.second);
					dprintf(D_ALWAYS, "machine resource %s has property ad %s=%s\n", tag, it.first.c_str(), ExprTreeToString(it.second));
				}
			}

			if ( ! nested.empty()) {
				// if there is an ad called common, make it the properties for all ids
				// then remove it from the nested ad collection so we don't see it when we iterate
				auto common_it = nested.find("common");
				if (common_it != nested.end()) {
					for (const auto& id : unique_ids) {
						m_machres_nft_map[tag].props[id].Update(*common_it->second);
					}
					ad.Delete(common_it->first);
					nested.erase(common_it);
				}

				// now update properties for each resource that matches one of the nested ads
				// and remove that nested ad from the inventory ad
				std::string resid;
				for (auto & it : nested) {
					if ( ! it.second->LookupString("id", resid)) { resid = it.first; }
					if (unique_ids.count(resid) > 0) {
						m_machres_nft_map[tag].props[resid].Update(*it.second);
						m_machres_nft_map[tag].props[resid].Delete("id");
						ad.Delete(it.first);
					}
				}

				// we are done with the nested ads
				nested.clear();
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

	if( strcasecmp( name, "MACHINE_RESOURCE_NAMES" ) == 0 ) { return true; }

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
		std::vector<std::string> offline_ids;
		std::string off_name("OFFLINE_"); off_name += name;
		std::string my_value;
		if (param(my_value, off_name.c_str()) && !my_value.empty()) {
			offline = parse_user_resource_config(tag, my_value.c_str(), offline_ids);
		}

		// if the param value parses as a double, then we can handle it simply
		std::vector<std::string> ids;
		num = parse_user_resource_config(tag, res_value, ids);
		if ( ! ids.empty()) {
			offline = 0; // we only want to count ids that match the detected list
			for (const auto& id: ids) {
				if (std::ranges::find(offline_ids,id) != offline_ids.end()) {
					pme->m_machres_offline_devIds_map[tag].emplace_back(id);
					++offline;
				} else {
					pme->m_machres_nft_map[tag].ids.emplace_back(id);
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
	m_machres_nft_map.clear();
	m_machres_runtime_offline_ids_map.clear();

	// this may be filled from resource inventory scripts
	m_machres_attr.Clear();

	// If there no are declared GPUs resource config knobs, and STARTD_DETECT_GPUS is non-empty
	// then do default gpu discovery using STARTD_DETECT_GPUS as arguments to condor_gpu_discovery
	bool has_gpus_res = param_defined("MACHINE_RESOURCE_INVENTORY_GPUS") || param_defined("MACHINE_RESOURCE_GPUS");
	if ( ! has_gpus_res) {
		// auto_gpus should be command line arg if defined, but treat setting it to false as a special case
		// to disable default discovery
		auto_free_ptr auto_gpus(param("STARTD_DETECT_GPUS"));
		if (auto_gpus && YourStringNoCase("false") != auto_gpus.ptr() && YourString("0") != auto_gpus.ptr()) {
			if (YourStringNoCase("true") == auto_gpus.ptr() || YourStringNoCase("1") == auto_gpus.ptr()) {
				dprintf(D_ERROR, "invalid configuration : STARTD_DETECT_GPUS=%s  see the manual for correct usage\n", auto_gpus.ptr());
			} else {
				auto_free_ptr cmd_line(expand_param("$(LIBEXEC)/condor_gpu_discovery $(STARTD_DETECT_GPUS)"));
				int num_not_offline = init_machine_resource_from_script("GPUs", cmd_line);
				if (num_not_offline < 0) num_not_offline = 0;
				m_machres_map["GPUs"] = num_not_offline;
			}
		}
	}

	Regex re;
	int errcode = 0, erroffset = 0;
	ASSERT(re.compile("^MACHINE_RESOURCE_[a-zA-Z0-9_]+", &errcode, &erroffset, PCRE2_CASELESS));
	const int iter_options = HASHITER_NO_DEFAULTS; // we can speed up iteration if there are no machine resources in the defaults table.
	foreach_param_matching(re, iter_options, (bool(*)(void*,HASHITER&))MachAttributes::init_machine_resource, this);

	std::vector<std::string> allowed;
	char* allowed_names = param("MACHINE_RESOURCE_NAMES");
	if (allowed_names) {
		// if admin defined MACHINE_RESOURCE_NAMES, then use this list
		// as the source of expected custom machine resources
		allowed = split(allowed_names);
		free(allowed_names);
	}

	std::vector<std::string> disallowed = {"CPU", "CPUS", "DISK", "SWAP", "MEM", "MEMORY", "RAM"};

	std::vector<const char *> tags_to_erase;
	for (auto & it : m_machres_map) {
		const char * tag = it.first.c_str();
		if ( ! allowed.empty() && ! contains_anycase(allowed, tag)) {
			dprintf(D_ALWAYS, "Local machine resource %s is not in MACHINE_RESOURCE_NAMES so it will have no effect\n", tag);
			tags_to_erase.emplace_back(tag);
			continue;
		}
		if ( ! disallowed.empty() && contains_anycase(disallowed, tag)) {
			EXCEPT("fatal error - MACHINE_RESOURCE_%s is invalid, '%s' is a reserved resource name", tag, tag);
			continue;
		}
		dprintf(D_ALWAYS, "Local machine resource %s = %g\n", tag, it.second);
	}
	for( auto i = tags_to_erase.begin(); i != tags_to_erase.end(); ++i ) {
		m_machres_map.erase(*i);
	}
}

void
MachAttributes::final_idle_dprintf() const
{
	if (m_idle_interval >= 0) {
		time_t my_timer = time(nullptr);
		time_t duration = my_timer - m_last_keypress;
		if (duration > m_idle_interval) {
			dprintf(D_IDLE, "keyboard idle for %lld sec. before shutdown\n",
					(long long)duration);
		}
	}
}

#ifdef LINUX

static int cloned_function(void * /*unused */ ) {
	return(0);
}

static bool hasUnprivUserNamespace() {
	static bool firstTime = true;
	static bool hasNamespace = false;
	if (firstTime) {
		firstTime = false;
		// Detect if we support unprivileged user namespaces
		// by cloning with the user namespace flag as non-root.
		// clone returns tid if successful, -1 on error

		double stack[128];  // force alignment
		int r = clone(cloned_function, &stack[126], 
					CLONE_NEWUSER,
					0);
		if (r > 0) {
			int status = 0;
			int wr = waitpid(r, &status, __WCLONE);
			dprintf(D_ALWAYS, "ResAttributes detected we do have unpriv user namespaces (pid was %d: status was %d)\n", wr, status);

			hasNamespace = true;

		} else if (r == 0) { 
			// The child
			exit(0);
		} else {
			dprintf(D_ALWAYS, "ResAttribute detected we don't have unpriv user namespaces:  return is  is %d\n", errno);
		}
	}
	return hasNamespace;
}

enum class rotational_result {
	ROTATIONAL, NOT_ROTATIONAL, UNKNOWN	
};
static rotational_result hasRotationalScratch() {

	std::string executeDir;
	param(executeDir, "EXECUTE");

	struct stat buf;
	int r = stat(executeDir.c_str(), &buf);

	if (r != 0) {
		// Huh.  Just don't know
		return rotational_result::UNKNOWN;
	}
	char maj[8];
	char min[8];
	{
	auto [ptr, ec] = std::to_chars(maj, maj+7, buf.st_dev >> 8);
	*ptr = '\0';
	}
	{
	auto [ptr, ec] = std::to_chars(min, min+7, buf.st_dev & 0xff);
	*ptr = '\0';
	}

	dev_t major_device = buf.st_dev >> 8;

	// major device 0 is memory device, ramdisk or tmpfs
	if (major_device == 0) {
		return rotational_result::NOT_ROTATIONAL; 
	}


	// Example path: /sys/dev/block/253:5/queue/rotational -- contains '0' or '1'
	std::string rotate_path = "/sys/dev/block/";
	rotate_path += maj;
	rotate_path += ':';
	rotate_path += min;
	rotate_path += "/queue/rotational";

	FILE *f = fopen(rotate_path.c_str(), "r");
	char c = '\0';
	if (f) {
		size_t r = fread(&c, 1, 1, f);
		fclose(f);
		if (r != 1) {
			return rotational_result::UNKNOWN;
		}
		if (c == '1') {
			return rotational_result::ROTATIONAL;
		} else {
			return rotational_result::NOT_ROTATIONAL;
		}
	} else {
		return rotational_result::UNKNOWN;
	}
}

#endif

void 
MachAttributes::publish_static(ClassAd* cp)
{
	// STARTD_IP_ADDR
	cp->Assign( ATTR_STARTD_IP_ADDR, daemonCore->InfoCommandSinfulString() );
	cp->Assign( ATTR_UID_DOMAIN, m_uid_domain );
	cp->Assign( ATTR_FILE_SYSTEM_DOMAIN, m_filesystem_domain );
	cp->Assign( ATTR_HAS_IO_PROXY, true );

	// publish OS info
	cp->Assign( ATTR_ARCH, m_arch );
	cp->Assign( ATTR_OPSYS, m_opsys );
	if (m_opsysver) { cp->Assign( ATTR_OPSYSVER, m_opsysver ); }
	cp->Assign( ATTR_OPSYS_AND_VER, m_opsys_and_ver );
	if (m_opsys_major_ver) { cp->Assign( ATTR_OPSYS_MAJOR_VER, m_opsys_major_ver ); }
	if (m_opsys_name) { cp->Assign( ATTR_OPSYS_NAME, m_opsys_name ); }
	if (m_opsys_long_name) { cp->Assign( ATTR_OPSYS_LONG_NAME, m_opsys_long_name ); }
	if (m_opsys_short_name) { cp->Assign( ATTR_OPSYS_SHORT_NAME, m_opsys_short_name ); }
	if (m_opsys_legacy) { cp->Assign( ATTR_OPSYS_LEGACY, m_opsys_legacy ); }

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
	for (auto *pav: m_lst_static) {
		if (pav) pav->AssignToClassAd(cp);
	}

#else
	// temporary attributes for raw utsname info
	cp->Assign( ATTR_UTSNAME_SYSNAME, m_utsname_sysname );
	cp->Assign( ATTR_UTSNAME_NODENAME, m_utsname_nodename );
	cp->Assign( ATTR_UTSNAME_RELEASE, m_utsname_release );
	cp->Assign( ATTR_UTSNAME_VERSION, m_utsname_version );
	cp->Assign( ATTR_UTSNAME_MACHINE, m_utsname_machine );

#endif // defined ( WIN32 )

	// publish resource totals
	cp->Assign( ATTR_TOTAL_CPUS, m_num_cpus );
	cp->Assign( ATTR_TOTAL_MEMORY, m_phys_mem );

	std::string machine_resources = "Cpus Memory Disk Swap";
	// publish any local resources
	for (auto & j : m_machres_map) {
		const char * rname = j.first.c_str();
		std::string attr(ATTR_DETECTED_PREFIX); attr += rname;
		double ipart, fpart = modf(j.second, &ipart);
		if (fpart >= 0.0 && fpart <= 0.0) {
			cp->Assign(attr, (long long)ipart);
		} else {
			cp->Assign(attr, j.second);
		}
		attr = ATTR_TOTAL_PREFIX; attr += rname;
		if (fpart >= 0.0 && fpart <= 0.0) {
			cp->Assign(attr, (long long)ipart);
		} else {
			cp->Assign(attr, j.second);
		}
		machine_resources += " ";
		machine_resources += j.first;
	}
	cp->Assign(ATTR_MACHINE_RESOURCES, machine_resources);

	// Advertise chroot information
	if ( m_named_chroot.size() > 0 ) {
		cp->Assign( "NamedChroot", m_named_chroot );
	}

	// Advertise Docker Volumes
	char *dockerVolumes = param("DOCKER_VOLUMES");
	if (dockerVolumes) {
		for (const auto& volume: StringTokenIterator(dockerVolumes)) {
			std::string attrName = "HasDockerVolume";
			attrName += volume;
			cp->Assign(attrName, true);
		}
		free(dockerVolumes);
	}

	char *dockerNetworks = param("DOCKER_NETWORKS");
	if (dockerNetworks) {
		cp->Assign(ATTR_DOCKER_NETWORKS, dockerNetworks);
		free(dockerNetworks);
	}

#ifdef WIN32
	// window's strtok_s is the 'safe' version of strtok, it's not identical to linx's strtok_r, but it's close enough.
#define strtok_r strtok_s
#endif

	// Advertise processor flags.
	std::string pflags = sysapi_processor_flags()->processor_flags;
	if (!pflags.empty()) {
		char * savePointer = NULL;
		char * processor_flags = strdup( pflags.c_str());
		char * flag = strtok_r( processor_flags, " ", & savePointer );
		std::string attributeName;
		while( flag != NULL ) {
			formatstr( attributeName, "has_%s", flag );
			cp->Assign( attributeName, true );
			flag = strtok_r( NULL, " ", & savePointer );
		}
		free( processor_flags );
	}
	int modelNo = -1;
	if ((modelNo = sysapi_processor_flags()->model_no) > 0) {
		cp->Assign(ATTR_CPU_MODEL_NUMBER, modelNo);
	}
	int family = -1;
	if ((family = sysapi_processor_flags()->family) > 0) {
		cp->Assign(ATTR_CPU_FAMILY, family);
	}
	int cache = -1;
	if ((cache = sysapi_processor_flags()->cache) > 0) {
		cp->Assign(ATTR_CPU_CACHE_SIZE, cache);
	}
	std::string microarch = sysapi_processor_flags()->processor_microarch;
	if (!microarch.empty()) {
		cp->Assign(ATTR_MICROARCH, microarch);
	}


#ifdef LINUX
	if (hasUnprivUserNamespace()) {
		cp->Assign(ATTR_HAS_USER_NAMESPACES, true);
	}

	static bool already_warned = false;
	switch (hasRotationalScratch()) {
		case rotational_result::ROTATIONAL:
		cp->Assign(ATTR_HAS_ROTATIONAL_SCRATCH, true);
		break;
		case rotational_result::NOT_ROTATIONAL:
		cp->Assign(ATTR_HAS_ROTATIONAL_SCRATCH, false);
		break;
		case rotational_result::UNKNOWN: {
			 if (!already_warned) {
				 dprintf(D_ALWAYS, "Cannot determine rotationality of execute dir, leaving it undefined\n");
				 already_warned = true;
			 }
			 break;
		 }

	}
#endif
	// Temporary Hack until this is a fixed path
	// the Starter will expand this magic string to the
	// actual value
	cp->Assign(ATTR_CONDOR_SCRATCH_DIR, "#CoNdOrScRaTcHdIr#");
}

// return true if the given slot has resources that are assigned to both normal and backfill
// and the given slot is a backfill slot. NOTE: this code does not check to see if the slot is active
bool
MachAttributes::has_nft_conflicts(int slot_id, int slot_subid)
{
	for (auto &[restag, nft] : m_machres_nft_map) {
		for (const auto & nfr : nft.ids) {
			if (nfr.is_owned(slot_id, slot_subid) == 3) {
				return true;
			}
		}
	}
	return false;
}

void
MachAttributes::publish_common_dynamic(ClassAd* cp, bool global /*=false*/)
{
	// things that need to be refreshed periodially

#ifdef WIN32
	// we periodically probe to see if there is a valid local credd.
	if (m_local_credd) { cp->Assign(ATTR_LOCAL_CREDD, m_local_credd); }
#endif

	// KFLOPS and MIPS are only conditionally computed; thus, only
	// advertise them if we computed them.
	if (m_kflops > 0) { cp->Assign( ATTR_KFLOPS, m_kflops ); }
	if (m_mips > 0) { cp->Assign( ATTR_MIPS, m_mips ); }

	if (m_docker_cached_image_size >= 0) {
		cp->Assign(ATTR_DOCKER_CACHED_IMAGE_SIZE, m_docker_cached_image_size);
	}
	// publish offline ids for any of the resources
	for (auto j(m_machres_map.begin());  j != m_machres_map.end();  ++j) {
		std::string ids;
		auto & offline_ids(m_machres_runtime_offline_ids_map[j->first]);
		slotres_devIds_map_t::const_iterator k = m_machres_offline_devIds_map.find(j->first);
		if ( ! offline_ids.empty()) {
			// we have runtime offline ids which *may* need to be merged with the static offline ids list
			if (k != m_machres_offline_devIds_map.end() && ! k->second.empty()) {
				// build a de-duplicated collection of offline ids including the static list and the dynamic list.
				std::set<std::string> all_offline;
				for (auto id = offline_ids.begin(); id != offline_ids.end(); ++id) { all_offline.insert(*id); }
				if (k != m_machres_offline_devIds_map.end()) {
					for (auto id = k->second.begin(); id != k->second.end(); ++id) { all_offline.insert(*id); }
				}
				for (auto id = all_offline.begin(); id != all_offline.end(); ++id) {
					if (!ids.empty()) { ids += ","; }
					ids += *id;
				}
			} else {
				for (auto id = offline_ids.begin(); id != offline_ids.end(); ++id) {
					if (!ids.empty()) { ids += ","; }
					ids += *id;
				}
			}
		} else if (k != m_machres_offline_devIds_map.end()) {
			// we just have the static offline ids list (set at startup)
			if ( ! k->second.empty()) {
				ids = join(k->second, ",");
			}
		}
		string attr(ATTR_OFFLINE_PREFIX); attr += j->first;
		if (ids.empty()) {
			if (cp->Lookup(attr)) {
				cp->Assign(attr, "");
			}
		} else {
			cp->Assign(attr, ids);
		}
	}

	// for the daemon ad, we also want to publish the detected GPUs properties and other nft properties
	if (global) {
		for (auto &[restag, nft] : m_machres_nft_map) {
			if ( ! nft.props.size()) continue;
			// auto & offline_ids(m_machres_runtime_offline_ids_map[restag]);
			std::string attr; attr.reserve(18);
			std::string ids; ids.reserve(2 + (nft.ids.size() * 18));

			ids = "{";
			for (const auto &[nfrId, nfrAd] : nft.props) {
				attr = restag; attr += "_";
				attr += nfrId;
				cleanStringForUseAsAttr(attr, '_');
				if (ids.size() > 1) { ids += ", "; }
				ids += attr;

				// copy the properties ad into and insert the resource id into it
				// then store that into the output ad
				ClassAd * propAd = dynamic_cast<ClassAd*>(nfrAd.Copy());
				propAd->Assign("Id", nfrId);
				cp->Insert(attr, propAd);
			}
			ids += "}";

			attr = "Detected"; attr += restag;
			cp->AssignExpr(attr, ids.c_str());
		}
	}

	cp->Assign( ATTR_TOTAL_VIRTUAL_MEMORY, m_virt_mem );
	cp->Assign( ATTR_LAST_BENCHMARK, m_last_benchmark );
	cp->Assign( ATTR_TOTAL_LOAD_AVG, rint(m_load * 100) / 100.0);
	cp->Assign( ATTR_TOTAL_CONDOR_LOAD_AVG, rint(m_condor_load * 100) / 100.0);
	cp->Assign( ATTR_CLOCK_MIN, m_clock_min );
	cp->Assign( ATTR_CLOCK_DAY, m_clock_day );

	for (auto *pav : m_lst_dynamic) {
		if (pav) pav->AssignToClassAd(cp);
	}
}

void
MachAttributes::publish_slot_dynamic(ClassAd* cp, int slot_id, int slot_subid, bool slot_is_bk, const std::string & res_conflict)
{
	// the global resource conflicts are determined elsewhere and passed in here
	// we we add the NFR conflicts and then publish
	std::string conflict = res_conflict;

	// publish "Available" custom resources and resource properties
	// for use by the evalInEachContext matchmaking function
	// this loop also detects primary/backfill resource conflicts for non-fungible resources
	for (auto &[restag, nft] : m_machres_nft_map) {
		auto & offline_ids(m_machres_runtime_offline_ids_map[restag]);

		std::string tmp;
		std::string attr; attr.reserve(18);
		std::string avail; avail.reserve(2 + (nft.ids.size() * 18));
		avail = "{";

		int num_avail = 0;
		const int owntype = (slot_is_bk ? 2 : 1);
		for (const auto & nfr : nft.ids) {
			if (slot_id >= 0) {
				int ot = nfr.is_owned(slot_id, slot_subid);
				if (IsDebugCategory(D_ZKM)) { formatstr_cat(tmp, "%d(%d.%d)", ot,nfr.owner.id,nfr.owner.dyn_id); }
				if (owntype != ot) {
					if (slot_subid > 0 && (ot &= ~owntype) == 1) {
						if (conflict.size() > 1) { conflict += ","; }
						conflict += nfr.id;
					}
					continue;
				}
			}
			if (offline_ids.count(nfr.id)) continue; // offline ids are not Available
			attr = restag; attr += "_";
			attr += nfr.id;
			cleanStringForUseAsAttr(attr, '_');
			if (avail.size() > 1) { avail += ", "; }
			avail += attr;
			++num_avail;
		}

		avail += "}";
		attr = "Available"; attr += restag;
		dprintf(D_ZKM | D_VERBOSE, "publish_dyn %d.%d.%d setting %s=%s%s\n",
			slot_id, slot_subid, slot_is_bk,
			attr.c_str(), avail.c_str(), tmp.c_str());
		cp->AssignExpr(attr, avail.c_str());

		// if the number of AvailableXX is less than the value of XX in the ad, reduce the value in the ad
		int num_in_ad = num_avail;
		if (cp->LookupInteger(restag, num_in_ad)) {
			dprintf(D_ZKM | D_VERBOSE, "publish_dyn %d.%d.%d %s=%d was %d\n",
				slot_id, slot_subid, slot_is_bk,
				restag.c_str(), num_avail, num_in_ad);
			cp->Assign(restag, num_avail);
		}
	}

	// if this is a backfill slot, publish the ResourceConflict attribute`
	if (slot_is_bk) {
		if (conflict.size() > 0) {
			cp->Assign("ResourceConflict", conflict);
			dprintf(D_ZKM, "publish_dyn %d.%d.%d ResourceConflict=%s\n",
				slot_id, slot_subid, slot_is_bk,
				conflict.c_str());
		} else {
			cp->Delete("ResourceConflict");
		}
	}


}

void
MachAttributes::start_benchmarks( Resource* rip, int &count )
{
	count = 0;
	const ClassAd* cp = rip->r_classad;

	if( rip->isSuspendedForCOD() ) {
			// if there's a COD job, we definitely don't want to run
			// benchmarks
		return;
	}

	bool run_benchmarks = false;
	if ( cp->LookupBool( ATTR_RUN_BENCHMARKS, run_benchmarks ) == 0 ) {
		run_benchmarks = false;
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
	// Update this ClassAds with this new value for LastBenchmark.
	// the other classads will be updated before the next policy evaluation pass
	m_last_benchmark = time(NULL);
	// refresh this in the classad, since we might be about to do policy evaluation
	rip->r_classad->Assign( ATTR_LAST_BENCHMARK, m_last_benchmark );

	dprintf( D_ALWAYS, "State change: benchmarks completed\n" );
	if( rip->activity() == benchmarking_act ) {
		rip->change_state( idle_act );
	}
	return;
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

CpuAttributes::CpuAttributes(
							  unsigned int slot_type,
							  double num_cpus_arg,
							  int num_phys_mem,
							  double virt_mem_fraction,
							  double disk_fraction,
							  const slotres_map_t& slotres_map,
							  const slotres_constraint_map_t & slotres_req_map,
							  const std::string &execute_dir,
							  const std::string &execute_partition_id )
{
	c_type_id = slot_type;
	c_num_slot_cpus = c_num_cpus = num_cpus_arg;
	c_allow_fractional_cpus = num_cpus_arg > 0 && num_cpus_arg < 0.9;
	c_slot_mem = c_phys_mem = num_phys_mem;
	c_virt_mem_fraction = virt_mem_fraction;
	c_disk_fraction = disk_fraction;
	c_slotres_map = slotres_map;
	c_slotres_constraint_map = slotres_req_map;
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


bool
CpuAttributes::set_total_disk(long long total, bool refresh, VolumeManager * volman) {
		// if input total is < 0, that means to figure it out
	if (total > 0) {
		bool changed = total != c_total_disk;
		c_total_disk = total;
		return changed;
	} else if (c_total_disk == 0) {
			// calculate disk at least once if a value is not passed in
		refresh = true;
	}
		// refresh disk if the flag was passed in, or we do not yet have a value
	if (refresh) {
		CondorError err;
		uint64_t used_bytes, total_bytes;
		if (volman && volman->is_enabled()) {
			if (volman->GetPoolSize(used_bytes, total_bytes, err)) {
				c_total_disk = total_bytes/1024 - sysapi_reserve_for_fs();
				c_total_disk = (c_total_disk < 0) ? 0 : c_total_disk;
				dprintf(D_FULLDEBUG, "Used volume manager to get total logical size of %llu\n", c_total_disk);
				return true;
			} else {
				dprintf(D_ERROR, "Failed to get LVM pool size: %s\n", err.getFullText().c_str());
			}
		}
		c_total_disk = sysapi_disk_space(executeDir());
		return true;
	}
	return false;
}

bool
CpuAttributes::bind_DevIds(MachAttributes* map, int slot_id, int slot_sub_id, bool backfill_slot, bool abort_on_fail) // bind non-fungable resource ids to a slot
{
	if ( ! map)
		return true;

	std::string namebuf;
	const char * name = "slot";

	if (IsDebugCatAndVerbosity(d_log_devids)) {
		formatstr(namebuf, "%sslot%d.%d", backfill_slot ? "backfill " : "", slot_id, slot_sub_id);
		name = namebuf.c_str();

		std::string ids_dump;
		if (rip) { // on startup this is called before this structure has been attached to a RIP
			dprintf(d_log_devids, "bind DevIds for %s before : %s\n", name, map->DumpDevIds(ids_dump));
		} else {
			::dprintf(d_log_devids, "bind DevIds for %s before : %s\n", name, map->DumpDevIds(ids_dump));
		}
	}

	for (auto & j : c_slotres_map) {
		// TODO: handle fractional assigned custome resources?
		int cAssigned = int(j.second);

		// if this resource already has bound ids, don't bind again.
		slotres_devIds_map_t::const_iterator m(c_slotres_ids_map.find(j.first));
		if (m != c_slotres_ids_map.end() && cAssigned == (int)m->second.size())
			continue;

		const char * request = nullptr;
		slotres_constraint_map_t::const_iterator req(c_slotres_constraint_map.find(j.first));
		if (req != c_slotres_constraint_map.end()) { request = req->second.c_str(); }

		::dprintf(D_ALWAYS, "bind %s DevIds tag=%s contraint=%s\n", name, j.first.c_str(), request ? request : "");

		if (map->machres_devIds().count(j.first)) {
			int cAllocated = 0;
			for (int ii = 0; ii < cAssigned; ++ii) {
				const char * id = map->AllocateDevId(j.first, request, slot_id, slot_sub_id, backfill_slot);
				if (id) {
					++cAllocated;
					c_slotres_ids_map[j.first].push_back(id);
					::dprintf(d_log_devids, "bind DevIds for %s bound %s %d\n",
						name, id, (int)c_slotres_ids_map[j.first].size());
				}
			}
			if (cAllocated < cAssigned) {
				::dprintf(D_ALWAYS | D_BACKTRACE, "%s Failed to bind local resource '%s'\n", name, j.first.c_str());
				if (abort_on_fail) {
					EXCEPT("Failed to bind local resource '%s'", j.first.c_str());
				}
				std::string reason = "Not enough instances of resource " + j.first;
				set_broken(BROKEN_CODE_BIND_FAIL, reason);
				return false;
			}
			// if any ids were allocated, calculate the effective properties attributes for the assigned ids
			// we *dont* want to execute this code if there are no allocated ids, because it has the side effect
			// of making c_slot_res_ids_map['tag'] exist, which in turn results in the
			// "Assigned<Tag>" slot attribute being defined rather than undefined.
			if (cAllocated > 0) {
				c_slotres_props_map[j.first].Clear();
				map->ComputeDevProps(c_slotres_props_map[j.first], j.first, c_slotres_ids_map[j.first]);
			}
		}
	}

	if (IsDebugCatAndVerbosity(d_log_devids)) {
		std::string ids_dump;
		if (rip) {
			dprintf(d_log_devids, "bind DevIds for %s after : %s\n", name, map->DumpDevIds(ids_dump));
		} else {
			::dprintf(d_log_devids, "bind DevIds for %s after : %s\n", name, map->DumpDevIds(ids_dump));
		}
	}

	return true;
}

void
CpuAttributes::unbind_DevIds(MachAttributes* map, int slot_id, int slot_sub_id, int new_sub_id) // release non-fungable resource ids
{
	if ( ! map) return;

	if (IsDebugCatAndVerbosity(d_log_devids)) {
		std::string ids_dump;
		dprintf(d_log_devids, "unbind DevIds for slot%d.%d before : %s\n", slot_id, slot_sub_id, map->DumpDevIds(ids_dump));
	}

	if ( ! slot_sub_id) return;

	for (auto & j : c_slotres_map) {
		slotres_devIds_map_t::const_iterator k(c_slotres_ids_map.find(j.first));
		if (k != c_slotres_ids_map.end()) {
			slotres_assigned_ids_t & ids = c_slotres_ids_map[j.first];
			while ( ! ids.empty()) {
				bool released = map->ReleaseDynamicDevId(j.first, ids.back().c_str(), slot_id, slot_sub_id, new_sub_id);
				dprintf(released ? d_log_devids : D_ALWAYS, "ubind DevIds for slot%d.%d unbind %s %d %s\n",
					slot_id, slot_sub_id, ids.back().c_str(), (int)ids.size(), released ? "OK" : "failed");
				ids.pop_back();
			}
		}
	}

	if (IsDebugCatAndVerbosity(d_log_devids)) {
		std::string ids_dump;
		dprintf(d_log_devids, "unbind DevIds for slot%d.%d after : %s\n", slot_id, slot_sub_id, map->DumpDevIds(ids_dump));
	}
}

void
CpuAttributes::reconfig_DevIds(MachAttributes* map, int slot_id, int slot_sub_id) // release non-fungable resource ids
{
	if (! map) return;

	//if (IsDebugCatAndVerbosity(d_log_devids)) {
	//	std::string ids_dump;
	//	dprintf(d_log_devids, "reconfig_DevIds for slot%d.%d before : %s\n", slot_id, slot_sub_id, map->DumpDevIds(ids_dump));
	//}

	// make sure that the assigned devid's matches the global assigment
	// which may have changed based on reconfig
	for (auto & j : c_slotres_map) {
		auto ids = c_slotres_ids_map.find(j.first);
		if (ids == c_slotres_ids_map.end())
			continue;

		// for p-slots & static slots, we ignore the sub-id when building the Assigned list (slot_res_devids)
		// but not when counting the number of resources
		// thus in slot1 we can have
		//   AssignedGPUs = "CUDA0,CUDA1,CUDA2"  (slot_res_devids)
		//   OfflineGPUs = "CUDA2"               (offline_ids)
		//   GPUs = 1                            (slot_res)
		// while in slot1_1 we have
		//   AssignedGPUs = "CUDA0"
		//   Gpus = 1

		j.second = map->RefreshDevIds(j.first, ids->second, slot_id, slot_sub_id);
	}

	if (IsDebugCatAndVerbosity(d_log_devids)) {
		std::string ids_dump;
		dprintf(d_log_devids, "reconfig_DevIds for slot%d.%d after : %s\n", slot_id, slot_sub_id, map->DumpDevIds(ids_dump));
	}
}

void
CpuAttributes::publish_dynamic(ClassAd* cp) const
{
		cp->Assign( ATTR_TOTAL_DISK, c_total_disk );
		cp->Assign( ATTR_DISK, c_disk );
		cp->Assign( ATTR_CONDOR_LOAD_AVG, rint(c_condor_load * 100) / 100.0 );
		cp->Assign( ATTR_LOAD_AVG, rint((c_owner_load + c_condor_load) * 100) / 100.0 );
		cp->Assign( ATTR_KEYBOARD_IDLE, c_idle );

		// dprintf(D_IDLE, "KeyboardIdle=%lld LoadAvg=%.2f\n", c_idle, rint((c_owner_load + c_condor_load) * 100) / 100.0);

			// ConsoleIdle cannot be determined on all platforms; thus, only
			// advertise if it is not -1.
		if( c_console_idle != -1 ) {
			cp->Assign( ATTR_CONSOLE_IDLE, c_console_idle );
		}
}

void
CpuAttributes::publish_static(
	ClassAd* cp,
	const ResBag * inuse, // resources in-use in primary slot
	const ResBag * brokenRes) const // resources lost to p-slot because a broken slot was deleted
{
		std::string ids;
		std::string broken_reason;
		bool broken = is_broken(&broken_reason);

		// no need to clear these here, slot brokenness is permanent
		// if we ever change that, we can clear these attributes where we
		// clear the brokenness state
		if (broken) {
			cp->Assign(ATTR_SLOT_BROKEN_CODE, c_broken_code);
			cp->Assign(ATTR_SLOT_BROKEN_REASON, broken_reason);
		}

		int mem = c_phys_mem;
		if (broken) {
			mem = 0;
		} else if (inuse) {
			int deduct = MAX(0, inuse->mem);
			mem = MAX(0, mem - deduct);
		}

		cp->Assign( ATTR_MEMORY, mem );

		// TotalSlotMemory is (original provisioned memory) - (memory of slots deleted while broken)
		int slot_mem = c_slot_mem;
		if (brokenRes && brokenRes->mem > 0) {
			cp->Assign("BrokenSlot" ATTR_MEMORY, brokenRes->mem);
			slot_mem = MAX(0, slot_mem - brokenRes->mem);
		}
		cp->Assign( ATTR_TOTAL_SLOT_MEMORY, slot_mem );

		double slot_disk = c_slot_disk;
		if (brokenRes && brokenRes->disk > 0) {
			cp->Assign("BrokenSlot" ATTR_DISK, brokenRes->disk);
			slot_disk = MAX(0, slot_disk - brokenRes->disk);
		}
		cp->Assign( ATTR_TOTAL_SLOT_DISK, slot_disk );

		double cpus = c_num_cpus;
		if (broken) {
			cpus = 0;
		} else if (inuse) {
			double deduct = MAX(0, inuse->cpus);
			cpus = MAX(0, cpus - deduct);
		}

		double slot_cpus = c_num_slot_cpus;
		if (brokenRes && brokenRes->cpus > 0) {
			if (c_allow_fractional_cpus) {
				cp->Assign("BrokenSlot" ATTR_CPUS, brokenRes->cpus);
			} else {
				cp->Assign("BrokenSlot" ATTR_CPUS, (int)(brokenRes->cpus + 0.1));
			}
			slot_cpus = MAX(0, slot_cpus - brokenRes->cpus);
		}

		if (c_allow_fractional_cpus) {
			cp->Assign( ATTR_CPUS, cpus );
			cp->Assign( ATTR_TOTAL_SLOT_CPUS, slot_cpus );
		} else {
			cp->Assign( ATTR_CPUS, (int)(cpus + 0.1) );
			cp->Assign( ATTR_TOTAL_SLOT_CPUS, (int)(slot_cpus + 0.1) );
		}
		
		cp->Assign( ATTR_VIRTUAL_MEMORY, c_virt_mem );

		// publish local resource quantities for this slot
		for (auto j(c_slotres_map.begin());  j != c_slotres_map.end();  ++j) {
			// example: set GPUs = 1
			int quantity = broken ? 0 : int(j->second);
			cp->Assign(j->first, quantity);

			string attr = ATTR_TOTAL_SLOT_PREFIX; attr += j->first;
			long long tot = 0;
			auto tt = c_slottot_map.find(j->first);
			if (tt != c_slottot_map.end()) { tot = (long long)tt->second; }
			if (brokenRes) {
				auto bt = brokenRes->resmap.find(j->first);
				if (bt != brokenRes->resmap.end() && bt->second > 0) {
					std::string broken_attr = "BrokenSlot" + j->first;
					cp->Assign(broken_attr, (long long)j->second);
					tot = MAX(0, tot - (long long)bt->second);
				}
			}
			cp->Assign(attr, tot);          // example: set TotalSlotGPUs = 2

			slotres_devIds_map_t::const_iterator k(c_slotres_ids_map.find(j->first));
			if (k != c_slotres_ids_map.end()) {
				attr = "Assigned";
				attr += j->first;
				ids = join(k->second, ",");  // k->second is type slotres_assigned_ids_t which is vector<string>
				cp->Assign(attr, ids);   // example: AssignedGPUs = "GPU-01abcdef,GPU-02bcdefa"
			} else {
				if ( ! broken && inuse) {
					auto dk = inuse->resmap.find(j->first);
					if (dk != inuse->resmap.end() && dk->second > 0) {
						cp->Assign(j->first, int(j->second - dk->second));  // example: set Bandwidth = 100
					}
				}
				continue;
			}

			// publish properties of assigned local resources
			slotres_props_t::const_iterator it = c_slotres_props_map.find(j->first);
			if (it != c_slotres_props_map.end()) {
				for (auto & kvp : it->second) {
					attr = j->first; attr += "_"; attr += kvp.first;
					cp->Insert(attr, kvp.second->Copy());
				}
			}
		}
}

void
CpuAttributes::compute_virt_mem_share(double virt_mem)
{
	// Shared attributes that we only get a fraction of
	double val = virt_mem;
	if (!IS_AUTO_SHARE(c_virt_mem_fraction)) {
		val *= c_virt_mem_fraction;
	}
	c_virt_mem = (unsigned long)floor( val );
}

void
CpuAttributes::compute_disk()
{
	// Dynamic, non-shared attributes we need to actually compute

	double val = c_total_disk * c_disk_fraction;
	c_disk = (long long)ceil(val);
	if (0 == (long long)c_slot_disk) {
		// only use the 1st compute ignore subsequent.
		c_slot_disk = c_disk; 
	}
}


void
CpuAttributes::display_load(int dpf_flags) const
{
	// dpf_flags is expected to be 0 or D_VERBOSE
	dprintf( D_KEYBOARD | dpf_flags,
				"Idle time: Keyboard: %-8lld Console: %lld\n",
				(long long)c_idle, (long long)c_console_idle);

	dprintf( D_LOAD | dpf_flags,
				"LoadAvg: %.2f  CondorLoad: %.2f  OwnerLoad: %.2f\n",
				c_condor_load + c_owner_load, c_condor_load, c_owner_load);
}


const char *
CpuAttributes::cat_totals(std::string & buf) const
{
	buf += "Cpus: ";
	if (IS_AUTO_SHARE(c_num_cpus)) {
		buf += "auto";
	} else {
		formatstr_cat(buf, "%.2f", c_num_cpus);
		if (c_num_cpus != c_num_slot_cpus) { formatstr_cat(buf, " of %.2f", c_num_slot_cpus); }
	}

	buf += ", Memory: ";
	if (IS_AUTO_SHARE(c_phys_mem)) {
		buf += "auto";
	}
	else {
		formatstr_cat(buf, "%d", c_phys_mem);
	}

	buf += ", Swap: ";
	if (IS_AUTO_SHARE(c_virt_mem_fraction)) {
		buf += "auto";
	}
	else {
		formatstr_cat(buf, "%.2f%%", 100*c_virt_mem_fraction );
	}

	buf += ", Disk: ";
	if (IS_AUTO_SHARE(c_disk_fraction)) {
		buf += "auto";
	}
	else {
		formatstr_cat(buf, "%.2f%%", 100*c_disk_fraction );
	}

	for (auto j(c_slotres_map.begin()); j != c_slotres_map.end(); ++j) {
		if (IS_AUTO_SHARE(j->second)) {
			formatstr_cat(buf, ", %s: auto", j->first.c_str());
		} else {
			formatstr_cat(buf, ", %s: %d", j->first.c_str(), int(j->second));
		}
	}

	return buf.c_str();
}


void
CpuAttributes::dprintf( int flags, const char* fmt, ... ) const
{
    if (NULL == rip) {
        ::dprintf(D_BACKTRACE, "about to except");
        EXCEPT("CpuAttributes::dprintf() called with NULL resource pointer");
    }
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}

CpuAttributes&
CpuAttributes::operator+=( CpuAttributes& rhs )
{
	c_num_cpus += rhs.c_num_cpus;
	c_phys_mem += rhs.c_phys_mem;
	if (!IS_AUTO_SHARE(rhs.c_virt_mem_fraction) &&
		!IS_AUTO_SHARE(c_virt_mem_fraction)) {
		c_virt_mem_fraction += rhs.c_virt_mem_fraction;
		c_virt_mem += rhs.c_virt_mem;   // not perfect, but close enough for now..
	}

	if (!IS_AUTO_SHARE(rhs.c_disk_fraction) &&
		!IS_AUTO_SHARE(c_disk_fraction)) {
		c_disk_fraction += rhs.c_disk_fraction;
	}

	for (auto j(c_slotres_map.begin()); j != c_slotres_map.end(); ++j) {
		j->second += rhs.c_slotres_map[j->first];
	}

	compute_disk();

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

	for (auto j(c_slotres_map.begin()); j != c_slotres_map.end(); ++j) {
		j->second -= rhs.c_slotres_map[j->first];
	}

	compute_disk();

	return *this;
}

const char * CpuAttributes::_slot_request::dump(std::string & buf) const
{
	buf.clear();
	formatstr(buf, "Cpus=%f, Memory=%d, Disk=%f/1", num_cpus, num_phys_mem, disk_fraction);
	if (virt_mem_fraction > 0) {
		formatstr_cat(buf, ", Swap=%f/1", virt_mem_fraction);
	}
	for (auto &[tag,val] : slotres) {
		formatstr_cat(buf, " ,%s=%f", tag.c_str(), val);
		auto found = slotres_constr.find(tag);
		if (found != slotres_constr.end()) {
			formatstr_cat(buf, " : %s", found->second.c_str());
		}
	}
	return buf.c_str();
}


ResBag&
ResBag::operator+=(const CpuAttributes& rhs)
{
	cpus += rhs.c_num_slot_cpus;
	disk += rhs.c_slot_disk;
	mem += rhs.c_slot_mem;
	slots += 1;
	for (auto & res : rhs.c_slottot_map) { resmap[res.first] += res.second; }
	return *this;
}

ResBag&
ResBag::operator-=(const CpuAttributes& rhs)
{
	cpus -= rhs.c_num_slot_cpus;
	disk -= rhs.c_slot_disk;
	mem -= rhs.c_slot_mem;
	slots -= 1;
	for (auto & res : rhs.c_slottot_map) { resmap[res.first] -= res.second; }
	return *this;
}

void ResBag::reset()
{
	cpus = 0; disk = 0; mem = 0; slots = 0;
	for (auto & res : resmap) { resmap[res.first] = 0; }
}

bool ResBag::underrun(std::string * names)
{
	if (names) {
		if (cpus < 0) *names += "Cpus,";
		if (mem < 0) *names +=  "Memory,";
		if (disk < 0) *names += "Disk,";
		for (const auto & res : resmap) {
			if (res.second < 0) *names += res.first + ",";
		}
		size_t cch = names->size();
		if (cch > 0) {
			names->resize(cch-1);  // delete the extra comma.
			return true;
		}
	} else if (cpus < 0 || disk < 0 || mem < 0) {
		return true;
	}
	for (const auto & res : resmap) {
		if (res.second < 0) return true;
	}
	return false;
}

bool ResBag::excess(std::string * names)
{
	if (names) {
		if (cpus > 0) *names += "Cpus,";
		if (mem > 0) *names +=  "Memory,";
		if (disk > 0) *names += "Disk,";
		for (const auto& res : resmap) {
			if (res.second > 0) *names += res.first + ",";
		}
		size_t cch = names->size();
		if (cch > 0) {
			names->resize(cch-1);  // delete the extra comma.
			return true;
		}
	} else if (cpus > 0 || disk > 0 || mem > 0) {
		return true;
	}
	for (const auto & res : resmap) {
		if (res.second > 0) return true;
	}
	return false;
}

// reset only the underrun values
void ResBag::clear_underrun()
{
	if (cpus < 0) cpus = 0;
	if (mem < 0)  mem = 0;
	if (disk < 0) disk = 0;
	for (auto & res : resmap) {
		if (res.second < 0) res.second = 0;
	}
}

// reset only the excess values
void ResBag::clear_excess()
{
	if (cpus > 0) cpus = 0;
	if (mem > 0)  mem = 0;
	if (disk > 0) disk = 0;
	for (auto & res : resmap) {
		if (res.second > 0) res.second = 0;
	}
}

const char * ResBag::dump(std::string & buf) const
{
	buf.clear();
	formatstr(buf, "Cpus=%f, Memory=%d, Disk=%lld", cpus, mem, disk);
	for (auto & res : resmap) {
		formatstr_cat(buf, " ,%s=%f", res.first.c_str(), res.second);
	}
	return buf.c_str();
}

void ResBag::Publish(ClassAd& ad, const char * prefix) const
{
	std::string attr(prefix ? prefix : "");
	size_t off = attr.size();

	attr.erase(off); attr.append(ATTR_MEMORY);
	ad.Assign(attr, mem);

	attr.erase(off); attr.append(ATTR_CPUS);
	ad.Assign(attr, cpus);

	attr.erase(off); attr.append(ATTR_DISK);
	ad.Assign(attr, disk);

	for (auto & res : resmap) {
		attr.erase(off); attr.append(res.first);
		ad.Assign(attr, res.second);
	}
}

AvailAttributes::AvailAttributes( MachAttributes* map) {
	a_num_cpus = map->num_cpus();
	a_num_cpus_auto_count = 0;
	a_phys_mem = map->phys_mem();
	a_phys_mem_auto_count = 0;
	a_virt_mem_fraction = 1.0;
	a_virt_mem_auto_count = 0;
    a_slotres_map = map->machres();
    for (auto & j : a_slotres_map) {
        a_autocnt_map[j.first] = 0;
    }
}

AvailDiskPartition &
AvailAttributes::GetAvailDiskPartition(std::string const &execute_partition_id)
{
	auto it =  m_execute_partitions.find(execute_partition_id);
	if (it == m_execute_partitions.end()) {
			// No entry found for this partition.  Create one.
		auto it_and_bool = m_execute_partitions.emplace(execute_partition_id,AvailDiskPartition());
		return it_and_bool.first->second;
	}
	return it->second;;
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
	for (auto j(new_res.begin()); j != new_res.end(); ++j) {
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

	for (auto j(a_slotres_map.begin()); j != a_slotres_map.end(); ++j) {
		j->second = new_res[j->first];
		if (IS_AUTO_SHARE(cap->c_slotres_map[j->first])) {
			a_autocnt_map[j->first] += 1;
		}
	}

	return true;
}

// call after ::decrement fails in order to reduce the request and produce a reason string
void
AvailAttributes::trim_request_to_fit( CpuAttributes::_slot_request & req, const char * execute_partition_id, std::string & unfit ) 
{
	const double floor = -0.000001f;

	if ( ! IS_AUTO_SHARE(req.num_cpus)) {
		if (req.allow_fractional_cpus && (req.num_cpus < .5)) {
			//PRAGMA_REMIND("TJ: account for fractional cpus properly...")
		} else {
			double new_cpus = a_num_cpus - req.num_cpus;
			if (new_cpus < floor) {
				formatstr_cat(unfit, "Cpus (%.2f short) ", -new_cpus);
				req.num_cpus = a_num_cpus;
			}
		}
	}

	if ( ! IS_AUTO_SHARE(req.num_phys_mem)) {
		int new_phys_mem = a_phys_mem - req.num_phys_mem;
		if (new_phys_mem < floor) {
			formatstr_cat(unfit, "Memory (%d short) ", -new_phys_mem);
			req.num_phys_mem = a_phys_mem;
		}
	}

	if( !IS_AUTO_SHARE(req.virt_mem_fraction) ) {
		double new_virt_mem = a_virt_mem_fraction - req.virt_mem_fraction;
		if (new_virt_mem < floor) {
			formatstr_cat(unfit, "Swap (%.2f%% short) ", -new_virt_mem*100);
			req.virt_mem_fraction = a_virt_mem_fraction;
		}
	}

	if (execute_partition_id && *execute_partition_id) {
		AvailDiskPartition &partition = GetAvailDiskPartition( execute_partition_id );
		if( !IS_AUTO_SHARE(req.disk_fraction) ) {
			double new_disk = partition.m_disk_fraction - req.disk_fraction;
			if (new_disk < floor) {
				formatstr_cat(unfit, "Disk (%.2f%% short) ", -new_disk*100);
				req.disk_fraction = partition.m_disk_fraction;
			}
		}
	}

	slotres_map_t new_res(a_slotres_map);
	for (auto j(new_res.begin()); j != new_res.end(); ++j) {
		if (!IS_AUTO_SHARE(req.slotres[j->first])) {
			double new_val = j->second - req.slotres[j->first];
			if (new_val < floor) {
				formatstr_cat(unfit, "%s (%.2f short) ", j->first.c_str(), -new_val);
				req.slotres[j->first] = j->second;
			}
		}
	}
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
	for (auto j(a_slotres_map.begin());  j != a_slotres_map.end();  ++j) {
		remain_cap[j->first] = j->second;
	}
	for (auto j(a_autocnt_map.begin());  j != a_autocnt_map.end();  ++j) {
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
		double new_value = (double) a_num_cpus / (double) a_num_cpus_auto_count;
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
		AvailDiskPartition &partition = GetAvailDiskPartition ( cap->c_execute_partition_id );
		ASSERT( partition.m_auto_count > 0 );
		double new_value = partition.m_disk_fraction / partition.m_auto_count;
		if( new_value <= 0 ) {
			return false;
		}
		cap->c_disk_fraction = new_value;
	}

	for (auto j(a_slotres_map.begin());  j != a_slotres_map.end();  ++j) {
		if (IS_AUTO_SHARE(cap->c_slotres_map[j->first])) {
			// replace auto share vals with final values:
			// if there are less than 1 of the resource remaining, hand out 0
			ASSERT(a_autocnt_map[j->first] > 0) // since this is auto, we expect that the count of autos is at least 1
			if (j->second < 1) {
				cap->c_slotres_map[j->first] = 0;
			} else {
				// distribute integral values evenly among slots, with excess going to first slots(s)
				long long new_value = (long long)ceil(remain_cap[j->first] / remain_cnt[j->first]);
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
AvailAttributes::cat_totals(std::string & buf, const char * execute_partition_id)
{
	AvailDiskPartition &partition = GetAvailDiskPartition( execute_partition_id );
	formatstr_cat(buf, "Cpus: %d, Memory: %d, Swap: %.2f%%, Disk: %.2f%%",
		a_num_cpus, a_phys_mem, 100*a_virt_mem_fraction,
		100*partition.m_disk_fraction );
	for (auto & j : a_slotres_map) {
		formatstr_cat(buf, ", %s: %d", j.first.c_str(), int(j.second));
	}
}
