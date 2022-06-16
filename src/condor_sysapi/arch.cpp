/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

// char  sysname[]  name of this implementation of the operating system
// char  nodename[] name of this node within an implementation-dependent
//                 communications network
// char  release[]  current release level of this implementation
// char  version[]  current version level of this release
// char  machine[]  name of the hardware type on which the system is running

#include "condor_common.h"
#include "condor_debug.h"
#include "match_prefix.h"
#include "sysapi.h"
#include "sysapi_externs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(Darwin)
#include <sys/sysctl.h>
#include "my_popen.h"
#endif

/* ok, here's the deal with these. For some reason, gcc on IRIX is just old
	enough to where it doesn't understand things like static const FALSE = 0.
	So these two things used to be defined as the above, but it would fail with
	a 'initializer element is not constant' during compile. I have defined
	them as to make the preprocessor do the work instead of a const int.
	-psilord 06/01/99 */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#if defined(WIN32)

// Names of all the architectures supported for Windows (Only some of
// these are relevant to us; however, listing them all simplifies lookup).
static char const *windows_architectures[] = {
	"INTEL",
	"MIPS",
	"ALPHA",
	"PPC",
	"SHX",
	"ARM",
	"IA64",
	"ALPHA64",
	"MSIL",
	"X86_64", // was AMD64, but it means both
	"IA32_ON_WIN64"
};

// On the off chance that we simply don't recognize the architecture we
// can tell the user as much:
static char const *unknown_architecture = "unknown";

#if _WIN32_WINNT < 0x0501
// hoisted from winbase.h, we won't need to do this, once we formally
// set the WINAPI target to 501
VOID WINAPI GetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo);
#endif

const char *
sysapi_condor_arch(void)
{
	SYSTEM_INFO info;
	GetNativeSystemInfo(&info);
	if (   info.wProcessorArchitecture >= PROCESSOR_ARCHITECTURE_INTEL
		&& info.wProcessorArchitecture <= PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 ) {
		return windows_architectures[info.wProcessorArchitecture];
	} else {
		return unknown_architecture;
	}
}

const char *
sysapi_uname_arch(void)
{
	return sysapi_condor_arch();
}

const char *
sysapi_uname_opsys(void)
{
	return sysapi_opsys();
}

static int windows_inited = FALSE;
static const char* opsys = NULL;
static const char* opsys_versioned = NULL;
static int opsys_version = 0;
static const char* opsys_name = NULL;
static const char* opsys_long_name = NULL;
static const char* opsys_short_name = NULL;
static int opsys_major_version = 0;
static const char* opsys_legacy = NULL;
static const char* opsys_super_short_name = NULL;


// Find/create all opsys params in this method using OSVERSIONINFO
void
sysapi_get_windows_info(void)
{
	char tmp_info[7+10+10] = "UNKNOWN";

	OSVERSIONINFOEX info;
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	MSC_SUPPRESS_WARNING_FOREVER(4996) // 'GetVersionExA': was declared deprecated
	if (GetVersionEx((LPOSVERSIONINFO)&info) > 0 ) {
		switch(info.dwPlatformId) {
		case VER_PLATFORM_WIN32s:
			sprintf(tmp_info, "WIN32s%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			sprintf(tmp_info, "WIN32%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_NT:
			sprintf(tmp_info, "WINNT%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		}
	}
	opsys_legacy = strdup( tmp_info );
	opsys = strdup( "WINDOWS" );
	
	if (info.dwMajorVersion == 10) {
		opsys_super_short_name = strdup("10");
	} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 2) {
		opsys_super_short_name = strdup("8");
	} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 1) {
		opsys_super_short_name = strdup("7");
	} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 0) {
		opsys_super_short_name = strdup("Vista");
	} else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 2) {
		opsys_super_short_name = strdup("Server2003");
	} else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 1) {
		opsys_super_short_name = strdup("XP");
	} else {
		opsys_super_short_name = strdup("Unknown");
	}

        sprintf(tmp_info, "Win%s",  opsys_super_short_name );
	opsys_short_name = strdup( tmp_info );

        opsys_version = info.dwMajorVersion * 100 + info.dwMinorVersion;
    	opsys_major_version = opsys_version;

        sprintf(tmp_info, "Windows%s",  opsys_super_short_name );
	opsys_name = strdup( tmp_info );

        sprintf(tmp_info, "Windows %s SP%d",  opsys_super_short_name, info.wServicePackMajor);
	opsys_long_name = strdup( tmp_info );

        sprintf(tmp_info, "%s%d",  opsys, opsys_version);
	opsys_versioned = strdup( tmp_info );

        if (!opsys) {
                opsys = strdup("Unknown");
        }
        if (!opsys_name) {
                opsys_name = strdup("Unknown");
        }
        if (!opsys_short_name) {
                opsys_short_name = strdup("Unknown");
        }
        if (!opsys_long_name) {
                opsys_long_name = strdup("Unknown");
        }
        if (!opsys_versioned) {
                opsys_versioned = strdup("Unknown");
        }
        if (!opsys_legacy) {
                opsys_legacy = strdup("Unknown");
        }

	if ( opsys ) {
		windows_inited = TRUE;
	}
}

