/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "match_prefix.h"
#include "sysapi.h"

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

const char *
sysapi_condor_arch()
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


const char *
sysapi_uname_arch() 
{
	return sysapi_condor_arch();
}


const char *
sysapi_uname_opsys() 
{
	return sysapi_opsys();
}


const char *
sysapi_opsys()
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
static char* uname_arch = NULL;
static char* opsys = NULL;
static char* uname_opsys = NULL;

#ifdef HPUX
char* get_hpux_arch( struct utsname* );
#endif

#ifdef AIX
char* get_aix_arch( struct utsname* );
#endif

void
init_arch() 
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
	opsys = sysapi_translate_opsys( buf.sysname, buf.release, buf.version );

	if ( arch && opsys ) {
		arch_inited = TRUE;
	}
}

char *
sysapi_translate_arch( char *machine, char *sysname )
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
	/*
	  On HPUX, to figure out if we're HPPA1 or HPPA2, we have to
	  lookup what we get back from uname in a file that lists all the
	  different HP models and what kinds of CPU each one has.  We do
	  this in a seperate function to keep this function somewhat
	  readable.   -Derek Wright 7/20/98
	*/
	struct utsname buf;	

	if( uname(&buf) < 0 ) {
		return NULL;
	}

	return( get_hpux_arch( &buf ) );
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
		sprintf( tmp, "INTEL" );
	} 
	else if( !strcmp(machine, "ia64") ) {
		sprintf( tmp, "IA64" );
	}
	else if( !strncmp( sysname, "IRIX", 4 ) ) {
		sprintf( tmp, "SGI" );
	} 
	else if( !strcmp(machine, "mips") ) { //LDAP entry
		sprintf( tmp, "SGI" );
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

char *
sysapi_translate_opsys( char *sysname, char *release, char *version )
{
	char tmp[64];
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
		
		if ( !strcmp(release, "2.9") //LDAP entry
			|| !strcmp(release, "5.9")
		)
		{
			sprintf( tmp, "SOLARIS29" );
		} 
		else if ( !strcmp(release, "2.8") //LDAP entry
			|| !strcmp(release, "5.8")
		)
		{
			sprintf( tmp, "SOLARIS28" );
		} 
		else if ( !strcmp(release, "2.7") //LDAP entry
			|| !strcmp(release, "5.7")
		)
		{
			sprintf( tmp, "SOLARIS27" );
		} 
		else if( !strcmp(release, "5.6") 
			||  !strcmp(release, "2.6") ) //LDAP entry
		{
			sprintf( tmp, "SOLARIS26" );
		} 
		else if ( !strcmp(release, "5.5.1") 
			|| !strcmp(release, "2.5.1") ) //LDAP entry
		{
			sprintf( tmp, "SOLARIS251" );
		} 
		else if ( !strcmp(release, "5.5") 
			|| !strcmp(release, "2.5") ) //LDAP entry
		{
			sprintf( tmp, "SOLARIS25" );
		} 
		else {
			sprintf( tmp, "SOLARIS%s", release );
		}
	} 

	else if( !strcmp(sysname, "OSF1") ) {
		sprintf( tmp, "OSF1" );
	} 
	else if( !strcmp(sysname, "HP-UX") ) {
		if( !strcmp(release, "B.10.20") ) {
			sprintf( tmp, "HPUX10" );
		} 
		else if( !strcmp(release, "B.11.00") ) {
			sprintf( tmp, "HPUX11" );
		} 
		else {
			sprintf( tmp, "HPUX%s", release );
		}
	} 
	else if( !strncmp(sysname, "IRIX", 4 ) 
		|| !strcmp( sysname, "irix" ))  //LDAP entry
	{
		if( !strcmp( release, "6.5" ) ) {
			sprintf( tmp, "IRIX65" );
		}
		else if( !strcmp( release, "6.2" ) ) {
			sprintf( tmp, "IRIX62" );
		} 
		else {
			sprintf( tmp, "IRIX%s", release );
		}
	} 
	else if ( !strncmp(sysname, "Darwin", 6) ) {
			//  there's no reason to differentiate across versions of
			//  OSX, since jobs can freely run on any version, etc.
		sprintf( tmp, "OSX");
	}
	else if ( !strncmp(sysname, "AIX", 3) ) {
		if ( !strcmp(version, "5") ) {
			sprintf(tmp, "%s%s%s", sysname, version, release);
		}
	}
	else {
			// Unknown, just use what uname gave:
		sprintf( tmp, "%s%s", sysname, release );
	}
	tmpopsys = strdup( tmp );
	if( !tmpopsys ) {
		EXCEPT( "Out of memory!" );
	}
	return( tmpopsys );
}


const char *
sysapi_condor_arch()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return arch;
}


const char *
sysapi_opsys()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return opsys;
}


const char *
sysapi_uname_arch()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return uname_arch;
}


const char *
sysapi_uname_opsys()
{
	if( ! arch_inited ) {
		init_arch();
	}
	return uname_opsys;
}


#if defined(HPUX)
char*
get_hpux_arch( struct utsname *buf )
{
	FILE* fp;
	static char *file = "/opt/langtools/lib/sched.models";
	char machinfo[4096], line[128];
	char *model;
	char cputype[128], cpumodel[128];
	int len, found_it = FALSE;
	char *tmparch;

	model = strchr( buf->machine, '/' );
	model++;
	len = strlen( model );

	fp = fopen( file, "r" );
	if( ! fp ) {
		return NULL;
	}	
	while( ! feof(fp) ) {
		fgets( line, 128, fp );
		if( !strncmp(line, model, len) ) {
			found_it = TRUE;
			break;
		}
	}
	fclose( fp );
	if( found_it ) {
  		sscanf( line, "%s\t%s", cpumodel, cputype );
		if( !strcmp(cputype, "2.0") ) {
			tmparch = strdup( "HPPA2" );
		} else {
			tmparch = strdup( "HPPA1" );
		}
		if( !tmparch ) {
			EXCEPT( "Out of memory!" );
		}
	} else {
		tmparch = strdup("Unknown");
	}
	return tmparch;
}
#endif /* HPUX */

#ifdef AIX

char*
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
