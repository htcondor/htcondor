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


#include "condor_common.h"
#include "condor_debug.h"
#include "match_prefix.h"
#include "sysapi.h"
#include "sysapi_externs.h"

#if defined(Darwin)
#include <sys/sysctl.h>
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

static int opsys_version = 0;

const char *
sysapi_opsys_versioned(void)
{
    static char answer[128] = {0};
    if (answer[0])
       return answer;

	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (GetVersionEx(&info) > 0) {
		switch(info.dwPlatformId) {
		case VER_PLATFORM_WIN32s:
			sprintf(answer, "WIN32s%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			sprintf(answer, "WIN32%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_NT:
			sprintf(answer, "WINNT%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		default:
			sprintf(answer, "UNKNOWN");
			break;
		}
        opsys_version = info.dwMajorVersion * 100 + info.dwMinorVersion;
	} else {
		sprintf(answer, "ERROR");
	}

	return answer;
}

const char *
sysapi_opsys(void)
{
    if (_sysapi_opsys_is_versioned) {
        return sysapi_opsys_versioned();
    }
    return "WINDOWS";
}

int sysapi_opsys_version()
{
    if ( ! opsys_version) {
         sysapi_opsys_versioned(); // init opsys_version
    }
    return opsys_version;
}

#else

static int arch_inited = FALSE;
static const char* arch = NULL;
static const char* uname_arch = NULL;
static const char* opsys = NULL;
static const char* opsys_versioned = NULL;
static const char* uname_opsys = NULL;
static int opsys_version = 0;

#ifdef HPUX
const char* get_hpux_arch();
#endif

#ifdef AIX
const char* get_aix_arch( struct utsname* );
#endif

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

	arch = sysapi_translate_arch( buf.machine, buf.sysname );
	opsys = sysapi_translate_opsys( buf.sysname, buf.release, buf.version, _sysapi_opsys_is_versioned );
	opsys_versioned = sysapi_translate_opsys( buf.sysname, buf.release, buf.version, true );
    opsys_version = sysapi_translate_opsys_version( buf.sysname, buf.release, buf.version );

	if ( arch && opsys ) {
		arch_inited = TRUE;
	}
}

const char *
sysapi_translate_arch( const char *machine, const char *)
{
	char tmp[64];
	char *tmparch;

#if defined(AIX)
	/* AIX machines have a ton of different models encoded into the uname
		structure, so go to some other function to decode and group the
		architecture together */
	struct utsname buf;

	if( uname(&buf) < 0 ) {
		return NULL;
	}

	return( get_aix_arch( &buf ) );

#elif defined(HPUX)

	return( get_hpux_arch( ) );
#else

		// Get ARCH
		//mikeu: I modified this to also accept values from Globus' LDAP server
	if( !strcmp(machine, "alpha") ) {
		sprintf( tmp, "ALPHA" );
	}
	else if( !strcmp(machine, "i86pc") ) {
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
	else if( !strcmp(machine, "sun4u") ) {
		sprintf( tmp, "SUN4u" );
	}
	else if( !strcmp(machine, "sun4m") ) {
		sprintf( tmp, "SUN4x" );
	}
	else if( !strcmp(machine, "sun4c") ) {
		sprintf( tmp, "SUN4x" );
	}
	else if( !strcmp(machine, "sparc") ) { //LDAP entry
		sprintf( tmp, "SUN4x" );
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
		sprintf( tmp, machine );
	}

	tmparch = strdup( tmp );
	if( !tmparch ) {
		EXCEPT( "Out of memory!" );
	}
	return( tmparch );
#endif /* if HPUX else */
}

const char *
sysapi_translate_opsys( const char *sysname,
						const char *release,
						const char *version,
                        int         append_version)
{
	char tmp[64];
    char ver[24];
    const char * pver="";
	char *tmpopsys;

		// Get OPSYS
	if( !strcmp(sysname, "Linux") ) {
		sprintf( tmp, "LINUX" );
	}
	else if( !strcmp(sysname, "linux") ) { //LDAP entry
		sprintf( tmp, "LINUX" );
	}

	else if( !strcmp(sysname, "SunOS")
		|| !strcmp(sysname, "solaris" ) ) //LDAP entry
	{
        sprintf( tmp, "SOLARIS" );
		if ( !strcmp(release, "2.10") //LDAP entry
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
	}

	else if( !strcmp(sysname, "HP-UX") ) {
        sprintf( tmp, "HPUX" );
		if( !strcmp(release, "B.10.20") ) {
			pver = "10";
		}
		else if( !strcmp(release, "B.11.00") ) {
			pver = "11";
		}
		else if( !strcmp(release, "B.11.11") ) {
			pver = "11";
		}
		else {
			pver = release;
		}
	}
	else if ( !strncmp(sysname, "Darwin", 6) ) {
			//  there's no reason to differentiate across versions of
			//  OSX, since jobs can freely run on any version, etc.
		sprintf( tmp, "OSX");
	}
	else if ( !strncmp(sysname, "AIX", 3) ) {
        sprintf(tmp, "%s", sysname);
		if ( !strcmp(version, "5") ) {
			sprintf(ver, "%s%s", version, release);
            pver = ver;
		}
	}
	else if ( !strncmp(sysname, "FreeBSD", 7) ) {
			//
			// Pull the major version # out of the release information
			//
		sprintf( tmp, "FREEBSD" );
        sprintf( ver, "%c", release[0]);
        pver = ver;
	}
	else {
			// Unknown, just use what uname gave:
		sprintf( tmp, "%s", sysname);
        pver = release;
	}
    if (append_version && pver) {
        strcat( tmp, pver );
    }

	tmpopsys = strdup( tmp );
	if( !tmpopsys ) {
		EXCEPT( "Out of memory!" );
	}
	return( tmpopsys );
}

int sysapi_translate_opsys_version( 
    const char * /*sysname*/,
	const char *release,
	const char * /*version*/ )
{
    const char * psz = release;

    // skip any leading non-digits.
    while (psz[0] && (psz[0] < '0' || psz[0] > '9')) {
       ++psz;
    }

    // parse digits until the first non-digit as the
    // major version number.
    //
    int major = 0;
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


#if defined(HPUX)
const char*
get_hpux_arch( void )
{
	/* As of 3/22/2006, it has been ten years since HP has made
		a HPPA1 machine.  Just assume that this is a HPPA2 machine,
		and if it is not the user can force the setting in the
		ARCH config file setting
   	*/
#if defined(IS_IA64_HPUX)
	return strdup("IA64");
#else
	return strdup("HPPA2");
#endif

}
#endif /* HPUX */

#ifdef AIX

const char*
get_aix_arch( struct utsname *buf )
{
	char *ret = "UNK";
	char d[3];
	int model;

	/* The model number is encoded in the last two non zero digits of the
		model code. */
	d[0] = buf->machine[ strlen( buf->machine ) - 4 ];
	d[1] = buf->machine[ strlen( buf->machine ) - 3 ];
	d[2] = '\0';
	model = strtol(d, NULL, 16);

	/* ok, group the model numbers into processor families: */
	switch(model)
	{
	  case 0x10:
		/* Model 530/730 */
		break;

	  case 0x11:
	  case 0x14:
		/* Model 540 */
		break;

	  case 0x18:
		/* Model 530H */
		break;

	  case 0x1C:
		/* Model 550 */
		break;

	  case 0x20:
		/* Model 930 */
		break;

	  case 0x2E:
		/* Model 950/950E */
		break;

	  case 0x30:
		/* Model 520 or 740/741 */
		break;

	  case 0x31:
		/* Model 320 */
		break;

	  case 0x34:
		/* Model 520H */
		break;

	  case 0x35:
		/* Model 32H/320E */
		break;

	  case 0x37:
		/* Model 340/34H */
		break;

	  case 0x38:
		/* Model 350 */
		break;

	  case 0x41:
		/* Model 220/22W/22G/230 */
		break;

	  case 0x42:
		/* Model 41T/41W */
		break;

	  case 0x43:
		/* Model M20 */
		break;

	  case 0x45:
		/* Model 220/M20/230/23W */
		break;

	  case 0x46:
	  case 0x49:
		/* Model 250 */
		break;

	  case 0x47:
		/* Model 230 */
		break;

	  case 0x48:
		/* Model C10 */
		break;

	  case 0x4C:
		/* PowerPC 603/604 model, and later */
		ret = strdup("PWR3II");
		break;

	  case 0x4D:
		/* Model 40P */
		break;

	  case 0x57:
		/* Model 390/3AT/3BT */
		break;

	  case 0x58:
		/* Model 380/3AT/3BT */
		break;

	  case 0x59:
		/* Model 39H/3CT */
		break;

	  case 0x5C:
		/* Model 560 */
		break;

	  case 0x63:
		/* Model 970/97B */
		break;

	  case 0x64:
		/* Model 980/98B */
		break;

	  case 0x66:
		/* Model 580/58F */
		break;

	  case 0x67:
		/* Model 570/770/R10 */
		break;

	  case 0x70:
		/* Model 590 */
		break;

	  case 0x71:
		/* Model 58H */
		break;

	  case 0x72:
		/* Model 59H/58H/R12/R20 */
		break;

	  case 0x75:
		/* Model 370/375/37T */
		break;

	  case 0x76:
		/* Model 360/365/36T */
		break;

	  case 0x77:
		/* Model 315/350/355/510/550H/550L */
		break;

	  case 0x79:
		/* Model 591 */
		break;

	  case 0x80:
		/* Model 990 */
		break;

	  case 0x81:
		/* Model R24 */
		break;

	  case 0x82:
		/* Model R00/R24 */
		break;

	  case 0x89:
		/* Model 595 */
		break;

	  case 0x90:
		/* Model C20 */
		break;

	  case 0x91:
		/* Model 42T */
		break;

	  case 0x94:
		/* Model 397 */
		break;

	  case 0xA0:
		/* Model J30 */
		break;

	  case 0xA1:
		/* Model J40 */
		break;

	  case 0xA3:
		/* Model R30 */
		break;

	  case 0xA4:
		/* Model R40 */
		break;

	  case 0xA6:
		/* Model G30 */
		break;

	  case 0xA7:
		/* Model G40 */
		break;

	  case 0xC4:
		/* Model F30 */
		break;

	  default:
	  	/* unknown model, use default value of ret */
		break;
	}

	return ret;
}

#endif /* AIX */

#endif /* ! WIN32 */
