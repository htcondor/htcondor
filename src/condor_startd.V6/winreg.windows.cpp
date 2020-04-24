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

/* 
    This file contains code used to gather values
    from the Windows registry and put them in a form that is convenient
    to publish in a ClassAd

   	Written 2010 by John (TJ) Knoeller <johnkn@cs.wisc.edu>
*/

#include "condor_common.h"
#include "startd.h"
#include <math.h>

// for the performance registry, we need to compare 2 query results
// separated by time to get a reading. this struct holds both query results
//
class WinPerf_Data;
typedef struct _WinPerf_QueryResult {
	DWORD                idObject; // ObjectNameIndex for the query (e.g. "Processor" is 238)
	DWORD                cbAlloc;  // allocation size of the object data
	const WinPerf_Data * pdata1;   // data_block for the older
	const WinPerf_Data * pdata2;   // data_block for the newer query
}   WinPerf_QueryResult;

// a registry query into HKEY_PERFORMANCE_DATA bakes down into this
// structure.  "HKPD\System\Processes" 
// becomes idKey == 2  idCounter = 248 Processes 
//
struct WinPerf_Query
{
	DWORD idKey;    // identifies counter group (WinPerf_Object)
	DWORD idCounter;// identifies counter (WinPerf_CounterDef)
	DWORD idAlt[6]; // alternate counter ids because there are multiple ids that match a single string.
	DWORD idInst;   // instance unique id (if available)
    int   ixInst;   // offset into the original string of the instance name
	int   cchInst;  // size of the instance name from the original string.
};

#include <hashtable.h>

// D_NORMAL can be set to D_ALWAYS to cause a LOT more output from the WinReg code
#define D_NORMAL D_FULLDEBUG

// these flags can be passes to some of the registry querying functions to get 
// diagnostic output.  they would not normally be used in shipping code.
//
#define WINREG_OPT_F_AUTO_ENUM_VALUES 1
#define WINREG_OPT_F_ENUM_SUBKEYS	  2
#define WINREG_OPT_F_ENUM_VALUES      4
#define WINREG_OPT_F_WOW32           32
#define WINREG_OPT_F_WOW64           64


