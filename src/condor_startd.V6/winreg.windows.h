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
    This file defines classes and structures use to gather values
    from the Windows registry and put them in a form that is convenient
    to publish in a ClassAd

   	Written 2010 by John M. Knoeller <johnkn@cs.wisc.edu>
*/

#ifndef _WINREG_CONDOR_H
#define _WINREG_CONDOR_H
#ifndef WIN32
#pragma error This module is Win32 specific
#endif


/*

	//
	int          qtype;    // the query type, used to decode the query union
	union {
		struct {
			HKEY    hive;
			const char * pszKey;
		} reg;
		struct {
			DWORD   idKey;    // identifies counter group (WinPerf_Object)
			DWORD   idCounter;// identifies counter (WinPerf_CounterDef)
			DWORD   idAlt[6]; // alternate counter ids because there are multiple ids that match a single string.
			DWORD   idInst;   // instance unique id (if available)
			int     ixInstName;  // offset to string of the instance name
			int     cchInstName; // size of the instance name in characters
		} perf;
	} query;

	bool IsRegQuery() const { return (this->qtype == 0); }
	bool IsPerfQuery() const { return (this->qtype == 1); }

	const char * RegKeyName() const {
		if (IsRegQuery() && query.reg.ixKeyName >= sizeof(this) && query.reg.ixKeyName < cb)
			return (const char *)this + query.reg.ixKeyName;
		return NULL;
	}

	const char * RegValueName() const {
		if (IsRegQuery() && query.reg.ixValueName >= sizeof(this) && query.reg.ixValueName < cb)
			return (const char *)this + query.reg.ixValueName;
		return NULL;
	}

	const char * PerfInstance() const {
		if (IsPerfQuery() && query.perf.ixInstName >= sizeof(this) && query.perf.ixInstName < cb)
			return (const char *)this + query.perf.ixInstName;
		return NULL;
	}
*/

char * generate_reg_key_attr_name(
    const char * pszPrefix, 
    const char * pszKeyName);

HKEY parse_hive_prefix(
    const char * psz, 
    int cch, 
    int * pixPrefixSep);

char * get_windows_reg_value(
	const char * pszRegKey, 
	const char * pszValueName, 
	int          options = 0);

void release_all_WinPerf_results();
bool update_WinPerf_Value(struct _AttribValue *pav);
void update_all_WinPerf_results();

struct _AttribValue * add_WinPerf_Query(
    const char * pszAttr,
    const char * pkey);

/*
bool init_WinPerf_Query(
	const char * pszRegKey, 
	const char * pszValueName, 
	WinPerf_Query & query);
*/

#endif /* _WINREG_CONDOR_H */
