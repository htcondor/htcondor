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

#include <Winsock2.h>
#include <windows.h>

#include   <msi.h>
#include   <msiquery.h>
#include   <stdio.h>

/* This is a utility DLL for the Condor installer. It's basically a
 * collection of handy functions we call up from the MSI installer as
 * Custom Actions. If you add a function that you want to call from a 
 * Custom Action, be sure to add it to the .def file so its exported.
 */

/* These are utility functions for getting and setting installer properties */
char *get_string_property(MSIHANDLE hInstall, const char* prop);
int get_integer_property(MSIHANDLE hInstall, const char* prop, int default_val);

BOOL set_string_property(MSIHANDLE hInstall, const char *prop, const char *val);
BOOL set_integer_property(MSIHANDLE hInstall, const char *prop, const int val);


BOOL get_hostname(char *buf, int sz);

BOOL 
set_string_property(MSIHANDLE hInstall, const char *prop, const char *val) {

	UINT uiStat;
	
	uiStat =  MsiSetProperty(hInstall, prop, val);

	return (uiStat == ERROR_SUCCESS);
}

BOOL
set_integer_property(MSIHANDLE hInstall, const char *prop, const int val) {
	
	char buf[1024];

	sprintf(buf, "%d", val);

	return set_string_property(hInstall, prop, buf);
}

int
get_integer_property(MSIHANDLE hInstall, const char* prop, int default_val) {
	
	char *ptr;
	int val;

	ptr = get_string_property(hInstall, prop);

	if ( ptr != NULL ) {
		val = atoi(ptr);
		free(ptr);

	} else {
		val = default_val;
	}

	return val;
}

char *
get_string_property(MSIHANDLE hInstall, const char* prop) {
	
	TCHAR* szValueBuf = NULL;
    DWORD cchValueBuf = 0;
    
	UINT uiStat;
	
	uiStat =  MsiGetProperty(hInstall, prop, TEXT(""), &cchValueBuf);
    
	if (ERROR_MORE_DATA == uiStat)
    {
        ++cchValueBuf; // on output does not include terminating null, so add 1
        szValueBuf = malloc(sizeof(TCHAR)*cchValueBuf);
        if (szValueBuf)
        {
            uiStat = MsiGetProperty(hInstall, prop, szValueBuf, &cchValueBuf);
        }
    }
    
	if (ERROR_SUCCESS != uiStat)
    {
        return NULL;
    }


    return szValueBuf;
}


BOOL __stdcall set_daemon_list(MSIHANDLE hInstall) {

	int offset, bufsiz = 1024;
	char buf[1024];


	/* always run the master */
	offset = _snprintf(buf, bufsiz, "MASTER");

	if ( get_integer_property(hInstall, "NEW_POOL", 0) > 0 ) {

		offset += _snprintf(buf+offset, (bufsiz-offset), " COLLECTOR");
		offset += _snprintf(buf+offset, (bufsiz-offset), " NEGOTIATOR");
	}

	if ( get_integer_property(hInstall, "SUBMIT_JOBS", 0) > 0 ) {
		offset += _snprintf(buf+offset, (bufsiz-offset), " SCHEDD");
	}

	if ( get_integer_property(hInstall, "RUN_JOBS", 0) > 0 ) {
		offset += _snprintf(buf+offset, (bufsiz-offset), " STARTD");
	}

	return set_string_property(hInstall, TEXT("DAEMON_LIST"), buf);
}


#define MAXHOSTNAMELEN 1024
UINT __stdcall 
get_full_hostname(MSIHANDLE hInstall) {
	
	char buf[MAXHOSTNAMELEN];

	if ( get_hostname(buf, MAXHOSTNAMELEN) ) {
		
		return set_string_property(hInstall, TEXT("CONDOR_HOST"), buf);
	} else {
		return FALSE;
	}
}


	/* tell us the DNS domain for this machine. */
BOOL __stdcall 
get_domain(MSIHANDLE hInstall) {

	char buf[MAXHOSTNAMELEN];

	if ( get_hostname(buf, MAXHOSTNAMELEN) ) {
		char* ptr;
		
		ptr = strchr(buf, '.');
		
		if ( ptr ) {
			ptr++;

			return set_string_property(hInstall, TEXT("UID_DOMAIN"), ptr);
		} 
	}
	
	return set_string_property(hInstall, TEXT("UID_DOMAIN"), "your.domain.here");
	
}

	/* opens up a file and appends the given string */
UINT __stdcall 
append_to_file( LPTSTR file, LPTSTR str ) {
	return 0;

}

	/* takes a string. If it ends with '\', we snip it off */
UINT __stdcall 
chomp_backslash ( MSIHANDLE hInstall, LPTSTR property ) {
	int a, rv;
	char *input;

	rv = 0;
	input = get_string_property(hInstall, property);
	
	if ( input != NULL ) {
		a = strlen(input);

		if ( a > 0 ) {
			if ( input[a-1] == '\\' ) {
				input[a-1] = '\0';
			}
		}

		rv = set_string_property(hInstall, property, input);
		free(input);
	}

	return rv;
}


BOOL 
get_hostname(char *buf, int sz) {

	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	char hostbuf[MAXHOSTNAMELEN];

	
	wVersionRequested = MAKEWORD( 2, 0 );
 
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		return FALSE;
	}
	
	if ( gethostname(hostbuf, sizeof(hostbuf)) == 0 ) {
		HOSTENT *a;

		a = gethostbyname(hostbuf);
		
		if ( a ) {
			return _snprintf(buf, sz, "%s", a->h_name);
		} else {
			return FALSE;
		}
	}

	return FALSE;
}

BOOL APIENTRY DllMain( HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved )
{
    return TRUE;
}

UINT __stdcall SampleFunction2 ( MSIHANDLE hModule )
{
    MessageBox(NULL, "Hello world", "CodeProject.com", MB_OK);
    return ERROR_SUCCESS;
}