/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "match_prefix.h"

static char *_FileName_ = __FILE__;

extern "C" {

#if defined(WIN32)

char *
my_arch()
{
	static char answer[1024];	
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	switch(info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		sprintf(answer, "INTEL");
		break;
	case PROCESSOR_ARCHITECTURE_MIPS:
		sprintf(answer, "MIPS");
		break;
	case PROCESSOR_ARCHITECTURE_ALPHA:
		sprintf(answer, "ALPHA");
		break;
	case PROCESSOR_ARCHITECTURE_PPC:
		sprintf(answer, "PPC");
		break;
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
	default:
		sprintf(answer, "UNKNOWN");
		break;
	}

	return answer;
}

char *
my_opsys()
{
	static char answer[1024];
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
	} else {
		sprintf(answer, "ERROR");
	}

	return answer;
}

#else

static int arch_inited = FALSE;
static char* arch = NULL;
static char* opsys = NULL;

void
init_arch() 
{
	struct utsname buf;	
	char tmp[64];

	if( uname(&buf) < 0 ) {
		return;
	}

		// Get ARCH
	if( !strcmp(buf.machine, "alpha") ) {
		sprintf( tmp, "ALPHA" );
	} else if( !strcmp(buf.machine, "i86pc") ) {
		sprintf( tmp, "INTEL" );
	} else if( !strcmp(buf.machine, "i686") ) {
		sprintf( tmp, "INTEL" );
	} else if( !strcmp(buf.machine, "i586") ) {
		sprintf( tmp, "INTEL" );
	} else if( !strcmp(buf.machine, "i486") ) {
		sprintf( tmp, "INTEL" );
	} else if( match_prefix(buf.sysname, "IRIX") ) {
		sprintf( tmp, "SGI" );
	} else if( !strcmp(buf.machine, "sun4u") ) {
		sprintf( tmp, "SUN4u" );
	} else if( !strcmp(buf.machine, "sun4m") ) {
		sprintf( tmp, "SUN4x" );
	} else if( !strcmp(buf.machine, "sun4c") ) {
		sprintf( tmp, "SUN4x" );
	} else {
			// Unknown, just use what uname gave:
		sprintf( tmp, buf.machine );
	}
	arch = strdup( tmp );
	if( !arch ) {
		EXCEPT( "Out of memory!" );
	}

		// Get OPSYS
	if( !strcmp(buf.sysname, "Linux") ) {
		if( access("/lib/libc.so.6", R_OK) != 0 ) {
			sprintf( tmp, "LINUX" );
		} else { 
			sprintf( tmp, "LINUX-GLIBC" );
		}
	} else if( !strcmp(buf.sysname, "SunOS") ) {
		if( !strcmp(buf.release, "5.6") ) {
			sprintf( tmp, "SOLARIS26" );
		} else if ( !strcmp(buf.release, "5.5.1") ) {
			sprintf( tmp, "SOLARIS251" );
		} else if ( !strcmp(buf.release, "5.5") ) {
			sprintf( tmp, "SOLARIS25" );
		} else {
			sprintf( tmp, "SOLARIS%s", buf.release );
		}
	} else if( !strcmp(buf.sysname, "OSF1") ) {
		sprintf( tmp, "OSF1" );
	} else if( match_prefix(buf.sysname, "IRIX") ) {
		if( buf.release[0] == '6' ) {
			sprintf( tmp, "IRIX6" );
		} else {
			sprintf( tmp, "IRIX%s", buf.release );
		}
	} else {
			// Unknown, just use what uname gave:
		sprintf( tmp, "%s%s", buf.sysname, buf.release );
	}
	opsys = strdup( tmp );
	if( !opsys ) {
		EXCEPT( "Out of memory!" );
	}
}



char *
my_arch()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return arch;
}

char *
my_opsys()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys;
}

#endif /* ! WIN32 */

} /* extern "C" */