const char *
sysapi_opsys(void)
{
        if( ! windows_inited ) {
                sysapi_get_windows_info();
        }
        return opsys;
}

const char *
sysapi_opsys_versioned(void)
{
        if( ! windows_inited ) {
                sysapi_get_windows_info();
        }
        return opsys_versioned;
}

int
sysapi_opsys_version(void)
{
        if( ! windows_inited ) {
                sysapi_get_windows_info();
        }
        return opsys_version;
}

const char *
sysapi_opsys_long_name(void)
{
        if( ! windows_inited ) {
                sysapi_get_windows_info();
        }
        return opsys_long_name;
}


const char *
sysapi_opsys_short_name(void)
{
	if( ! windows_inited ) {
                sysapi_get_windows_info();
	}
	return opsys_short_name;
}

int
sysapi_opsys_major_version(void)
{
        if( ! windows_inited ) {
                sysapi_get_windows_info();
        }
        return opsys_major_version;
}

const char *
sysapi_opsys_name(void)
{
        if( ! windows_inited ) {
                sysapi_get_windows_info();
        }
        return opsys_name;
}

const char *
sysapi_opsys_legacy(void)
{
        if( ! windows_inited ) {
                sysapi_get_windows_info();
        }
        return opsys_legacy;
}

void sysapi_opsys_dump(int category)
{
	// Print out param values to the logfiles for debugging
	dprintf(category, "OpSysMajorVer:  %d \n", opsys_major_version);
	dprintf(category, "OpSysShortName:  %s \n", opsys_short_name);
	dprintf(category, "OpSysLongName:  %s \n", opsys_long_name);
	dprintf(category, "OpSysAndVer:  %s \n", opsys_versioned);
	dprintf(category, "OpSysLegacy:  %s \n", opsys_legacy);
	dprintf(category, "OpSysName:  %s \n", opsys_name);
	dprintf(category, "OpSysVer:  %d \n", opsys_version);
	dprintf(category, "OpSys:  %s \n", opsys);
}

#else

static int arch_inited = FALSE;
static int utsname_inited = FALSE;
static const char* arch = NULL;
static const char* uname_arch = NULL;
static const char* uname_opsys = NULL;

static const char* opsys = NULL;
static const char* opsys_versioned = NULL;
static int opsys_version = 0;
static const char* opsys_name = NULL;
static const char* opsys_long_name = NULL;
static const char* opsys_short_name = NULL;
static int opsys_major_version = 0;
static const char* opsys_legacy = NULL;

// temporary attributes for raw utsname info
static const char* utsname_sysname = NULL;
static const char* utsname_nodename = NULL;
static const char* utsname_release = NULL;
static const char* utsname_version = NULL;
static const char* utsname_machine = NULL;

void 
init_utsname(void)
{
	struct utsname buf;

	if( uname(&buf) < 0 ) {
		return;
	}

	utsname_sysname = strdup( buf.sysname );
	if( !utsname_sysname ) {
		EXCEPT( "Out of memory!" );
	}

	utsname_nodename = strdup( buf.nodename );
	if( !utsname_nodename ) {
		EXCEPT( "Out of memory!" );
	}

	utsname_release = strdup( buf.release );
	if( !utsname_release ) {
		EXCEPT( "Out of memory!" );
	}

	utsname_version = strdup( buf.version );
	if( !utsname_version ) {
		EXCEPT( "Out of memory!" );
	}

	utsname_machine = strdup( buf.machine );
	if( !utsname_machine ) {
		EXCEPT( "Out of memory!" );
	}
        
	if ( utsname_sysname && utsname_nodename && utsname_release ) {
		utsname_inited = TRUE;
	}
}