// parse the start of a string and return hive key if the start matches
// one of the known hive designators HKLM for HKEY_LOCAL_MACHINE, etc.
// if the caller passes pixPrefixSep, then this function will also return
// the index of the first character after the hive prefix, which should be
// \0, \ or /. 
HKEY parse_hive_prefix(const char * psz, int cch, int * pixPrefixSep)
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
				'HKLM',	HKEY_LOCAL_MACHINE,
				'HKCC',	HKEY_CURRENT_CONFIG,
				'HKCR', HKEY_CLASSES_ROOT,
				'HKCU', HKEY_CURRENT_USER,
				'HKU\0',HKEY_USERS,
				'HKPD',	HKEY_PERFORMANCE_DATA,
				//'HKPT',	HKEY_PERFORMANCE_TEXT,
				//'HKPN',	HKEY_PERFORMANCE_NLSTEXT,
				//Win9x only 'HKDD', HKEY_DYN_DATA,
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
// we use this for logging.
//
static const char * root_key_name(HKEY hroot)
{
	static const struct {
		const char * psz;
		HKEY key;
	} aMap[] = {
		"HKLM",	HKEY_LOCAL_MACHINE,
		"HKCC",	HKEY_CURRENT_CONFIG,
		"HKCU", HKEY_CURRENT_USER,
		"HKCR", HKEY_CLASSES_ROOT,
		"HKU\0",	HKEY_USERS,
		"HKPD",	HKEY_PERFORMANCE_DATA,
		//"HKPT",	HKEY_PERFORMANCE_TEXT,
		//"HKPN",	HKEY_PERFORMANCE_NLSTEXT,
		//Win9x only "HKDD", HKEY_DYN_DATA,
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
char * get_windows_reg_value(
	const char * pszRegKey, 
	const char * pszValueName, 
	int          options /* = 0*/)
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

	if (HKEY_PERFORMANCE_DATA == hroot ||
		HKEY_PERFORMANCE_TEXT == hroot ||
		HKEY_PERFORMANCE_NLSTEXT == hroot)
	{
		dprintf( D_FULLDEBUG, 
 				"get_windows_reg_value() does not support \"%s\\%s\"\n",
				root_key_name(hroot), pszRegKey);
		return NULL;
	}

	// check if the caller wanted to force the 32 bit or 64 bit registry
	//
	int force = 0;
	if ((options & WINREG_OPT_F_WOW32) || (options & KEY_WOW64_32KEY))
		force = KEY_WOW64_32KEY;
	else if ((options & WINREG_OPT_F_WOW64) || (options & KEY_WOW64_64KEY))
		force = KEY_WOW64_64KEY;

	bool fAutoEnumValues = ((options & WINREG_OPT_F_AUTO_ENUM_VALUES) != 0);
	bool fEnumSubkeys = ((options & WINREG_OPT_F_ENUM_SUBKEYS) != 0);
	bool fEnumValues =	((options & WINREG_OPT_F_ENUM_VALUES) != 0);

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
			uli.QuadPart = 0; // make sure that our volatile uli is zero'ed out.
			if (vtype == REG_MULTI_SZ || vtype == REG_SZ || vtype == REG_EXPAND_SZ || vtype == REG_LINK || vtype == REG_BINARY)
			{
				value = (char*)malloc(cbData+1);	
				lres = RegQueryValueEx(hkey, pszName, NULL, &vtype, (byte*)value, &cbData);
			}
			else
			{
				cbData = sizeof(uli);
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
			char sz[20] = "";
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
				// htonl converts from native endian to big-endian (i.e network byte order)
				uli.LowPart = htonl(uli.LowPart);
				// fall though
			case REG_DWORD:
				wsprintf(sz, "%u", uli.LowPart);
				break;

			case REG_QWORD:
				wsprintf(sz, "%I64u", uli.QuadPart);
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
		if (IsDebugLevel(D_NORMAL))
		{
			if (fEnumValues)
			{
				int ii = 0;
				for (ii = 0; ii < 10000; ++ii)
				{
					TCHAR szName[MAX_PATH];
					DWORD cchName = NUM_ELEMENTS(szName), vt, cb = 0;
					lres = RegEnumValue(hkey, ii, szName, &cchName, 0, &vt, NULL, &cb);
					if (ERROR_NO_MORE_ITEMS == lres)
						break;
					if ( ! ii) dprintf ( D_NORMAL, " Named values:\n");
					dprintf( D_FULLDEBUG, "  \"%s\" = %d bytes\n", szName, cb);
				}
				if ( ! ii) dprintf ( D_NORMAL, " No Named values\n");
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
					if ( ! ii) dprintf ( D_NORMAL, " Subkeys:\n");
					dprintf( D_NORMAL, "  \"%s\"\n", szName);
				}
				if ( ! ii) dprintf ( D_NORMAL, " No Subkeys\n");
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
	size_t cchPrefix = pszPrefix ? strlen(pszPrefix) : 0;

	// is the input of the form attr_name=reg_path?  if so, then
	// we want to return prefix + attr_name. 
	//
	const char * psz = strchr(pszKeyName, '=');
	const char * pbs = strchr(pszKeyName, '\\');
	if (psz && ( ! pbs || psz < pbs))
	{
		int    cch = (psz - pszKeyName);
		char * pszAttr = (char*)malloc(cchPrefix + cch +1);
		if ( ! pszAttr) return NULL;
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
	int cSteps = (strstr(pszKeyName, "HKPD\\") == pszKeyName) ? 2 : 3;
	while (psz > pszKeyName && (psz[-1] != '\\' || --cSteps > 0))
		--psz;

	// the identifier can't start with a digit, so back up one more
	// character if it does. (psz should end up pointing to a \)
	if ( ! cchPrefix && psz > pszKeyName && isdigit(*psz))
		--psz;

	// allocate space for prefix + key_part and copy
	// both into the allocated buffer.
	//
	size_t cch = strlen(psz);
	bool fPercent = false;
	if (strchr(psz, '%'))
	{
		cch += strlen("Percent");
		fPercent = true;
	}

	bool fPerSec = false;
	if (strstr(psz, "/sec"))
	{
		cch += strlen("_Per_");
		fPerSec = true;
	}

	char * pszAttr = (char *)malloc(cchPrefix + cch + 1);
	if ( ! pszAttr) return NULL;
	if (pszPrefix)
		strcpy(pszAttr, pszPrefix);

	// a bit of a special case, if the keyname ends in a "\"
	// pretend that the last slash isn't there.
	//
	if (cch > 1 && pszAttr[cchPrefix + cch-1] == '\\')
		pszAttr[cchPrefix + cch-1] = 0;

	// only A-Za-z0-9 are valid for identifiers
	// convert other characters into _
	//
	char * pszT = pszAttr + cchPrefix;
	while (*pszT = *psz)
	{ 
		if (fPercent && *pszT == '%')
		{
			strcpy(pszT, "Percent");
			pszT += strlen(pszT)-1;
			fPercent = false;
		}
		else if (fPerSec && *pszT == '/' &&	strstr(psz, "sec") == psz+1)
		{
			strcpy(pszT, "_Per_S");
			pszT += strlen(pszT)-1;
			psz += 1;
			fPerSec = false;
		}
		else if ( ! isalnum(*pszT))
		{
			// don't generate more than 1 consecutive _
			if (pszT > pszAttr && pszT[-1] == '_')
				--pszT;
			pszT[0] = '_';
		}
		++pszT;
		++psz;
	}

	// a bit of a special case, remove the last trailing _ from 
	// the attribute name.  We do this so that if a the keyname ends in a \
	// we don't end up with a trailing _ on the attrib name.
	//
	cch = strlen(pszAttr);
	if (cch > 0 && pszAttr[cch-1] == '_')
		pszAttr[cch-1] = 0;

	return pszAttr;
}


static size_t
DWORDHash( const DWORD & n )
{
	return n;
}

#if 1 // def TIMER_TYPE_NAME_TABLE
static const struct {
	DWORD type;
	char * psz;
} aPerfTimerTypeNames[] = {

#define _TABLE_ITEM(a) a, #a 

// 32-bit Counter.  Divide delta by delta time.  Display suffix: "/sec"
_TABLE_ITEM(PERF_COUNTER_COUNTER),

// 64-bit Timer.  Divide delta by delta time.  Display suffix: "%"
_TABLE_ITEM(PERF_COUNTER_TIMER),
// Queue Length Space-Time Product. Divide delta by delta time. No Display Suffix.
_TABLE_ITEM(PERF_COUNTER_QUEUELEN_TYPE),
// Queue Length Space-Time Product. Divide delta by delta time. No Display Suffix.
_TABLE_ITEM(PERF_COUNTER_LARGE_QUEUELEN_TYPE),
// Queue Length Space-Time Product using 100 Ns timebase.
// Divide delta by delta time. No Display Suffix.
_TABLE_ITEM(PERF_COUNTER_100NS_QUEUELEN_TYPE),
// Queue Length Space-Time Product using Object specific timebase.
// Divide delta by delta time. No Display Suffix.
_TABLE_ITEM(PERF_COUNTER_OBJ_TIME_QUEUELEN_TYPE),
// 64-bit Counter.  Divide delta by delta time. Display Suffix: "/sec"
_TABLE_ITEM(PERF_COUNTER_BULK_COUNT),
// Indicates the counter is not a  counter but rather Unicode text Display as text.
_TABLE_ITEM(PERF_COUNTER_TEXT),
// Indicates the data is a counter  which should not be
// time averaged on display (such as an error counter on a serial line),
// Display as is.  No Display Suffix.
_TABLE_ITEM(PERF_COUNTER_RAWCOUNT),
// Same as _TABLE_ITEM(PERF_COUNTER_RAWCOUNT except its size is a large integer
_TABLE_ITEM(PERF_COUNTER_LARGE_RAWCOUNT),
// Special case for RAWCOUNT that want to be displayed in hex
// Indicates the data is a counter  which should not be
// time averaged on display (such as an error counter on a serial line),
// Display as is.  No Display Suffix.
_TABLE_ITEM(PERF_COUNTER_RAWCOUNT_HEX),
// Same as _TABLE_ITEM(PERF_COUNTER_RAWCOUNT_HEX except its size is a large integer
_TABLE_ITEM(PERF_COUNTER_LARGE_RAWCOUNT_HEX),

// A count which is either 1 or 0 on each sampling interrupt (% busy),
// Divide delta by delta base. Display Suffix: "%"
_TABLE_ITEM(PERF_SAMPLE_FRACTION),
// A count which is sampled on each sampling interrupt (queue length),
// Divide delta by delta time. No Display Suffix.
_TABLE_ITEM(PERF_SAMPLE_COUNTER),
// A label: no data is associated with this counter (it has 0 length),
// Do not display.
_TABLE_ITEM(PERF_COUNTER_NODATA),
// 64-bit Timer inverse (e.g., idle is measured, but display busy %),
// Display 100 - delta divided by delta time.  Display suffix: "%"
_TABLE_ITEM(PERF_COUNTER_TIMER_INV),
// The divisor for a sample, used with the previous counter to form a
// sampled %.  You must check for >0 before dividing by this!  This
// counter will directly follow the  numerator counter.  It should not
// be displayed to the user.
_TABLE_ITEM(PERF_SAMPLE_BASE),

// A timer which, when divided by an average base, produces a time
// in seconds which is the average time of some operation.  This
// timer times total operations, and  the base is the number of opera-
// tions.  Display Suffix: "sec"
_TABLE_ITEM(PERF_AVERAGE_TIMER),
// Used as the denominator in the computation of time or count
// averages.  Must directly follow the numerator counter.  Not dis-
// played to the user.
_TABLE_ITEM(PERF_AVERAGE_BASE),

// A bulk count which, when divided (typically), by the number of
// operations, gives (typically), the number of bytes per operation.
// No Display Suffix.
_TABLE_ITEM(PERF_AVERAGE_BULK),
// 64-bit Timer in object specific units. Display delta divided by
// delta time as returned in the object type header structure.  Display suffix: "%"
_TABLE_ITEM(PERF_OBJ_TIME_TIMER),

// 64-bit Timer in 100 nsec units. Display delta divided by
// delta time.  Display suffix: "%"
_TABLE_ITEM(PERF_100NSEC_TIMER),
// 64-bit Timer inverse (e.g., idle is measured, but display busy %),
// Display 100 - delta divided by delta time.  Display suffix: "%"
_TABLE_ITEM(PERF_100NSEC_TIMER_INV),
// 64-bit Timer.  Divide delta by delta time.  Display suffix: "%"
// Timer for multiple instances, so result can exceed 100%.
_TABLE_ITEM(PERF_COUNTER_MULTI_TIMER),
// 64-bit Timer inverse (e.g., idle is measured, but display busy %),
// Display 100 * _MULTI_BASE - delta divided by delta time.
// Display suffix: "%" Timer for multiple instances, so result
// can exceed 100%.  Followed by a counter of type _MULTI_BASE.
_TABLE_ITEM(PERF_COUNTER_MULTI_TIMER_INV),
// Number of instances to which the preceding _MULTI_..._INV counter
// applies.  Used as a factor to get the percentage.
_TABLE_ITEM(PERF_COUNTER_MULTI_BASE),
// 64-bit Timer in 100 nsec units. Display delta divided by delta time.
// Display suffix: "%" Timer for multiple instances, so result can exceed 100%.
_TABLE_ITEM(PERF_100NSEC_MULTI_TIMER),
// 64-bit Timer inverse (e.g., idle is measured, but display busy %),
// Display 100 * _MULTI_BASE - delta divided by delta time.
// Display suffix: "%" Timer for multiple instances, so result
// can exceed 100%.  Followed by a counter of type _MULTI_BASE.
_TABLE_ITEM(PERF_100NSEC_MULTI_TIMER_INV),
// Indicates the data is a fraction of the following counter  which
// should not be time averaged on display (such as free space over
// total space.), Display as is.  Display the quotient as "%".
_TABLE_ITEM(PERF_RAW_FRACTION),
_TABLE_ITEM(PERF_LARGE_RAW_FRACTION),
// Indicates the data is a base for the preceding counter which should
// not be time averaged on display (such as free space over total space.),
_TABLE_ITEM(PERF_RAW_BASE),
_TABLE_ITEM(PERF_LARGE_RAW_BASE),
// The data collected in this counter is actually the start time of the
// item being measured. For display, this data is subtracted from the
// sample time to yield the elapsed time as the difference between the two.
// In the definition below, the PerfTime field of the Object contains
// the sample time as indicated by the _TABLE_ITEM(PERF_OBJECT_TIMER bit and the
// difference is scaled by the PerfFreq of the Object to convert the time
// units into seconds.
_TABLE_ITEM(PERF_ELAPSED_TIME),
//  The following counter type can be used with the preceding types to
//  define a range of values to be displayed in a histogram.
//

_TABLE_ITEM(PERF_COUNTER_HISTOGRAM_TYPE),
//
//  This counter is used to display the difference from one sample
//  to the next. The counter value is a constantly increasing number
//  and the value displayed is the difference between the current
//  value and the previous value. Negative numbers are not allowed
//  which shouldn't be a problem as long as the counter value is
//  increasing or unchanged.
//
_TABLE_ITEM(PERF_COUNTER_DELTA),
_TABLE_ITEM(PERF_COUNTER_LARGE_DELTA),
//  The precision counters are timers that consist of two counter values:
//      1), the count of elapsed time of the event being monitored
//      2), the "clock" time in the same units
//
//  the precition timers are used where the standard system timers are not
//  precise enough for accurate readings. It's assumed that the service
//  providing the data is also providing a timestamp at the same time which
//  will eliminate any error that may occur since some small and variable
//  time elapses between the time the system timestamp is captured and when
//  the data is collected from the performance DLL. Only in extreme cases
//  has this been observed to be problematic.
//
//  when using this type of timer, the definition of the
//      _TABLE_ITEM(PERF_PRECISION_TIMESTAMP counter must immediately follow the
//      definition of the _TABLE_ITEM(PERF_PRECISION_*_TIMER in the Object header
//
// The timer used has the same frequency as the System Performance Timer
_TABLE_ITEM(PERF_PRECISION_SYSTEM_TIMER),
// The timer used has the same frequency as the 100 NanoSecond Timer
_TABLE_ITEM(PERF_PRECISION_100NS_TIMER),
//
// The timer used is of the frequency specified in the Object header's
//  PerfFreq field (PerfTime is ignored),
_TABLE_ITEM(PERF_PRECISION_OBJECT_TIMER),
//
// This is the timestamp to use in the computation of the timer specified
// in the previous description block
_TABLE_ITEM(PERF_PRECISION_TIMESTAMP),
};

static void dump_perf_timer_type_name_table() 
{
	dprintf(D_ALWAYS, "Perf Timer Type Names:\n");
	for (int ii = 0; ii < NUM_ELEMENTS(aPerfTimerTypeNames); ++ii)
	{
		dprintf(D_ALWAYS, "0x%08X %s\n", aPerfTimerTypeNames[ii].type, aPerfTimerTypeNames[ii].psz);
	}
}
static const char * perf_timer_type_name(DWORD type)
{
	for (int ii = 0; ii < NUM_ELEMENTS(aPerfTimerTypeNames); ++ii)
	{
		if (type == aPerfTimerTypeNames[ii].type)
			return aPerfTimerTypeNames[ii].psz;
	}
	return "";
}
#endif //def TIMER_TYPE_NAME_TABLE

/* unused.
typedef struct _WinPerf_RawCounterData {
	DWORD     type;   // CounterType
	DWORD     more;   // second value for multi-data
	ULONGLONG data;	  // counter data
	LONGLONG  time;   // timestamp or base value
	LONGLONG  freq;   // frequency
}  WinPerf_RawCounterData;
*/

enum {
	WinPerf_CT_Empty       = 0,  // there is no value
	WinPerf_CT_Unsigned    = 1,  // use the 64 bit unsigned value
	WinPerf_CT_Signed      = 2,  // use the 64 bit signed value
	WinPerf_CT_Double      = 3,  // use the double value

	WinPerf_CT_Decimal     = 0x00,  // print as decimal value
	WinPerf_CT_Hex         = 0x10,  // print as hex value
	WinPerf_CT_Bytes       = 0x20,  // value is in bytes (can be converted to MB)
	WinPerf_CT_Short       = 0x40,  // value is only 32 bits

	WinPerf_CT_Seconds     = 0x100, // append "sec" when printing
	WinPerf_CT_PerSecond   = 0x200, // append "/sec" when printing
	WinPerf_CT_Percent     = 0x300, // append "%" when printing
	WinPerf_CT_UnitsMask   = 0xF00, // mask of the units(sec/persec/percent)
};

typedef int WinPerf_CounterValueType;

typedef struct _WinPerf_CounterValue {
	WinPerf_CounterValueType type;
	int                      spare;
	union {
		int       i;
		UINT      u;
		LONGLONG  ll;
		ULONGLONG ul;
		double    d;
	} value;
	int Print(char * psz, int cch, bool fIncludeUnits) const;
} WinPerf_CounterValue;

typedef struct _WinPerf_TimerDeltas {
	LONGLONG obj;	  // object timer delta
	LONGLONG objabs;  // object absolute time
	LONGLONG objfreq; // object timer frequency
	LONGLONG head;    // head timer delta
	LONGLONG headfreq;// head timer frequency
	LONGLONG nanos;   // head 100ns timer delta
} WinPerf_TimerDeltas;


// global variables used by code that queries the windows performance counters
//
static struct {
	// keep track of the set of name/index pairs that we get from querying
	// the performance registry.  We keep that set of strings in pszzNames
	// and we hash name->index and index->name in two hashtables. 
	char * pszzNames; // holds all of the strings that the two hash tables refer to.
	std::map<YourStringNoCase, std::vector<const char *>> * pPerfTable;
	HashTable<DWORD, const char *> * pNameTable;
    HashTable<DWORD, WinPerf_QueryResult> * pQueries;
} rl = {0};

// build a hashtable that maps all of the performance field names to their
// index value and visa versa. 
//
static bool init_windows_performance_hashtable()
{
	if (rl.pPerfTable)
		return true;

	//dump_perf_timer_type_name_table();

	DWORD cbData = 0;
	DWORD vtype = REG_SZ; 
	LONG lres = RegQueryValueEx(HKEY_PERFORMANCE_TEXT, "Counter", NULL, &vtype, NULL, &cbData);
	if (ERROR_SUCCESS == lres)
	{
		rl.pszzNames = (char*)malloc(cbData);
		lres = RegQueryValueEx(HKEY_PERFORMANCE_TEXT, "Counter", NULL, &vtype, (byte*)rl.pszzNames, &cbData);
		if (ERROR_SUCCESS != lres)
		{
			if (rl.pszzNames) free (rl.pszzNames);
			rl.pszzNames = NULL;
		}
	}
	if (ERROR_SUCCESS != lres)
	{
		// there was some other error, probably an access violation.
		char szMsg[MAX_PATH];
		dprintf( D_ALWAYS, 
			"Failed to read Performance Text! Reason: %s\n",
				GetLastErrMessage(GetLastError(), szMsg, NUM_ELEMENTS(szMsg)));
	}
	else if (REG_MULTI_SZ == vtype)
	{
		rl.pQueries   = new HashTable<DWORD, WinPerf_QueryResult>(DWORDHash);
		rl.pPerfTable = new std::map<YourStringNoCase, std::vector<const char *>>();
		rl.pNameTable = new HashTable<DWORD, const char *>(DWORDHash);
		if (rl.pPerfTable)
		{
			char * psz = rl.pszzNames;
			while (psz && *psz)
			{
				char * pszIndex = psz;
				char * pszName = psz + lstrlen(psz)+1;
				if (*pszName) psz = pszName + lstrlen(pszName)+1;
				(*rl.pPerfTable)[pszName].push_back(pszIndex);
				if (rl.pNameTable)
				{
					DWORD ix = atoi(pszIndex);
					rl.pNameTable->insert(ix, pszName);
				}
			}
		}
	}
	return (rl.pPerfTable != NULL);
}

// these classes wrap the PERF_xxx structures that we get back from registry queries
// on the HKEY_PERFORMANCE_DATA key, they expose methods that make traversal of the
// structures a bit simpler.  Each registry query returns WinPerf_Data, which
// contains one or more WinPerf_Object each of which contains one or more
// WinPerf_CounterDef structures that describe the layout of the CounterData. 
// each WinPerf_Object also has one or more sets of CounterData, either as a set
// of WinPerf_ObjInst, each of which has an instance name and a set of CounterData
// (for things like HKPD\Processes).  Or the WinPerf_Object has a single set of CounterData
// (for things like HKPD\Memory).
//

class WinPerf_CounterDef : public PERF_COUNTER_DEFINITION
{
public:
	DWORD NameIndex() const {
		return this->CounterNameTitleIndex; 
	}
};

class WinPerf_ObjInst : public PERF_INSTANCE_DEFINITION
{
public:
	const WinPerf_ObjInst * Next() const { 
		return (const WinPerf_ObjInst*)((BYTE*)this + this->ByteLength + CounterBytes());
	}
	const WCHAR *           Name() const { 
		if (this->NameLength <= 0)
			return NULL;
		return (const WCHAR *)((BYTE*)this + this->NameOffset); 
	}

	// do case-insensitive comparison between the instance name, which is unicode
	// and a supplied value which is ascii. no multi-byte character support.
	// ignore () on the input, treat a closing ) on input as if it were a null.
	// treat a * as 'match all of the rest' (characters after the * are ignored)
	//
	bool Matches(const char * psz) const {
		if (psz && *psz == '*')
			return true;
		if (this->NameLength <= 0 || ! psz)
			return false;
		if (*psz == '(')
			++psz;
		const WCHAR * pwsz = (const WCHAR *)((BYTE*)this + this->NameOffset); 
		if ( ! *pwsz)
			return false;
		while (*pwsz) {
			if ( ! *psz || *psz == ')')
				return false;
			if (tolower(*psz) != tolower(*pwsz))
				return false;
			++psz;
			++pwsz;
			if (*psz == '*')
				return true;
		}
		return (*psz == 0 || *psz == ')');
	}
	DWORD CounterBytes() const { 
		const PERF_COUNTER_BLOCK* pBlock = (const PERF_COUNTER_BLOCK*)((BYTE*)this + this->ByteLength);
		return pBlock->ByteLength;
	}
	// this returns the base address of the counter data (PERF_COUNTER_BLOCK) for this
	// instance.
	const BYTE * CounterData() const {
		return (const BYTE*)((BYTE*)this + this->ByteLength);
	}
};


class WinPerf_Object : public PERF_OBJECT_TYPE
{
public:
	const WinPerf_Object * Next() const { return (const WinPerf_Object *)((BYTE*)this + this->TotalByteLength); }
	DWORD            NameIndex() const { return this->ObjectNameTitleIndex; }

	int              InstanceCount() const { return this->NumInstances; }

	const WinPerf_ObjInst * FirstInstance() const { 
		if (this->NumInstances <= 0)
			return NULL;
		return (const WinPerf_ObjInst *)((BYTE*)this + this->DefinitionLength);
	}

	// use this only if the object has only a single instance, or never has instances.
	const WinPerf_CounterDef * Counter(int ix) const { 
		if (ix < 0 || ix >= this->NumCounters)
			return NULL;
		const WinPerf_CounterDef * pCounter = (const WinPerf_CounterDef *)((BYTE*)this + this->HeaderLength);
		return pCounter + ix;
	}
	// this returns the base address of the counter data (PERF_COUNTER_BLOCK) if there
	// are no instances, if there are instances, each instance has its own counter block.
	const BYTE * CounterData() const {
		if (this->NumInstances > 0)
			return NULL;
		return (const BYTE*)((BYTE*)this + this->DefinitionLength);
	}

	const WinPerf_CounterDef * Default() const {
		if (this->DefaultCounter >= 0 && this->DefaultCounter < this->NumCounters)
			return this->Counter(this->DefaultCounter);
		return NULL;
	}

	// return the counter with the given id, or the default counter if id == 0
	const WinPerf_CounterDef * Select(const DWORD * pidCounters) const {
		const WinPerf_CounterDef * pCounter = (const WinPerf_CounterDef *)((BYTE*)this + this->HeaderLength);
		while (*pidCounters) {
			for (UINT jj = 0; jj < this->NumCounters; ++jj) {
				if ((pCounter+jj)->NameIndex() == *pidCounters)
					return pCounter + jj;
			}
			++pidCounters;
		}
		if (this->DefaultCounter >= 0 && this->DefaultCounter < this->NumCounters)
			return pCounter + this->DefaultCounter;
		return NULL;
	}
	const


	ULONGLONG GetRawValue(const WinPerf_CounterDef * pCounter, const BYTE * data) const {
		ULARGE_INTEGER uli = {0};
		if (pCounter->CounterSize == 4)
			uli.LowPart = *(DWORD*)(data + pCounter->CounterOffset);
		else if (pCounter->CounterSize >= 8)
			uli.QuadPart = *(UNALIGNED ULONGLONG*)(data + pCounter->CounterOffset);
		return uli.QuadPart;
	}

	char * PrintValue(const WinPerf_CounterDef * pCounter, 
		              const WinPerf_TimerDeltas & time, 
					  const BYTE * curr, 
					  const BYTE * prev) const;

	bool GetValue(WinPerf_CounterValue & value,
		          const WinPerf_CounterDef * pCounter, 
		          const WinPerf_TimerDeltas & time, 
				  const BYTE * curr, 
				  const BYTE * prev) const;
};

class WinPerf_Data : public PERF_DATA_BLOCK
{
public:
	const WinPerf_Object * First() const { 
		if (this->NumObjectTypes > 0)
			return (const WinPerf_Object *)((BYTE*)this + this->HeaderLength); 
		return NULL;
	}
	const WinPerf_Object * Find(DWORD idObject) const {
		const WinPerf_Object * pObj = First();
		for (UINT ii = 0; ii < this->NumObjectTypes; ++ii, pObj = pObj->Next()) {
			if (pObj->ObjectNameTitleIndex == idObject)
				return pObj;
		}
		return NULL;
	}
	int Length() const { 
		return (int)this->NumObjectTypes; 
	}
	const WCHAR * SystemName() const { 
		if (this->SystemNameLength > 0)
			return (const WCHAR *)((BYTE*)this + this->SystemNameOffset); 
		return NULL;
	}
};

bool WinPerf_Object::GetValue(
	WinPerf_CounterValue & ctval,
	const WinPerf_CounterDef * pCounter, 
	const WinPerf_TimerDeltas & time, 
	const BYTE * curr, 
	const BYTE * prev) const
{
	ctval.type = WinPerf_CT_Empty;
	ctval.value.ul = 0;

	const DWORD type = pCounter->CounterType;
	if (type & PERF_DISPLAY_NOSHOW)
		return false;

	ctval.value.ul = GetRawValue(pCounter, curr);
	if (type & PERF_DELTA_COUNTER) 
	{
		if ( !  prev)
			return false;
		ULONGLONG old = GetRawValue(pCounter, prev);
		if (ctval.value.ul >= old)
			ctval.value.ul = ctval.value.ul - old;
		//else dprintf(D_FULLDEBUG, "  PrintValue: negative delta from winreg performance data.\n");
	}

	// number are easy, they can be decimal or hex
	// but there is no scaling.  
	//
	if ((type & PERF_TYPE_ZERO) == PERF_TYPE_NUMBER)
	{
		ctval.type = WinPerf_CT_Unsigned;
		if (type & PERF_NUMBER_DECIMAL)
			ctval.type = ctval.type | WinPerf_CT_Decimal;
		else 
		{
			ctval.type = WinPerf_CT_Hex;
			if ( ! (type & PERF_SIZE_LARGE))
				ctval.type = ctval.type | WinPerf_CT_Short;
		}
		return true;
	}

	// the only other type we support is type counter, if its of type
	// text or type zero, just return false
	//
	if ((type & PERF_TYPE_ZERO) != PERF_TYPE_COUNTER)
		return false;

	// counters have a lot of subtypes, each with different
	// scaling rules.
	//
    if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_VALUE)   // display counter value
	{
		ctval.type = WinPerf_CT_Unsigned;
		if ( ! (type & PERF_SIZE_LARGE))
			ctval.type |= WinPerf_CT_Short;
	}
	else if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_RATE) // divide ctr / delta time
	{
		LONGLONG val = ctval.value.ul;
		ctval.type = WinPerf_CT_Double;
		ctval.value.d = 0.0;

		// determine whether we need to take into account
		// the clock frequency (i.e. scale the delta time 1/seconds)
		// note: there might be a bug here. MS sample code indicates
		// that type PREF_COUNTER_MULTI_TIMER should be scaled by headfreq
		// but that PREF_COUNTER_MULTI_TIMER_INV should not. but both
		// are displayed as %, so freq will be set to 1 here for those
		// counter types.  since there are no instances of these counters
		//
		LONGLONG freq = 1;
		if ((type & PERF_DISPLAY_SECONDS) <= PERF_DISPLAY_PER_SEC) // if /sec or no units
			freq = time.headfreq;

		// special case to scale for percent so we can avoid some
		// truncation error in the scaling.
		LONG pct  = 1;
		if ((type & PERF_DISPLAY_SECONDS) == PERF_DISPLAY_PERCENT)
			pct = 100;

		// determine time scale (freq/delta-time)
		// 
		double   scale = 1.0;
		if (type & PERF_TIMER_100NS) 
		{
			if (time.nanos) scale = (double)freq*pct/time.nanos;
		}
		else if (type & PERF_OBJECT_TIMER) 
		{
			if (time.obj) scale = (double)freq*pct/time.obj;
		}
		else
		{
			if (time.head) scale = (double)freq*pct/time.head;
		}

		// apply the rate and scale to the value
		//
		double value = 0.0;
		if (type & PERF_MULTI_COUNTER)
		{
			DWORD multi = ((DWORD*)(curr + pCounter->CounterOffset))[2];
			if (type & PERF_INVERSE_COUNTER)
				ctval.value.d = (double)multi * pct - val * scale;
			else if (multi)
				ctval.value.d = (val * scale) / multi;
		}
		else if (type & PERF_INVERSE_COUNTER)
		{
			ctval.value.d = pct - (val * scale);
		}
		else
		{
			ctval.value.d = val * scale;
		}
	}
	else if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_ELAPSED) // subtract ctr from abs obj time
	{
		ctval.value.d = (double)(time.objabs - ctval.value.ul);
		if (time.objfreq)
			ctval.value.d /= time.objfreq;
		ctval.type = WinPerf_CT_Double;
	}
	else if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_FRACTION)
	{
		ULONGLONG val = ctval.value.ul;
		// special case to scale for percent so we can avoid some
		// truncation error in the scaling.
		LONG pct  = 1;
		if ((type & PERF_DISPLAY_SECONDS) == PERF_DISPLAY_PERCENT)
			pct = 100;

		const WinPerf_CounterDef * pBase = pCounter+1;
		double scale = 1.0;
		if ((pBase->CounterType & PERF_COUNTER_PRECISION) == PERF_COUNTER_BASE)
		{
			ULONGLONG base = GetRawValue(pBase, curr);
			if (type & PERF_DELTA_COUNTER)
			{
				ULONGLONG baseOld = GetRawValue(pBase, prev);
				base -= baseOld;
			}
			if (base)
				scale = pct/base;
		}

		// TJ: This looks like a special case to me. PERF_AVERAGE_TIMER
		// does NOT have PERF_DELTA_COUNTER, but according to the example
		// code it should be calculated as ((N1-N0)/TB) / (B1 - B0)
		if (PERF_AVERAGE_TIMER == type)
		{
			ctval.type = WinPerf_CT_Double;
			ctval.value.d = val / time.headfreq * scale;
		}
		else
		{
			ctval.type = WinPerf_CT_Double;
			ctval.value.d = val * scale;
		}
	}
	else if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_QUEUELEN)
	{
		EXCEPT( "WinReg perf counter type QUEUELEN is unimplemented." );
	}

	if (ctval.type != WinPerf_CT_Empty)
	{
		switch (type & PERF_DISPLAY_SECONDS)
		{
		case PERF_DISPLAY_PERCENT: ctval.type |= WinPerf_CT_Percent; break;
		case PERF_DISPLAY_PER_SEC: ctval.type |= WinPerf_CT_PerSecond; break;
		case PERF_DISPLAY_SECONDS: ctval.type |= WinPerf_CT_Seconds; break;
		}
	}

	return true;
}

