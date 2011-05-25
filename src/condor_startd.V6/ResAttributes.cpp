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
#include <math.h>


MachAttributes::MachAttributes()
{
	m_mips = -1;
	m_kflops = -1;
	m_last_benchmark = 0;
	m_last_keypress = time(0)-1;
	m_seen_keypress = false;

	m_arch = NULL;
	m_opsys = NULL;
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
#endif
}


MachAttributes::~MachAttributes()
{
	if( m_arch ) free( m_arch );
	if( m_opsys ) free( m_opsys );
	if( m_uid_domain ) free( m_uid_domain );
	if( m_filesystem_domain ) free( m_filesystem_domain );
	if( m_ckptpltfrm ) free( m_ckptpltfrm );
#if defined(WIN32)
	if( m_local_credd ) free( m_local_credd );
#endif
}


void
MachAttributes::init()
{
	this->compute( A_ALL );
}


void
MachAttributes::compute( amask_t how_much )
{
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
	}

	if( IS_TIMEOUT(how_much) && IS_SUMMED(how_much) ) {
		m_condor_load = resmgr->sum( &Resource::condor_load );
		if( m_condor_load > m_load ) {
			m_condor_load = m_load;
		}
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

//
// Add Windows registry keys to machine ClassAds
//
#ifdef WIN32 

// parse the start of a string and return hive key if the start matches
// one of the known hive designators HKLM for HKEY_LOCAL_MACHINE, etc.
// if the caller passes pixPrefixSep, then this function will also return
// the index of the first character after the hive prefix, which should be
// \0, \ or /. 
static HKEY parse_hive_prefix(const char * psz, int cch, int * pixPrefixSep)
{
	HKEY hkey = NULL;
	char ach[5]	= {0};
	for (int ii = 0; ii <= cch && ii < NUM_ELEMENTS(ach); ++ii)
	{
		if (psz[ii] == '\\' || /*psz[ii] == '/' || */ psz[ii] == '\0')
		{
			static const struct {
				long id;
				HKEY key;
			} aMap[] = {
				'HKCR', HKEY_CLASSES_ROOT,
				'HKCU', HKEY_CURRENT_USER,
				'HKLM',	HKEY_LOCAL_MACHINE,
				'HKU\0',HKEY_USERS,
				'HKPD',	HKEY_PERFORMANCE_DATA,
				'HKPT',	HKEY_PERFORMANCE_TEXT,
				'HKPN',	HKEY_PERFORMANCE_NLSTEXT,
				'HKCC',	HKEY_CURRENT_CONFIG,
				'HKDD', HKEY_DYN_DATA,
			};

			long id = htonl(*(long*)&ach);
			for (int ix = 0; ix < NUM_ELEMENTS(aMap); ++ix)
			{
				if (id == aMap[ix].id)
				{
					hkey = aMap[ix].key;
					break;
				}
			}
				
			if (hkey && pixPrefixSep)
				*pixPrefixSep = ii;
			break;
		}
		ach[ii] = toupper(psz[ii]);
	}

	return hkey;
}

// return a string name for one of the HKEY_xxx constants
// we use this for logging used for logging.
//
static const char * root_key_name(HKEY hroot)
{
	static const struct {
		const char * psz;
		HKEY key;
	} aMap[] = {
		"HKCR", HKEY_CLASSES_ROOT,
		"HKCU", HKEY_CURRENT_USER,
		"HKLM",	HKEY_LOCAL_MACHINE,
		"HKU",	HKEY_USERS,
		"HKPD",	HKEY_PERFORMANCE_DATA,
		"HKPT",	HKEY_PERFORMANCE_TEXT,
		"HKPN",	HKEY_PERFORMANCE_NLSTEXT,
		"HKCC",	HKEY_CURRENT_CONFIG,
		"HKDD", HKEY_DYN_DATA,
	};

	for (int ix = 0; ix < NUM_ELEMENTS(aMap); ++ix)
	{
		if (hroot == aMap[ix].key)
		{
			return aMap[ix].psz;
		}
	}
	return NULL;
}

// return the error message string
// for a given windows error code.  used for logging.  
//
static const char * GetLastErrMessage(int err, char * szMsg, int cchMsg)
{
	int cch = FormatMessage ( 
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, 
				err, 
				MAKELANGID ( LANG_NEUTRAL, SUBLANG_DEFAULT ), 
				szMsg, 
				cchMsg, 
				NULL );
	if (cch > 0)
	{

		// remove trailing \r\n
		while (cch > 0 && (szMsg[cch-1] == '\r' || szMsg[cch-1] == '\n'))
			szMsg[--cch] = 0;
	}
	else
	{
		// no message found, print the last error code
		wsprintf(szMsg, "0x%X", err);
	}

	return szMsg;
}


//
// returns a strdup'd string value for the given windows registry entry.
//
static char * get_windows_reg_value(
	const char * pszRegKey, 
	const char * pszValueName, 
	int          options = 0)
{
	char * value = NULL;   // return value from this function
	char * pszTemp = NULL; // in case we need to copy the keyname
	int cch = lstrlen(pszRegKey);

	int ixStart = 0; 
	HKEY hroot = parse_hive_prefix(pszRegKey, cch, &ixStart);
	if (hroot)
	{
		pszRegKey += ixStart;
		if (pszRegKey[0] == '\\')
			++pszRegKey;
		// really short keys like HKCR\.bat can end up with an empty
		// keyname once we strip off the hive prefix, if that happens
		// use the valuename as the keyname and set the valuename to ""
		if ( ! pszRegKey[0] && pszValueName)
		{
			pszRegKey = pszValueName;
			pszValueName = "";
		}
	}
	else
	{
		hroot = HKEY_LOCAL_MACHINE;
	}

	// check if the caller wanted to force the 32 bit or 64 bit registry
	//
	int force = 0;
	if ((options & 32) || (options & KEY_WOW64_32KEY))
		force = KEY_WOW64_32KEY;
	else if ((options & 64) || (options & KEY_WOW64_64KEY))
		force = KEY_WOW64_64KEY;

	bool fAutoEnumValues = ((options & 1) != 0);
	bool fEnumSubkeys = ((options & 2) != 0);
	bool fEnumValues =	((options & 4) != 0);

	// try and open the key
	//
	HKEY hkey = NULL;
	LONG lres = RegOpenKeyEx(hroot, pszRegKey, 0, KEY_READ | force, &hkey);

	// if the key wasn't found, and the caller didn't pass a value name, perhaps
	// the last element of the registry path is really a value name, try opening
	// the parent of the passed in key. if that works, set the keyname to the
	// parent and the valuename to the child and continue on.
	//
	if ((ERROR_FILE_NOT_FOUND == lres) && ! pszValueName)
	{
		pszTemp = strdup(pszRegKey); // this gets free'd as we leave the function.
		char * pszName = strrchr(pszTemp, '\\');
		if (pszName)
		{
           *pszName++ = 0;
			lres = RegOpenKeyEx(hroot, pszTemp, 0, KEY_READ | force, &hkey);
			if (ERROR_SUCCESS == lres)
			{
				pszRegKey = pszTemp;
				pszValueName = pszName;
			}
		}
	}

	// if we failed to open the base key, log the error and fall down
	// to return null. otherwise try and read the value indicated
	// by pszValueName.
	// 
	if (ERROR_SUCCESS != lres)
	{
		if (ERROR_FILE_NOT_FOUND == lres)
		{
			// the key did not exist.
			dprintf( D_FULLDEBUG, 
 				"The Registry Key \"%s\\%s\" does not exist\n",
					root_key_name(hroot), pszRegKey);
		}
		else
		{
			// there was some other error, probably an access violation.
			char szMsg[MAX_PATH];
			dprintf( D_ALWAYS, 
				"Failed to open Registry Key \"%s\\%s\"\nReason: %s\n",
					root_key_name(hroot), pszRegKey, 
					GetLastErrMessage(GetLastError(), szMsg, NUM_ELEMENTS(szMsg)));
		}
		ASSERT( ! value);
	}
	else
	{
		ULARGE_INTEGER uli; // in case we need to read a dword/qword value.
		DWORD vtype = REG_SZ, cbData = 0;
		const char * pszName = pszValueName; // so we can switch to NULL if we need to
		lres = RegQueryValueEx(hkey, pszName, NULL, &vtype, NULL, &cbData);

		// if the named value was not found, check to see if it's actually a subkey
		// if it is, open the subkey, set the valuename to null, and continue on
		// as if that was what the caller passed to begin with.
		//
		if ((ERROR_FILE_NOT_FOUND == lres) && pszName && pszName[0])
		{
			HKEY hkeyT = NULL;
			LONG lResT = RegOpenKeyEx(hkey, pszName, 0, KEY_READ | force, &hkeyT); 
			if (ERROR_SUCCESS == lResT)
			{
				// yep, it's a key, pretend that we opened it rather than its parent.
				RegCloseKey(hkey);
				hkey = hkeyT;  // so the code below will close this for us.

				pszName = NULL;
				cbData = 0;
				lres = RegQueryValueEx(hkey, pszName, NULL, &vtype, NULL, &cbData);
			}
		}

		// the value exists, but we did not retrieve it yet, we need to allocate
		// space and then queryvalue again. 
		//
		if (ERROR_MORE_DATA == lres || ERROR_SUCCESS == lres)
		{
			if (vtype == REG_MULTI_SZ || vtype == REG_SZ || vtype == REG_EXPAND_SZ || vtype == REG_LINK || vtype == REG_BINARY)
			{
				value = (char*)malloc(cbData+1);	
				lres = RegQueryValueEx(hkey, pszName, NULL, &vtype, (byte*)value, &cbData);
			}
			else
			{
				cbData = sizeof(uli);
				uli.QuadPart = 0; // in case we don't read the whole 8 bytes.
				lres = RegQueryValueEx(hkey, pszName, NULL, &vtype, (byte*)&uli, &cbData);
			}
		}

		if (ERROR_FILE_NOT_FOUND == lres)
		{
			if (pszName && pszName[0])
			{
				// the named value does not exist
				//
				dprintf( D_FULLDEBUG, 
 					"The Registry Key \"%s\\%s\" does not have a value named \"%s\"\n",
						root_key_name(hroot), pszRegKey, pszName);
			}
			else
			{
				// the key exists, but it has no default value. This is common for keys
				// To distinquish between this case and 'the key does not exist'
				// we will return a "" string here.
				//
				value = strdup("");
				dprintf ( D_FULLDEBUG, "The Registry Key \"%s\\%s\\%s\" has no default value.\n", 
					root_key_name(hroot), pszRegKey, pszValueName);
				fEnumValues = fAutoEnumValues;
			}
		}
		else if (ERROR_SUCCESS != lres)
		{
			// there was some other error, probably an access violation.
			//
			char szMsg[MAX_PATH];
			dprintf( D_ALWAYS, 
				"Failed to read Registry Key \"%s\\%s\\%s\"\nReason: %s\n", 
					root_key_name(hroot), pszRegKey, pszValueName, 
					GetLastErrMessage(GetLastError(), szMsg, NUM_ELEMENTS(szMsg)));
		}
		else
		{
			// we got a value, now have to turn it into a string.
			//
			char sz[10] = "";
			switch (vtype)
			{
			case REG_LINK:
			case REG_SZ:
				break;
			case REG_EXPAND_SZ:
				// TJ: write this.
				break;
			case REG_MULTI_SZ:
				// TJ: write this.
				break;

			case REG_DWORD_BIG_ENDIAN:
				uli.LowPart = htonl(uli.LowPart);
				// fall though
			case REG_DWORD:
				wsprintf(sz, "%u", uli.LowPart);
				break;

			case REG_QWORD:
				wsprintf(sz, "%lu", uli.QuadPart);
				break;

			case REG_BINARY:
				break;
			}

			if ( ! value) 
				value = strdup(sz);

			dprintf ( D_FULLDEBUG, "The Registry Key \"%s\\%s\\%s\" contains \"%s\"\n", 
				root_key_name(hroot), pszRegKey, pszValueName, value);
		}

		// In fulldebug mode, enumerate the value names and subkey names
		// if requested.
		//
		if (DebugFlags & D_FULLDEBUG) 
		{
			if (fEnumValues)
			{
				int ii = 0;
				for (ii = 0; ii < 10000; ++ii)
				{
					TCHAR szName[MAX_PATH];
					DWORD cchName = NUM_ELEMENTS(szName), vt, cbData = 0;
					lres = RegEnumValue(hkey, ii, szName, &cchName, 0, &vt, NULL, &cbData);
					if (ERROR_NO_MORE_ITEMS == lres)
						break;
					if ( ! ii) dprintf ( D_FULLDEBUG, " Named values:\n");
					dprintf( D_FULLDEBUG, "  \"%s\" = %d bytes\n", szName, cbData);
				}
				if ( ! ii) dprintf ( D_FULLDEBUG, " No Named values\n");
			}
			if (fEnumSubkeys)
			{
				int ii = 0;
				for (ii = 0; ii < 10000; ++ii)
				{
					TCHAR szName[MAX_PATH]; 
					DWORD cchName = NUM_ELEMENTS(szName);
					lres = RegEnumKeyEx(hkey, ii, szName, &cchName, NULL, NULL, NULL, NULL);
					if (ERROR_NO_MORE_ITEMS == lres)
						break;
					if ( ! ii) dprintf ( D_FULLDEBUG, " Subkeys:\n");
					dprintf( D_FULLDEBUG, "  \"%s\"\n", szName);
				}
				if ( ! ii) dprintf ( D_FULLDEBUG, " No Subkeys\n");
			}
		}

		RegCloseKey(hkey);
	}
	// free temp buffer that we may have used to crack the key
	if (pszTemp) free(pszTemp);

	return value;
}

// generate an ClassAd attribute name from a registry key name.
// we do this by first checking to see if the input is of the form attr=reg_path
// if it is, just extract attr, otherwise we extract the last N pieces of 
// the registry path and convert characters that aren't allowed in attribute names
// to _.  
// 
// returns an allocated string that is the concatinaton of prefix and attr
//
char * generate_reg_key_attr_name(const char * pszPrefix, const char * pszKeyName)
{
	int cchPrefix = pszPrefix ? strlen(pszPrefix) : 0;

	// is the input of the form attr_name=reg_path?  if so, then
	// we want to return prefix + attr_name. 
	//
	const char * psz = strchr(pszKeyName, '=');
	const char * pbs = strchr(pszKeyName, '\\');
	if (psz && ( ! pbs || psz < pbs))
	{
		int    cch = (psz - pszKeyName);
		char * pszAttr = (char*)malloc(cchPrefix + cch +1);
		if (pszPrefix)
			strcpy(pszAttr, pszPrefix);
		memcpy(pszAttr + cchPrefix, pszKeyName, cch);
		pszAttr[cchPrefix + cch] = 0;
		while (cch > 0 && isspace(pszAttr[cchPrefix+cch-1]))
		{
			pszAttr[cchPrefix+cch-1] = 0;
		    --cch;
		}
		return pszAttr;
	}
	
	// no explicit attr_name, so generate it from the last N parts
	// of the keyname. we start by walking backward counting \ until
	// we hit N or the start of the input.
	//
	psz = pszKeyName + strlen(pszKeyName);
	int cSteps = 3;
	while (psz > pszKeyName && (psz[-1] != '\\' || --cSteps > 0))
		--psz;

	// the identifier can't start with a digit, so back up one more
	// character if it does. (psz should end up pointing to a \)
	if ( ! cchPrefix && psz > pszKeyName && isdigit(*psz))
		--psz;

	// allocate space for prefix + key_part and copy
	// both into the allocated buffer.
	//
	int cch = strlen(psz);
	char * pszAttr = (char *)malloc(cchPrefix + cch + 1);
    if (pszPrefix)
		strcpy(pszAttr, pszPrefix);
	memcpy(pszAttr + cchPrefix, psz, cch);
	pszAttr[cchPrefix + cch] = 0;

	// a bit of a special case, if the keyname ends in a "\"
	// pretend that the last slash isn't there.
	//
	if (cch > 1 && pszAttr[cchPrefix + cch-1] == '\\')
		pszAttr[cchPrefix + cch-1] = 0;

	// only A-Za-z0-9 are valid for identifiers
	// convert other characters into _
	//
	char * pszT = pszAttr;
	while (*pszT)
	{ 
		if ( ! isalnum(*pszT))
			pszT[0] = '_';
		++pszT;
	}

	return pszAttr;
}

#endif // WIN32

void
MachAttributes::publish( ClassAd* cp, amask_t how_much) 
{
	if( IS_STATIC(how_much) || IS_PUBLIC(how_much) ) {

			// STARTD_IP_ADDR
		cp->Assign( ATTR_STARTD_IP_ADDR,
					daemonCore->InfoCommandSinfulString() );

        cp->Assign( ATTR_ARCH, m_arch );

		cp->Assign( ATTR_OPSYS, m_opsys );

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

		// publish values from the window's registry as specified
		// in the MACHINE_AD_REGISTRY_KEYS param.
		//
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

#endif

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
	if ( count ) {
		rip->change_state( benchmarking_act );
	}
	bench_job_mgr->StartBenchmarks( rip, count );

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
							  int num_cpus, 
							  int num_phys_mem,
							  float virt_mem_fraction,
							  float disk_fraction,
							  MyString &execute_dir,
							  MyString &execute_partition_id )
{
	map = map_arg;
	c_type = slot_type;
	c_num_cpus = num_cpus;
	c_phys_mem = num_phys_mem;
	c_virt_mem_fraction = virt_mem_fraction;
	c_disk_fraction = disk_fraction;
	c_execute_dir = execute_dir;
	c_execute_partition_id = execute_partition_id;
	c_idle = -1;
	c_console_idle = -1;
	c_disk = 0;
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
		if( DebugFlags & D_LOAD ) {
			dprintf( D_FULLDEBUG, 
					 "%s %.2f  %s %.2f  %s %.2f\n",  
					 "SystemLoad:", c_condor_load + c_owner_load,
					 "CondorLoad:", c_condor_load,
					 "OwnerLoad:", c_owner_load );
		}
		if( DebugFlags & D_KEYBOARD ) {
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
			 "Cpus: %d", c_num_cpus);

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
				   ", Disk: auto\n" );
	}
	else {
		::dprintf( dflag | D_NOHEADER, 
				   ", Disk: %.2f%%\n", 100*c_disk_fraction );
	}
}


void
CpuAttributes::dprintf( int flags, const char* fmt, ... )
{
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

	compute( A_TIMEOUT | A_UPDATE ); // Re-compute

	return *this;
}

AvailAttributes::AvailAttributes( MachAttributes* map ):
	m_execute_partitions(500,MyStringHash,updateDuplicateKeys)
{
	a_num_cpus = map->num_cpus();
	a_phys_mem = map->phys_mem();
	a_phys_mem_auto_count = 0;
	a_virt_mem_fraction = 1.0;
	a_virt_mem_auto_count = 0;
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

	if( new_cpus < floor || new_phys_mem < floor || 
		new_virt_mem < floor || new_disk < floor ) {
		return false;
	} else {
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
		cap->c_phys_mem = new_value;
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
	return true;
}


void
AvailAttributes::show_totals( int dflag, CpuAttributes *cap )
{
	AvailDiskPartition &partition = GetAvailDiskPartition( cap->c_execute_partition_id );
	::dprintf( dflag | D_NOHEADER, 
			 "Cpus: %d, Memory: %d, Swap: %.2f%%, Disk: %.2f%%\n",
			 a_num_cpus, a_phys_mem, 100*a_virt_mem_fraction,
			 100*partition.m_disk_fraction );
}