void
init_arch(void)
{
	struct utsname buf;

	if( uname(&buf) < 0 ) {
		return;
	}

	uname_arch = strdup( buf.machine );
	if( !uname_arch ) {
		EXCEPT( "Out of memory!" );
	}

	uname_opsys = strdup( buf.sysname );
	if( !uname_opsys ) {
		EXCEPT( "Out of memory!" );
	}

	// 02-14-2012 bgietzel
	// New section for determining OpSys related params
	// Find values for MacOS, BSD, Linux, then everything else (Unix)
	// Windows params are set earlier in this code 

	// Param Changes 
	// NAME             OLD  ==> Linux       | BSD          | UNIX    | Windows
	// ---------------------------------------------------------------------------
	// OpSys =         LINUX ==> LINUX       | OSX          | AIX     | WINDOWS
	// OpSysAndVer =   LINUX ==> RedHat5     | MacOSX7      | AIX53   | WINDOWS601
	// OpSysVer =        206 ==> 501         | 703          | 503     | 601
	// OpSysShortName =  N/A ==> Linux       | MACOSX       | AIX     | Win7 
	// OpSysLongName =   N/A ==> Red Hat 5.1 | MACOSX 7.3   | AIX 5.3 | Windows 7 SP2
	// OpSysMajorVer =   N/A ==> 5           | 7            | 5       | 601
	// OpSysName(dist) = N/A ==> RedHat      | Lion         | AIX     | Windows7
	// OpSysLegacy	   = N/A ==> LINUX       | OSX          | AIX     | WINNT61

#if defined( Darwin )

	opsys = strdup( "macOS" );
	opsys_legacy = strdup( "OSX" );
	opsys_short_name = strdup( "macOS" );
	// This sets the following:
	//   opsys_long_name
	//   opsys_major_version
	//   opsys_name
	sysapi_get_darwin_info();
	opsys_version = sysapi_translate_opsys_version( opsys_long_name );
	opsys_versioned = sysapi_find_opsys_versioned( opsys_short_name, opsys_major_version );
	
#elif defined( CONDOR_FREEBSD )

	opsys = strdup( "FREEBSD" );
	opsys_legacy = strdup( opsys );
 	opsys_short_name = strdup( "FreeBSD" );
	opsys_name = strdup ( opsys_short_name );
	opsys_long_name = sysapi_get_bsd_info( opsys_short_name, buf.release ); 
	opsys_major_version = sysapi_find_major_version( buf.release );
	opsys_versioned = sysapi_find_opsys_versioned( opsys_name, opsys_major_version );
	opsys_version = sysapi_translate_opsys_version( buf.release );
#else

	if(MATCH == strcasecmp(uname_opsys, "linux") )
        {
		opsys = strdup( "LINUX" );
		opsys_legacy = strdup( opsys );
		opsys_long_name = sysapi_get_linux_info();
		opsys_name = sysapi_find_linux_name( opsys_long_name );
		opsys_short_name = strdup( opsys_name );
		opsys_major_version = sysapi_find_major_version( opsys_long_name );
		opsys_version = sysapi_translate_opsys_version( opsys_long_name );
		opsys_versioned = sysapi_find_opsys_versioned( opsys_name, opsys_major_version );

     	} else
        {
		// if opsys_long_name is "Solaris 11.250"
		//    opsys_name      is "Solaris"
		//    opsys_legacy    is "SOLARIS"
		//    opsys           is "SOLARIS"
		//    opsys_short_name is "Solaris"
		//    opsys_versioned  is "Solaris11"
		opsys_long_name = sysapi_get_unix_info( buf.sysname, buf.release, buf.version );
		char * p = strdup( opsys_long_name );
		opsys_name = p; p = strchr(p, ' '); if (p) *p = 0;
		opsys_legacy = p = strdup( opsys_name ); for (; *p; ++p) { *p = toupper(*p); }
		opsys = strdup( opsys_legacy );
		opsys_short_name = strdup( opsys_name );
		opsys_major_version = sysapi_find_major_version( opsys_long_name );
		opsys_version = sysapi_translate_opsys_version( opsys_long_name );
		opsys_versioned = sysapi_find_opsys_versioned( opsys_name, opsys_major_version );
        }

#endif

        if (!opsys) {
                opsys = strdup("Unknown");
        }
        if (!opsys_name) {
                opsys_name = strdup("Unknown");
        }
        if (!opsys_short_name) {
                opsys_short_name = strdup("Unknown");
        }
        if (!opsys_long_name) {
                opsys_long_name = strdup("Unknown");
        }
        if (!opsys_versioned) {
                opsys_versioned = strdup("Unknown");
        }
        if (!opsys_legacy) {
                opsys_legacy = strdup("Unknown");
        }


	// Now find the arch
	arch = sysapi_translate_arch( buf.machine, buf.sysname );

	if ( arch && opsys ) {
		arch_inited = TRUE;
	}
}