char * WinPerf_Object::PrintValue(
	const WinPerf_CounterDef * pCounter, 
	const WinPerf_TimerDeltas & time, 
	const BYTE * curr, 
	const BYTE * prev) const 
{
	ULONGLONG val = GetRawValue(pCounter, curr);
	char sz[64];
	sz[0] = 0;

	const DWORD type = pCounter->CounterType;
	if (type & PERF_DELTA_COUNTER) 
	{
		if ( !  prev)
			return NULL;
		ULONGLONG old = GetRawValue(pCounter, prev);
		if (val >= old)
			val = val - old;
		else
			dprintf(D_FULLDEBUG, "  PrintValue: negative delta from winreg performance data.\n");
		//dprintf(D_FULLDEBUG, "  PrintValue: delta %I64d - %I64d = %I64d,  %I64d/%I64d %I64d/%I64d %I64d %I64d\n", 
		//	val, old, val - old, time.head, time.headfreq, time.obj, time.objfreq, time.nanos, time.objabs);
	}

	if ((type & PERF_TYPE_ZERO) == PERF_TYPE_NUMBER || (type & PERF_COUNTER_PRECISION) == PERF_COUNTER_VALUE)
	{
		if (type & PERF_NUMBER_DECIMAL)
			sprintf(sz, "%I64d     ", val);
		else if (type & PERF_SIZE_LARGE)
			sprintf(sz, "0x%I64X     ", val);
		else
			sprintf(sz, "0x%X     ", *(DWORD*)&val);
	}
	else if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_RATE) // divide ctr / delta time
	{
		double scale = 1;
		double value = 0.0;
		const char * pszUnits = "    ";
		LONGLONG mult = 1;
		if ((type & PERF_DISPLAY_SECONDS) == PERF_DISPLAY_PERCENT)
		{
			mult = 100;
			pszUnits = " %   ";
		}
		else if ((type & PERF_DISPLAY_SECONDS) == PERF_DISPLAY_PER_SEC) // if /sec or no units
		{
			mult = time.headfreq;
			pszUnits = " /sec";
		}
		else if ((type & PERF_DISPLAY_SECONDS) == PERF_DISPLAY_NO_SUFFIX) // if /sec or no units
		{
			mult = time.headfreq;
		}
		else if ((type & PERF_DISPLAY_SECONDS) == PERF_DISPLAY_SECONDS)
		{
			pszUnits = " sec ";
		}

		if (type & PERF_TIMER_100NS) 
		{
			if (time.nanos) scale = (double)mult/time.nanos;
		}
		else if (type & PERF_OBJECT_TIMER) 
		{
			if (time.obj) scale = (double)mult/time.obj;
		}
		else
		{
			if (time.head) scale = (double)mult/time.head;
		}

		if (type & PERF_MULTI_COUNTER)
		{
			DWORD multi = ((DWORD*)(curr + pCounter->CounterOffset))[2];
			if (type & PERF_INVERSE_COUNTER)
				value = 100.0 * multi - val * scale;
			else if (multi)
				value = (val * scale) / multi;
		}
		else if (type & PERF_INVERSE_COUNTER)
		{
			if ((type & PERF_DISPLAY_SECONDS) == PERF_DISPLAY_PERCENT)
				value = 100.0 - (val * scale);
			else
				value = 1.0 - (val * scale);
		}
		else
		{
			//dprintf (D_FULLDEBUG, "    scaling %I64d by %f\n", val, scale);
			value = (double)val * scale;
		}
		sprintf(sz, "%f%s", value, pszUnits);
	}
	else if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_ELAPSED) // subtract ctr from abs obj time
	{
		double value = (double)(time.objabs - val);
		if (time.objfreq)
			value /= time.objfreq;
		sprintf(sz, "%f sec ", value);
	}
	else if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_FRACTION)
	{
		const WinPerf_CounterDef * pBase = pCounter+1;
		ULONGLONG baseCurr = 0;
		if ((pBase->CounterType & PERF_COUNTER_PRECISION) == PERF_COUNTER_BASE)
		{
			baseCurr = GetRawValue(pBase, curr);
		}
		double scale = 1.0;
		if (baseCurr)
			scale = 100.0/baseCurr;
		double value = (double)val * scale;
		sprintf(sz, "%f     ", value);
	}
	else if ((type & PERF_COUNTER_PRECISION) == PERF_COUNTER_QUEUELEN)
	{
		EXCEPT( "WinReg perf counter type QUEUELEN is unimplemented." );
	}
	if (sz[0])
		return strdup(sz);
	return NULL;
}


