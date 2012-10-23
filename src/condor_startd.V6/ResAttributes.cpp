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
	m_num_cpus = sysapi_ncpus();

	m_num_real_cpus = sysapi_ncpus_raw();

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
	m_ckptpltfrm = strdup(sysapi_ckptpltfrm());

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
		m_ckptpltfrm = strdup(sysapi_ckptpltfrm());

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
		dprintf( D_FULLDEBUG, "Swap space: %lu\n", m_virt_mem );

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


void MachAttributes::init_machine_resources() {
    // defines the space of local machine resources, and their quantity
    m_machres_map.clear();

    // this may be filled from resource inventory scripts
    m_machres_attr.Clear();

    // these are reserved for standard/fixed resources
    std::set<string> fixedRes;
    fixedRes.insert("cpu");
    fixedRes.insert("cpus");
    fixedRes.insert("disk");
    fixedRes.insert("swap");
    fixedRes.insert("mem");
    fixedRes.insert("memory");
    fixedRes.insert("ram");

    // get the list of defined local resource names:
    char* ptmp = param("MACHINE_RESOURCE_NAMES");
    string resnames;
    if (NULL != ptmp) {
        // if admin defined MACHINE_RESOURCE_NAMES, then use this list
        // as the source of expected custom machine resources
        resnames = ptmp;
        free(ptmp);
    } else {
        // otherwise, build the list of custom machine resources from
        // all occurrences of MACHINE_RESOURCE_<resname>
        std::set<string> resset;
        Regex re;
        int err = 0;
        const char* pszMsg = 0;
        const string prefix = "MACHINE_RESOURCE_";
        const string invprefix = "INVENTORY_";
        const string restr = prefix + "(.+)";
        ASSERT(re.compile(restr.c_str(), &pszMsg, &err, PCRE_CASELESS));
	std::vector<std::string> resdef;
        const int n = param_names_matching(re, resdef);
        for (int j = 0;  j < n;  ++j) {
            string rname = resdef[j].substr(prefix.length());
            string testinv = rname.substr(0, invprefix.length());
            if (0 == strcasecmp(testinv.c_str(), invprefix.c_str())) {
                // if something was defined via MACHINE_RESOURCE_INVENTORY_<rname>, strip "INVENTORY_"
                rname = rname.substr(invprefix.length());
            }
            std::transform(rname.begin(), rname.end(), rname.begin(), ::tolower);
            resset.insert(rname);
        }
        for (std::set<string>::iterator j(resset.begin());  j != resset.end();  ++j) {
            resnames += " ";
            resnames += *j;
        }
    }

    // scan the list of local resources, to obtain their quantity and associated attributes (if any)
    StringList reslist(resnames.c_str());
    reslist.rewind();
    while (const char* rnp = reslist.next()) {
        string rname(rnp);
        std::transform(rname.begin(), rname.end(), rname.begin(), ::tolower);
        if (fixedRes.count(rname) > 0) {
            EXCEPT("pre-defined resource name '%s' is reserved", rname.c_str());
        }

        // If MACHINE_RESOURCE_<rname> is present, use that and move on:
        string pname;
        formatstr(pname, "MACHINE_RESOURCE_%s", rname.c_str());
        char* machresp = param(pname.c_str());
        if (machresp) {
            int v = param_integer(pname.c_str(), 0, 0, INT_MAX);
            m_machres_map[rname] = v;
            free(machresp);
            continue;
        }

        // current definition of REMIND macro is not working with gcc
        #pragma message("MACHINE_RESOURCE_INVENTORY_<rname> is deprecated, and will be removed when a solution using '|' in config files is fleshed out")
        // if we didn't find MACHINE_RESOURCE_<rname>, then try MACHINE_RESOURCE_INVENTORY_<rname>
        formatstr(pname, "MACHINE_RESOURCE_INVENTORY_%s", rname.c_str());
        char* invscriptp = param(pname.c_str());
        if (NULL == invscriptp) {
            EXCEPT("Missing configuration for local machine resource %s", rname.c_str());
        }

        // if there is an inventory script, then attempt to execute it and use its output to
        // identify the local resource quantity, and any associated attributes:
        string invscript = invscriptp;
        free(invscriptp);

        ArgList invcmd;
        StringList invcmdal(invscript.c_str());
        invcmdal.rewind();
        while (char* invarg = invcmdal.next()) {
            invcmd.AppendArg(invarg);
        }
        FILE* fp = my_popen(invcmd, "r", FALSE);
        if (NULL == fp) {
            EXCEPT("Failed to execute local resource inventory script \"%s\"\n", invscript.c_str());
        }

        ClassAd invad;
        char invline[1000];
        while (fgets(invline, sizeof(invline), fp)) {
            if (!invad.Insert(invline)) {
                dprintf(D_ALWAYS, "Failed to insert \"%s\" into machine resource attributes: ignoring\n", invline);
            }
        }
        my_pclose(fp);

        // require the detected machine resource to be present as an attribute
        string ccname(rname.c_str());
        *(ccname.begin()) = toupper(*(ccname.begin()));
        string detname;
        formatstr(detname, "%s%s", ATTR_DETECTED_PREFIX, ccname.c_str());
        int v = 0;
        if (!invad.LookupInteger(detname.c_str(), v)) {
            EXCEPT("Missing required attribute \"%s = <n>\" from output of %s\n", detname.c_str(),  invscript.c_str());
        }

        // success
        m_machres_map[rname] = v;
        invad.Delete(rname.c_str());
        m_machres_attr.Update(invad);
    }

    for (slotres_map_t::iterator j(m_machres_map.begin());  j != m_machres_map.end();  ++j) {
        dprintf(D_ALWAYS, "Local machine resource %s = %d\n", j->first.c_str(), int(j->second));
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

		cp->Assign( ATTR_TOTAL_VIRTUAL_MEMORY, (int)m_virt_mem );

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

        string machine_resources = "cpus memory disk swap";
        // publish any local resources
        for (slotres_map_t::iterator j(m_machres_map.begin());  j != m_machres_map.end();  ++j) {
            string rname(j->first.c_str());
            *(rname.begin()) = toupper(*(rname.begin()));
            string attr;
            formatstr(attr, "%s%s", ATTR_DETECTED_PREFIX, rname.c_str());
            cp->Assign(attr.c_str(), int(j->second));
            formatstr(attr, "%s%s", ATTR_TOTAL_PREFIX, rname.c_str());
            cp->Assign(attr.c_str(), int(j->second));
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
							  int num_cpus_arg, 
							  int num_phys_mem,
							  float virt_mem_fraction,
							  float disk_fraction,
                              const slotres_map_t& slotres_map,
							  MyString &execute_dir,
							  MyString &execute_partition_id )
{
	map = map_arg;
	c_type = slot_type;
	c_num_slot_cpus = c_num_cpus = num_cpus_arg;
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
CpuAttributes::publish( ClassAd* cp, amask_t how_much )
{
	if( IS_UPDATE(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_VIRTUAL_MEMORY, (int)c_virt_mem );

		cp->Assign( ATTR_TOTAL_DISK, (int)c_total_disk );

		cp->Assign( ATTR_DISK, (int)c_disk );
		
	}

	if( IS_TIMEOUT(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_CONDOR_LOAD_AVG, rint(c_condor_load * 100) / 100.0 );

		cp->Assign( ATTR_LOAD_AVG, rint((c_owner_load + c_condor_load) * 100) / 100.0 );

		cp->Assign( ATTR_KEYBOARD_IDLE, (int)c_idle );
  
			// ConsoleIdle cannot be determined on all platforms; thus, only
			// advertise if it is not -1.
		if( c_console_idle != -1 ) {
			cp->Assign( ATTR_CONSOLE_IDLE, (int)c_console_idle );
		}
	}

	if( IS_STATIC(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_MEMORY, c_phys_mem );

		cp->Assign( ATTR_CPUS, c_num_cpus );
		
		cp->Assign( ATTR_TOTAL_SLOT_MEMORY, c_slot_mem );
		
		cp->Assign( ATTR_TOTAL_SLOT_DISK, (int)c_slot_disk );

		cp->Assign( ATTR_TOTAL_SLOT_CPUS, c_num_slot_cpus );
		
        // publish local resource quantities for this slot
        for (slotres_map_t::iterator j(c_slotres_map.begin());  j != c_slotres_map.end();  ++j) {
            string rname(j->first.c_str());
            *(rname.begin()) = toupper(*(rname.begin()));
            string attr;
            formatstr(attr, "%s%s", "", rname.c_str());
            cp->Assign(attr.c_str(), int(j->second));
            formatstr(attr, "%s%s", ATTR_TOTAL_SLOT_PREFIX, rname.c_str());
            cp->Assign(attr.c_str(), int(c_slottot_map[j->first]));
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
		c_disk = (int)floor( val );
		if (0 == (int)c_slot_disk)
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
		if( IsDebugLevel( D_LOAD ) ) {
			dprintf( D_FULLDEBUG, 
					 "%s %.2f  %s %.2f  %s %.2f\n",  
					 "SystemLoad:", c_condor_load + c_owner_load,
					 "CondorLoad:", c_condor_load,
					 "OwnerLoad:", c_owner_load );
		}
		if( IsDebugLevel( D_KEYBOARD ) ) {
			dprintf( D_FULLDEBUG, 
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

	
	if( c_num_cpus == AUTO_CPU ) {
		::dprintf( dflag | D_NOHEADER, 
			 "Cpus: auto");
	} else {
		::dprintf( dflag | D_NOHEADER, 
			 "Cpus: %d", c_num_cpus);
	}

	if( c_phys_mem == AUTO_MEM ) {
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
	float new_virt_mem, new_disk, floor = -0.000001f;
	
	new_cpus = a_num_cpus - cap->c_num_cpus;

	new_phys_mem = a_phys_mem;
	if( cap->c_phys_mem != AUTO_MEM ) {
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

    a_phys_mem = new_phys_mem;
    if( cap->c_phys_mem == AUTO_MEM ) {
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

bool
AvailAttributes::computeAutoShares( CpuAttributes* cap )
{
	if( cap->c_phys_mem == AUTO_MEM ) {
		ASSERT( a_phys_mem_auto_count > 0 );
		int new_value = a_phys_mem / a_phys_mem_auto_count;
		if( new_value < 1 ) {
			return false;
		}
		cap->c_slot_mem = cap->c_phys_mem = new_value;
	}

	if( IS_AUTO_SHARE(cap->c_virt_mem_fraction) ) {
		ASSERT( a_virt_mem_auto_count > 0 );
		float new_value = a_virt_mem_fraction / a_virt_mem_auto_count;
		if( new_value <= 0 ) {
			return false;
		}
		cap->c_virt_mem_fraction = new_value;
	}

	if( IS_AUTO_SHARE(cap->c_disk_fraction) ) {
		AvailDiskPartition &partition = GetAvailDiskPartition( cap->c_execute_partition_id );
		ASSERT( partition.m_auto_count > 0 );
		float new_value = partition.m_disk_fraction / partition.m_auto_count;
		if( new_value <= 0 ) {
			return false;
		}
		cap->c_disk_fraction = new_value;
	}

    for (slotres_map_t::iterator j(a_slotres_map.begin());  j != a_slotres_map.end();  ++j) {
        if (IS_AUTO_SHARE(cap->c_slotres_map[j->first])) {
            // replace auto share vals with final values:
            ASSERT(a_autocnt_map[j->first] > 0);
            int new_value = int(j->second / a_autocnt_map[j->first]);
            if (new_value <= 0) return false;
            cap->c_slotres_map[j->first] = new_value;
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