void sysapi_opsys_dump(int category)
{
	// Print out param values to the logfiles for debugging
	dprintf(category, "OpSysMajorVer:  %d \n", opsys_major_version);
	dprintf(category, "OpSysShortName:  %s \n", opsys_short_name);
	dprintf(category, "OpSysLongName:  %s \n", opsys_long_name);
	dprintf(category, "OpSysAndVer:  %s \n", opsys_versioned);
	dprintf(category, "OpSysLegacy:  %s \n", opsys_legacy);
	dprintf(category, "OpSysName:  %s \n", opsys_name);
	dprintf(category, "OpSysVer:  %d \n", opsys_version);
	dprintf(category, "OpSys:  %s \n", opsys);
}

// Darwin (MacOS) methods
#if defined( Darwin )
void
sysapi_get_darwin_info(void)
{
    char ver_str[255] = "";
    const char *args[] = {"/usr/bin/sw_vers", "-productVersion", NULL};
    FILE *output_fp;

    char tmp_info[262];
    const char *os_name = "macOS ";
 
    if ((output_fp = my_popenv(args, "r", FALSE)) != NULL) {
	fgets(ver_str, 255, output_fp);
	my_pclose(output_fp);
    }

	int major = 0, minor = 0, patch = 0;
	int fields = sscanf(ver_str, "%d.%d.%d", &major, &minor, &patch);
	if (major < 10 || major > 12 || fields < 2 || (major == 10 && fields != 3)) {
		dprintf(D_FULLDEBUG, "UNEXPECTED MacOS version string %s", ver_str);
	}

    sprintf( tmp_info, "%s%d.%d", os_name, major, minor);
    opsys_long_name = strdup( tmp_info );

    if( !opsys_long_name ) {
    	EXCEPT( "Out of memory!" );
    }

	opsys_major_version = major;

	const char *osname = "Unknown";
	if ( major == 12 ) {
		osname = "Monterey";
	} else if ( major == 11 ) {
		osname = "BigSur";
	} else if ( major == 10 && minor == 15 ) {
		osname = "Catalina";
	} else if ( major == 10 && minor == 14 ) {
		osname = "Mojave";
	} else if ( major == 10 && minor == 13 ) {
		osname = "HighSierra";
	}

	opsys_name = strdup(osname);

	if (!opsys_name) {
		EXCEPT("Out of memory!");
	}
}

#elif defined( CONDOR_FREEBSD )
// BSD methods
const char * 
sysapi_get_bsd_info( const char *tmp_opsys_short_name, const char *tmp_release) 
{
    char tmp_info[strlen(tmp_opsys_short_name) + 1 + 10];
    char *info_str;

    sprintf( tmp_info, "%s%s", tmp_opsys_short_name, tmp_release);
    info_str = strdup( tmp_info );

    if( !info_str ) {
    	EXCEPT( "Out of memory!" );
    }

    return info_str;
}
#endif