bool init_WinPerf_Query(
	const char * pszRegKey, 
	const char * pszValueName, 
	WinPerf_Query & query)
{
	ZeroMemory(&query, sizeof(query));

	char * pszTemp = NULL; // in case we need to copy the keyname

	if (( ! pszValueName || ! pszValueName[0]) && strchr(pszRegKey, '\\'))
	{
		pszTemp = strdup(pszRegKey); // this gets free'd as we leave the function.
		char * pszCounter = strchr(pszTemp, '\\');
		if (pszCounter)
		{
           *pszCounter++ = 0;
			pszRegKey = pszTemp;
			pszValueName = pszCounter;
		}
		dprintf ( D_FULLDEBUG, "cracking key as key=\"%s\", counter=\"%s\"\n",
			     pszRegKey, pszValueName);
	}
	if (strchr(pszRegKey, '('))
	{
		char * pszInst = NULL;
		if ( ! pszTemp)
			pszTemp = strdup(pszRegKey); // this gets free'd as we leave the function.
		char * pszInstance = strchr(pszTemp, '(');
		if (pszInstance)
		{
			*pszInstance++ = 0;
			pszInst = pszInstance;
			pszInstance = strchr(pszInstance, ')');
			if (pszInstance)
			{
				*pszInstance = 0;
				query.ixInst = (int)(pszInst - pszTemp);
				query.cchInst = (int)(pszInstance - pszInst);
				query.idInst = atoi(pszInst);
			}
			pszRegKey = pszTemp;
		}
		dprintf ( D_FULLDEBUG, "cracking key as key=\"%s\", inst=\"%s\"\n",
			     pszRegKey, pszInst);
	}

	// if the regkey is a number, use it, otherwise, use the performance_hashtable
	// to get the index from the given name.
	//
	if (isdigit(pszRegKey[0]))
		query.idKey = atoi(pszRegKey);
	else
	{
		init_windows_performance_hashtable();  // this is quick if we already init'd
		std::map<YourStringNoCase, std::vector<const char *>>::iterator it;
		if (rl.pPerfTable &&
			(it = rl.pPerfTable->find(pszRegKey)) != rl.pPerfTable->end())
		{
			query.idKey = atoi(it->second.back());
		}
	}

	if (pszValueName && pszValueName[0])
	{
		if (isdigit(pszValueName[0]))
			query.idCounter = atoi(pszValueName);
		else
		{
			init_windows_performance_hashtable();  // this is quick if we already init'd
			const char * pszCounterIndex = NULL;
			std::map<YourStringNoCase, std::vector<const char *>>::iterator it;
			if (rl.pPerfTable &&
				(it = rl.pPerfTable->find(pszValueName)) != rl.pPerfTable->end())
			{
				//if (rl.pPerfTable->lookup(pszValueName, pszCounterIndex) >= 0)
				//	query.idCounter = atoi(pszCounterIndex);
				int ix = 0;
				for (size_t i = 0; i < it->second.size(); i++)
				{
					((DWORD*)&query.idCounter)[ix++] = atoi(it->second[i]);
				}
				if (query.idAlt[NUM_ELEMENTS(query.idAlt)-1] != 0)
				{
					dprintf (D_ALWAYS, "Error: too many counter ids map to the same name!\n");
					query.idAlt[NUM_ELEMENTS(query.idAlt)-1] = 0;
				}
			}
		}
	}

	if (pszTemp)
		free(pszTemp);

	return (query.idKey != 0);
}

int WinPerf_CounterValue::Print(char * psz, int cchMax, bool fIncludeUnits) const
{
	int cch = 0;
	psz[0] = 0;
	if ((this->type & WinPerf_CT_Double) == WinPerf_CT_Double)
	{
		cch = sprintf(psz, "%f", value.d);
	}
	else
	{
		const char * pszFmt = "";
		switch (this->type & ~WinPerf_CT_UnitsMask)
		{
		case WinPerf_CT_Unsigned | WinPerf_CT_Hex:     pszFmt = "%I64X"; break;
		case WinPerf_CT_Signed   | WinPerf_CT_Decimal: pszFmt = "%I64d"; break;
		case WinPerf_CT_Unsigned | WinPerf_CT_Decimal: pszFmt = "%I64u"; break;

		case WinPerf_CT_Unsigned | WinPerf_CT_Hex | WinPerf_CT_Short:     pszFmt = "%X"; break;
		case WinPerf_CT_Signed   | WinPerf_CT_Decimal | WinPerf_CT_Short: pszFmt = "%d"; break;
		case WinPerf_CT_Unsigned | WinPerf_CT_Decimal | WinPerf_CT_Short: pszFmt = "%u"; break;
		}
		cch = sprintf(psz, pszFmt, value.ul);
	}
	
	if (fIncludeUnits)
	{
		const char * pszUnits = NULL;
		switch (this->type & WinPerf_CT_UnitsMask)
		{
		case WinPerf_CT_Seconds:   pszUnits = " sec";  break;
		case WinPerf_CT_PerSecond: pszUnits = "/sec"; break;
		case WinPerf_CT_Percent:   pszUnits = " %";	  break;
		}
		if (pszUnits)
		{
			//psz[cch++] = ' ';
			strcpy(&psz[cch], pszUnits);
			cch += (int)strlen(pszUnits);
		}
	}

	return cch;
}