// Linux methods
const char *
sysapi_get_linux_info(void)
{
	char* info_str = NULL;
	FILE *my_fp;
	const char * etc_issue_path[] = { "/etc/issue","/etc/redhat-release","/etc/system-release","/etc/issue.net",NULL };
	int i;

	for (i=0;etc_issue_path[i];i++) {
	// read the first line only
	my_fp = safe_fopen_wrapper_follow(etc_issue_path[i], "r");
	if ( my_fp != NULL ) {
		char tmp_str[200] = {0};
		char *ret = fgets(tmp_str, sizeof(tmp_str), my_fp);
		if (ret == NULL) {
			strcpy( tmp_str, "Unknown" );
		} 
		dprintf(D_FULLDEBUG, "Result of reading %s:  %s \n", etc_issue_path[i], tmp_str);
		fclose(my_fp);

		// trim trailing spaces and other cruft
		int len = strlen(tmp_str);
		while (len > 0) {
			while (len > 0 && 
				   (isspace((int)(tmp_str[len-1])) || tmp_str[len-1] == '\n') ) {
				tmp_str[--len] = 0;
			}

			// Ubuntu and Debian have \n \l at the end of the issue string
			// this looks like a bug, in any case, we want to strip it
			if (len > 2 && 
				tmp_str[len-2] == '\\' && (tmp_str[len-1] == 'n' || tmp_str[len-1] == 'l')) {
				tmp_str[--len] = 0;
				tmp_str[--len] = 0;
			} else {
				break;
			}
		}
 
		info_str = strdup( tmp_str );
	} else {
		continue;
	}
	const char *temp_opsys_name = sysapi_find_linux_name( info_str );
	ASSERT(temp_opsys_name);
	if ( strcmp(temp_opsys_name,"LINUX")==0 ) {
		// failed to find what we want in this issue file; try another
		free(const_cast<char*>(temp_opsys_name));
		free(info_str);
		info_str = NULL;
	} else {
		// we found distro info in this line, stop searching
		free(const_cast<char*>(temp_opsys_name));
		break;
	}
	}
	
	if (!info_str) {
		info_str = strdup( "Unknown" );
	}

	if( !info_str ) {
		EXCEPT( "Out of memory!" );
	}

	return info_str;
}

const char *
sysapi_find_linux_name( const char *info_str )
{
	char* distro;
        char* distro_name_lc = strdup( info_str );

        int i = 0;
        char c;
        while (distro_name_lc[i])
        {
                c = distro_name_lc[i];
                distro_name_lc[i] = tolower(c);
                i++;
        }

	if ( strstr(distro_name_lc, "red") && strstr(distro_name_lc, "hat") )
	{
                distro = strdup( "RedHat" );
	}	
	else if ( strstr(distro_name_lc, "fedora") )
	{
		distro = strdup( "Fedora" );
	}
	else if ( strstr(distro_name_lc, "ubuntu") )
	{
		distro = strdup( "Ubuntu" );
	}
	else if ( strstr(distro_name_lc, "debian") )
	{
		distro = strdup( "Debian" );
	}
   	else if ( strstr(distro_name_lc, "scientific") && strstr(distro_name_lc, "cern") )
        {
                distro = strdup("SLCern");
        }
        else if ( strstr(distro_name_lc, "scientific") && strstr(distro_name_lc, "slf") )
        {
                distro = strdup("SLFermi");
        }
   	else if ( strstr(distro_name_lc, "scientific") )
        {
                distro = strdup("SL");
        }
        else if ( strstr(distro_name_lc, "centos") )
        {
                distro = strdup("CentOS");
        }  
        else if ( strstr(distro_name_lc, "rocky") )
        {
                distro = strdup("Rocky");
        }
        else if ( strstr(distro_name_lc, "amazon linux") )
        {
                distro = strdup("AmazonLinux");
        }  
        else if ( strstr(distro_name_lc, "opensuse") )
        {
                distro = strdup("openSUSE");
	}
        else if ( strstr(distro_name_lc, "suse") )
        {
                distro = strdup("SUSE");
        } else 
	{
                distro = strdup("LINUX");
 	}
	
  	if( !distro ) {
                EXCEPT( "Out of memory!" );
        }
	free( distro_name_lc );
	return distro;
}