static int free_query_data(WinPerf_QueryResult result) {
	if (result.pdata1)
		free((void*)result.pdata1);
	if (result.pdata2)
		free((void*)result.pdata2);
	return true;
}

void release_all_WinPerf_results()
{
    if (rl.pQueries)
    {
        rl.pQueries->walk(free_query_data);
        free (rl.pQueries);
        rl.pQueries = NULL;
    }
}

bool update_windows_performance_result(WinPerf_QueryResult & result)
{
	char szKeyID[12];
	wsprintf(szKeyID, "%d", result.idObject);
	DWORD cbData = result.cbAlloc;
	DWORD vtype = REG_SZ; 

	// the new data set becomes the old data set, and the (formerly) old
	// data set will be overwritten. 
	byte * pdata = (byte*)result.pdata1;
	result.pdata1 = result.pdata2;
	result.pdata2 = (const WinPerf_Data *)pdata;

	// query for the data, if it fits, we get back success, note that it's
	// safe to pass NULL for pdata here. 
	LONG lres = RegQueryValueEx(HKEY_PERFORMANCE_DATA, szKeyID, NULL, &vtype, pdata, &cbData);
	if (ERROR_SUCCESS == lres)
	{
		return true;
	}

	if (ERROR_SUCCESS != lres && ERROR_MORE_DATA != lres)
	{
		if (ERROR_FILE_NOT_FOUND == lres)
		{
			// the key did not exist.
			dprintf( D_FULLDEBUG, 
 				"The Performance Key \"HKPD\\%s\" does not exist\n",
					szKeyID);
		}
		else
		{
			// there was some other error, probably an access violation.
			char szMsg[MAX_PATH];
			dprintf( D_ALWAYS, 
				"Failed to read Performance Key \"HKPD\\%s\"\nReason: %s\n",
					szKeyID, 
					GetLastErrMessage(GetLastError(), szMsg, NUM_ELEMENTS(szMsg)));
		}
		return false;
	}
	
	// save off the old data set, we may need to copy it into a new buffer.
	const byte * pdataOld = (const byte *)result.pdata1;
	result.pdata1 = NULL;
	result.pdata2 = NULL;

	// keep querying with increasingly larger allocations until
	// we get back an answer other than ERROR_MORE_DATA.
	DWORD  cbAlloc = result.cbAlloc;
	do 
	{
		if (cbAlloc < 0x10000) cbAlloc *= 2;
        cbAlloc += (cbAlloc > 0x200) ? cbAlloc : 0x200;

		if (pdata) 
			free(pdata);
		pdata = (byte*)malloc(cbAlloc);
		if ( ! pdata)
			break;

		//dprintf( D_FULLDEBUG, "Query %s with %d byte buffer\n", szKeyID, cbData);

		cbData = cbAlloc;
		lres = RegQueryValueEx(HKEY_PERFORMANCE_DATA, szKeyID, NULL, &vtype, pdata, &cbData);

	} while (lres == ERROR_MORE_DATA);

	if (lres != ERROR_SUCCESS)
	{
		if (pdataOld) free((void*)pdataOld);
		if (pdata) free(pdata);
		return false;
	}

	// if we have old data, we need to copy it into a buffer of size cbAlloc
	//
	if (pdataOld)
	{
		byte * pdataT = (byte*)malloc(cbAlloc);
		if (pdataT)
		{
			CopyMemory(pdataT, pdataOld, result.cbAlloc);
			free ((void*)pdataOld);
			pdataOld = pdataT;
		}
	}

	result.cbAlloc = cbAlloc;
	result.pdata1 = (const WinPerf_Data *)pdataOld;
	result.pdata2 = (const WinPerf_Data *)pdata;
	return true;
}

void update_all_WinPerf_results()
{
    if (rl.pQueries)
    {
        rl.pQueries->startIterations();
        WinPerf_QueryResult result;
        while (rl.pQueries->iterate(result)) 
        {
            update_windows_performance_result(result);
            rl.pQueries->insert(result.idObject, result, true);
        }
    }
}

static bool update_windows_performance_value(
	const WinPerf_QueryResult & result, 
	const WinPerf_Query & query, 
	AttribValue *pav)
{
	if ( ! pav)
		return false;

	const WinPerf_Data * pHead = result.pdata2;
	const WinPerf_Object * pObj = pHead->Find(query.idKey);
	if ( ! pObj)
		return false;

	const WinPerf_CounterDef * pCounter = pObj->Select(&query.idCounter);
	if ( ! pCounter)
		return false;

	const char *pszInst = NULL;
	if (query.cchInst)
	{
		if ( ! query.ixInst)
			pszInst = (const char*)((&query)+1);
		else
		{
			// error, can't use a relative pszInst without
			// the pszKey that it references.
		}
	}

	const BYTE * pbData = pObj->CounterData();

	// figure out which set of counter data to look at.
	// use the object data if no instances, use the matching
	// instance if there is a pszMatch, use the last instance
	// if no match. _Total is usually the last instance.
	// we start iterating at 1 because we want to compare only
	// the first n-1 instances, the last instance will always match
	const WinPerf_ObjInst * pInst = pObj->FirstInstance();
	const WCHAR * pszDataName = NULL;
	for (int ix = 1; ix < (int)pObj->NumInstances; ++ix, pInst = pInst->Next())
	{
		if (pszInst && pInst->Matches(pszInst))
		{
			pszDataName = pInst->Name();
			break;
		}
	}
	if (pInst)
		pbData = pInst->CounterData();

	if ( ! pbData)
		return false;

	WinPerf_CounterValue ctval;
	WinPerf_TimerDeltas time;
	time.obj      = pObj->PerfTime.QuadPart;
	time.objabs   = pObj->PerfTime.QuadPart;
	time.objfreq  = pObj->PerfFreq.QuadPart;
	time.head     = pHead->PerfTime.QuadPart;
	time.headfreq = pHead->PerfFreq.QuadPart;
	time.nanos    = pHead->PerfTime100nSec.QuadPart;

	const BYTE * pbDataPrev = NULL;
	if (result.pdata1 && (pCounter->CounterType & PERF_DELTA_COUNTER))
	{
		const WinPerf_Object  * pObj1 = result.pdata1->Find(query.idKey);
		const WinPerf_ObjInst * pInst1 = pObj1->FirstInstance();
		pbDataPrev = pObj1->CounterData();
		for (int ix = 1; ix < (int)pObj1->NumInstances; ++ix, pInst1 = pInst1->Next())
		{
			if (pszInst && pInst1->Matches(pszInst)
				&& pInst->Name() && pszDataName
				&& (0 == lstrcmpW(pInst->Name(), pszDataName)))
				break;
		}
		if (pInst1)
			pbDataPrev = pInst1->CounterData();

		time.obj      -= pObj1->PerfTime.QuadPart;
		time.head     -= result.pdata1->PerfTime.QuadPart;
		time.nanos    -= result.pdata1->PerfTime100nSec.QuadPart;
	}

	if ( ! pObj->GetValue(ctval, pCounter, time, pbData, pbDataPrev))
		return false;

	pav->value.ll = ctval.value.ll;
	pav->vtype = AttribValue_DataType_Int;
	if ((ctval.type & WinPerf_CT_Double) == WinPerf_CT_Double)
		pav->vtype = AttribValue_DataType_Double;
																												 
	// if the value is a 64bit integer, and it won't fit in a 32 bit integer
	// convert it to a double.
	//
	if ((ctval.type & (WinPerf_CT_Double | WinPerf_CT_Short)) == WinPerf_CT_Signed)
	{
		if (ctval.value.ll > 0x7FFFFFFF || ctval.value.ll < 0x8000000)
		{
			pav->value.d = (double)ctval.value.ll;
			pav->vtype = AttribValue_DataType_Double;
		}
	}
	else if ((ctval.type & (WinPerf_CT_Double | WinPerf_CT_Short)) == WinPerf_CT_Unsigned)
	{
		if (ctval.value.ul > 0x7FFFFFFF)
		{
			pav->value.d = (double)ctval.value.ul;
			pav->vtype = AttribValue_DataType_Double;
		}
	}

	return true;
}