// Unix methods
const char *
sysapi_get_unix_info( const char *sysname,
			const char *release,
			const char *version)
{
	char tmp[64];
	const char * pver="";
	char *tmpopsys;

	// Get OPSYS
	if( !strcmp(sysname, "SunOS")
		|| !strcmp(sysname, "solaris" ) ) //LDAP entry
	{
		if ( !strcmp(release, "2.11") //LDAP entry
			|| !strcmp(release, "5.11") )
		{
			pver = "211";
		} else if ( !strcmp(release, "2.10") //LDAP entry
			|| !strcmp(release, "5.10") )
		{
			pver = "210";
		}
		else if ( !strcmp(release, "2.9") //LDAP entry
			|| !strcmp(release, "5.9") )
		{
			pver = "29";
		}
		else if ( !strcmp(release, "2.8") //LDAP entry
			|| !strcmp(release, "5.8") )
		{
			pver = "28";
		}
		else if ( !strcmp(release, "2.7") //LDAP entry
			|| !strcmp(release, "5.7") )
		{
			pver = "27";
		}
		else if( !strcmp(release, "5.6")
			||  !strcmp(release, "2.6") ) //LDAP entry
		{
			pver = "26";
		}
		else if ( !strcmp(release, "5.5.1")
			|| !strcmp(release, "2.5.1") ) //LDAP entry
		{
			pver = "251";
		}
		else if ( !strcmp(release, "5.5")
			|| !strcmp(release, "2.5") ) //LDAP entry
		{
			pver = "25";
		}
		else {
            pver = release;
		}
		if (!strcmp(version,"11.0")) version = "11";
        sprintf( tmp, "Solaris %s.%s", version, pver );
	}

	else {
			// Unknown, just use what uname gave:
		sprintf( tmp, "%s", sysname);
        pver = release;
	}
        if (pver) {
            strcat( tmp, pver );
        }

	tmpopsys = strdup( tmp );
	if( !tmpopsys ) {
		EXCEPT( "Out of memory!" );
	}
	return( tmpopsys );
}

// generic methods
int 
sysapi_find_major_version ( const char *info_str ) 
{
    	const char * verstr = info_str;
    	int major = 0;

	// In the case where something fails above and verstr = Unknown, return 0
	if ( !strcmp(verstr, "Unknown") ){
		return major;
	}

    	// skip any leading non-digits.
   	while (verstr[0] && (verstr[0] < '0' || verstr[0] > '9')) { 
       		++verstr;
  	}
    	// parse digits until the first non-digit as the
  	// major version number.
 	//
    	while (verstr[0]) {
        	if (verstr[0] >= '0' && verstr[0] <= '9') {
            		major = major * 10 + (verstr[0] - '0');
        	} else {
           		break;
        	}
		++verstr;
	}

	return major;
}

const char *
sysapi_find_opsys_versioned( const char *tmp_opsys, int tmp_opsys_major_version )
{
        char tmp_opsys_versioned[strlen(tmp_opsys) + 1 + 10];
        char *my_opsys_versioned;

        sprintf( tmp_opsys_versioned, "%s%d", tmp_opsys, tmp_opsys_major_version);

	my_opsys_versioned = strdup( tmp_opsys_versioned );
        if( !my_opsys_versioned ) {
                EXCEPT( "Out of memory!" );
        }
        return my_opsys_versioned;
}

int
sysapi_translate_opsys_version ( const char *info_str)
{
    const char * psz = info_str;
    int major = 0;

    // In the case where release == Unknown, return 0
    if ( !strcmp(psz, "Unknown") ){
	return major;
    }

    // skip any leading non-digits.
    while (psz[0] && (psz[0] < '0' || psz[0] > '9')) {
       ++psz;
    }

    // parse digits until the first non-digit as the
    // major version number.
    //
    while (psz[0]) {
        if (psz[0] >= '0' && psz[0] <= '9') {
            major = major * 10 + (psz[0] - '0');
        } else {
           break;
        }
        ++psz;
    }

    // if the major version number ended with '.' parse
    // at most 2 more digits as the minor version number.
    //
    int minor = 0;
    if (psz[0] == '.') {
       ++psz;
       if (psz[0] >= '0' && psz[0] <= '9') {
          minor = psz[0] - '0';
          ++psz;
       }
       if (psz[0] >= '0' && psz[0] <= '9') {
          minor = minor * 10 + psz[0] - '0';
       }
    }

    return (major * 100) + minor;
}