bool update_WinPerf_Value(AttribValue *pav)
{
    if ( ! pav)
        return false;

    WinPerf_Query * pquery = (WinPerf_Query *)pav->pquery;
    if (pquery && rl.pQueries)
    {
        WinPerf_QueryResult result;
        if (rl.pQueries->lookup(pquery->idKey, result) >= 0)
            return update_windows_performance_value(result, *pquery, pav);
    }

    return false;
}


AttribValue * add_WinPerf_Query(
    const char * pszAttr,
    const char * pkey)
{
	WinPerf_Query query;
	if ( ! init_WinPerf_Query(pkey, NULL, query)) 
       return NULL;

    // allocate an initialize an AttribValue, this consists
    // of a header (AttribValue) the attrib string, the query
    // and the query instance data all packed together into a single
    // allocation, with the AttribValue at the start. 
    //
	int cbQuery  = sizeof(query) + query.cchInst+1;
	int cchAttr  = (int)strlen(pszAttr) + 1;
	int cb = sizeof(AttribValue) + cbQuery + cchAttr;
	AttribValue * pav = (AttribValue*)malloc(cb);
	if (pav) 
    {
		pav->cb = cb;
		strcpy((char*)(pav+1), pszAttr);
		pav->pszAttr = (char*)(pav+1);

		pav->vtype = AttribValue_DataType_Double;
		pav->value.d = 0.0;
		WinPerf_Query * pquery = (WinPerf_Query *)(pav->pszAttr + cchAttr);
		*pquery = query;
		pquery->ixInst = 0;
		if (pquery->cchInst) 
        {
			strncpy((char*)(pquery+1), pkey + query.ixInst, query.cchInst);
			((char*)(pquery+1))[query.cchInst] = 0;
		}
		pav->pquery = (void*)pquery;

		// add this this key to the list of queries
		WinPerf_QueryResult result = {query.idKey, 0, NULL, NULL};
		rl.pQueries->insert(query.idKey, result, true);
        /*
        bool found = false;
        lst_WinPerf.Rewind();
        while (WinPerf_QueryResult * pres = lst_WinPerf.Next()) 
        {
            if (pres && (pres->idObject == query.idKey)) 
            {
                found = true;
                break;
            }
        }

        if ( ! found) 
        {
            WinPerf_QueryResult * pres = (WinPerf_QueryResult *)malloc (sizeof(*pres));
            ASSERT(pres);
            ZeroMemory(pres, sizeof(*pres));
            pres->idObject = query.idKey;
	        lst_WinPerf.InsertHead(pres);
        }
        */
	}

    return pav;
}