// find arch 
const char *
sysapi_translate_arch( const char *machine, const char *)
{
	char tmp[64];
	char *tmparch;

		// Get ARCH
		//mikeu: I modified this to also accept values from Globus' LDAP server
	if( !strcmp(machine, "i86pc") ) {
		sprintf( tmp, "INTEL" );
	}
	else if( !strcmp(machine, "i686") ) {
		sprintf( tmp, "INTEL" );
	}
	else if( !strcmp(machine, "i586") ) {
		sprintf( tmp, "INTEL" );
	}
	else if( !strcmp(machine, "i486") ) {
		sprintf( tmp, "INTEL" );
	}
	else if( !strcmp(machine, "i386") ) { //LDAP entry
#if defined(Darwin)
		/* Mac OS X often claims to be i386 in uname, even if the
		 * hardware is x86_64 and the OS can run 64-bit binaries.
		 * We'll base our architecture name on the default build
		 * target for gcc. In 10.5 and earlier, that's i386.
		 * On 10.6, it's x86_64.
		 * The value we're querying is the kernel version.
		 * 10.6 kernels have a version that starts with "10."
		 * Older versions have a lower first number.
		 */
		int ret;
		char val[32];
		size_t len = sizeof(val);

		/* assume x86 */
		sprintf( tmp, "INTEL" );
		ret = sysctlbyname("kern.osrelease", &val, &len, NULL, 0);
		if (ret == 0 && strncmp(val, "10.", 3) == 0) {
			/* but we could be proven wrong */
			sprintf( tmp, "X86_64" );
		}
#else
		sprintf( tmp, "INTEL" );
#endif
	}
	else if( !strcmp(machine, "ia64") ) {
		sprintf( tmp, "IA64" );
	}
	else if( !strcmp(machine, "x86_64") ) {
		sprintf( tmp, "X86_64" );
	}
	//
	// FreeBSD 64-bit reports themselves as "amd64"
	// Andy - 01/25/2008
	//
	else if( !strcmp(machine, "amd64") ) {
		sprintf( tmp, "X86_64" );
	}
	else if( !strcmp(machine, "Power Macintosh") ) { //LDAP entry
		sprintf( tmp, "PPC" );
	}
	else if( !strcmp(machine, "ppc") ) {
		sprintf( tmp, "PPC" );
	}
	else if( !strcmp(machine, "ppc32") ) {
		sprintf( tmp, "PPC" );
	}
	else if( !strcmp(machine, "ppc64") ) {
		sprintf( tmp, "PPC64" );
	}
	else {
			// Unknown, just use what uname gave:
		sprintf( tmp, "%s", machine );
	}

	tmparch = strdup( tmp );
	if( !tmparch ) {
		EXCEPT( "Out of memory!" );
	}
	return( tmparch );
}


const char *
sysapi_condor_arch(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return arch;
}


const char *
sysapi_opsys(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys;
}

int sysapi_opsys_version()
{
	if( ! arch_inited ) {
		init_arch();
	}
    return opsys_version;
}

const char *
sysapi_opsys_versioned(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys_versioned;
}

const char *
sysapi_uname_arch(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return uname_arch;
}


const char *
sysapi_uname_opsys(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return uname_opsys;
}

int
sysapi_opsys_major_version(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys_major_version;
}

const char *
sysapi_opsys_name(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys_name;
}

const char *
sysapi_opsys_long_name(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys_long_name;
}

const char *
sysapi_opsys_short_name(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys_short_name;
}

const char *
sysapi_opsys_legacy(void)
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys_legacy;
}

// temporary attributes for raw utsname info
const char *
sysapi_utsname_sysname(void)
{
	if( ! utsname_inited ) {
		init_utsname();
	}
	return utsname_sysname;
}

const char *
sysapi_utsname_nodename(void)
{
	if( ! utsname_inited ) {
		init_utsname();
	}
	return utsname_nodename;
}

const char *
sysapi_utsname_release(void)
{
	if( ! utsname_inited ) {
		init_utsname();
	}
	return utsname_release;
}

const char *
sysapi_utsname_version(void)
{
	if( ! utsname_inited ) {
		init_utsname();
	}
	return utsname_version;
}


const char *
sysapi_utsname_machine(void)
{
	if( ! utsname_inited ) {
		init_utsname();
	}
	return utsname_machine;
}


#endif /* ! WIN32 */
