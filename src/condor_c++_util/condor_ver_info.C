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
#include "condor_ver_info.h"

extern char *mySubSystem;
extern "C" char *CondorVersion(void);
extern "C" char *CondorPlatform(void);

CondorVersionInfo::CondorVersionInfo(const char *versionstring, 
									 char *subsystem, char *platformstring)
{
	myversion.MajorVer = 0;
	myversion.Arch = NULL;
	myversion.OpSys = NULL;
	mysubsys = NULL;
	string_to_VersionData(versionstring,myversion);
	string_to_PlatformData(platformstring,myversion);

	if ( subsystem ) {
		mysubsys = strdup(subsystem);
	} else {
		mysubsys = strdup(mySubSystem);
	}
}

CondorVersionInfo::~CondorVersionInfo()
{
	if (mysubsys) free(mysubsys);
 	if(myversion.Arch) free(myversion.Arch);
 	if(myversion.OpSys) free(myversion.OpSys);
}

	
int
CondorVersionInfo::compare_versions(const char* VersionString1)
{
	VersionData_t ver1;

	ver1.Scalar = 0;
	string_to_VersionData(VersionString1,ver1);

	if ( ver1.Scalar < myversion.Scalar )
		return -1;
	if ( ver1.Scalar > myversion.Scalar )
		return 1;

	return 0;
}

int
CondorVersionInfo::compare_build_dates(const char* VersionString1)
{
	VersionData_t ver1;

	ver1.BuildDate = 0;
	string_to_VersionData(VersionString1,ver1);


	if ( ver1.BuildDate < myversion.BuildDate )
		return -1;
	if ( ver1.BuildDate > myversion.BuildDate )
		return 1;

	return 0;
}

bool
CondorVersionInfo::is_compatible(const char* other_version_string, 
								 const char* other_subsys)
{
	VersionData_t other_ver;

	if ( !(string_to_VersionData(other_version_string,other_ver)) ) {
		// say not compatible if we cannot even grok the version
		// string!
		return false;
	}

	/*********************************************************
		Add any logic on specific compatibilty issues right
		here!
	*********************************************************/

	// This version is not compatible with this for that, blah, blah


	/*********************************************************
		Now that specific rule checks are over, and we still have
		not made a decision, we fall back upon generalized rules.
		The general rule is within a given stable series, anything
		is both forwards and backwards compatible.  Other than that,
		everything is assumed to be backwards compatible but _not_
		forwards compatible.  We only check version numbers, not
		build dates, when performing these checks.
	*********************************************************/

		// check if both in same stable series
	if ( is_stable_series() && (myversion.MajorVer == other_ver.MajorVer)
		&& (myversion.MinorVer == other_ver.MinorVer) ) {
		return true;
	}

		// check if other version is older (or same) than we are; if so,
		// we are compatible because the assumption is we are 
		// backwards compatible.
	if ( other_ver.Scalar <= myversion.Scalar ) {
		return true;
	}


	return false;
}

bool
CondorVersionInfo::built_since_version(int MajorVer, int MinorVer, 
									   int SubMinorVer)
{
	int Scalar = MajorVer * 1000000 + MinorVer * 1000 
					+ SubMinorVer;

	return ( myversion.Scalar >= Scalar );
}

bool
CondorVersionInfo::built_since_date(int month, int day, int year)
{

		// Make a struct tm
	struct tm build_date;
	time_t BuildDate;
	build_date.tm_hour = 0;
	build_date.tm_isdst = 1;
	build_date.tm_mday = day;
	build_date.tm_min = 0;
	build_date.tm_mon = month - 1;
	build_date.tm_sec = 0;
	build_date.tm_year = year - 1900;

	if ( (BuildDate = mktime(&build_date)) == -1 ) {
		return false;
	}

	return ( myversion.BuildDate >= BuildDate );
}

bool
CondorVersionInfo::is_valid(const char* VersionString)
{
	bool ret_value;
	VersionData_t ver1;

	if ( !VersionString ) {
		return (myversion.MajorVer > 5);
	}

	ret_value = string_to_VersionData(VersionString,ver1);

	return ret_value;		
}


char *
CondorVersionInfo::get_version_from_file(const char* filename, 
										 char *ver, int maxlen)
{
	bool must_free = false;

	if (!filename)
		return NULL;
	
	if (ver && maxlen < 40 )
		return NULL;

	maxlen--;	// save room for the NULL character at the end

#ifdef WIN32
	static const char *readonly = "rb";	// need binary-mode on NT
#else
	static const char *readonly = "r";
#endif

	FILE *fp = fopen(filename,readonly);

	if ( !fp ) {
		// file not found
		return NULL;
	}
		
	if (!ver) {
		if ( !(ver = (char *)malloc(100)) ) {
			// out of memory
			return NULL;
		}
		must_free = true;
		maxlen = 100;
	}

	int i = 0;
	bool got_verstring = false;
	const char* verprefix = CondorVersion();
	int ch;
	while( (ch=fgetc(fp)) != EOF ) {
		if ( ch != verprefix[i] ) {
			i = 0;
			if ( ch != verprefix[0] ) {
				continue;
			}
		}

		ver[i++] = ch;

		if ( ch == ':' ) {
			while ( (i < maxlen) && ((ch=fgetc(fp)) != EOF) ) {
				ver[i++] = ch;
				if ( ch == '$' ) {
					got_verstring = true;
					ver[i] = '\0';
					break;
				}
			}
			break;
		}
	}

	fclose(fp);

	if ( got_verstring ) {
		return ver;
	} else {
		// could not find it
		if ( must_free ) {
			free( ver );
		}
		return NULL;
	}
}

char *
CondorVersionInfo::get_platform_from_file(const char* filename, 
										 char *platform, int maxlen)
{
	bool must_free = false;

	if (!filename)
		return NULL;
	
	if (platform && maxlen < 40 )
		return NULL;

	maxlen--;	// save room for the NULL character at the end

#ifdef WIN32
	static const char *readonly = "rb";	// need binary-mode on NT
#else
	static const char *readonly = "r";
#endif

	FILE *fp = fopen(filename,readonly);

	if ( !fp ) {
		// file not found
		return NULL;
	}
		
	if (!platform) {
		if ( !(platform = (char *)malloc(100)) ) {
			// out of memory
			return NULL;
		}
		must_free = true;
		maxlen = 100;
	}

	int i = 0;
	bool got_verstring = false;
	const char* platprefix = CondorPlatform();
	int ch;
	while( (ch=fgetc(fp)) != EOF ) {
		if ( ch != platprefix[i] ) {
			i = 0;
			if ( ch != platprefix[0] ) {
				continue;
			}
		}

		platform[i++] = ch;

		if ( ch == ':' ) {
			while ( (i < maxlen) && ((ch=fgetc(fp)) != EOF) ) {
				platform[i++] = ch;
				if ( ch == '$' ) {
					got_verstring = true;
					platform[i] = '\0';
					break;
				}
			}
			break;
		}
	}

	fclose(fp);

	if ( got_verstring ) {
		return platform;
	} else {
		// could not find it
		if ( must_free ) {
			free( platform );
		}
		return NULL;
	}
}
							
bool
CondorVersionInfo::string_to_VersionData(const char *verstring, 
									 VersionData_t & ver)
{
	// verstring looks like "$CondorVersion: 6.1.10 Nov 23 1999 $"

	if ( !verstring ) {
		// Use our own version number. 
		verstring = CondorVersion();

		// If we already computed myversion, we're done.
		if ( myversion.MajorVer ) {
			ver = myversion;
			return true;
		}
	}

	if ( strncmp(verstring,"$CondorVersion: ",16) != 0 ) {
		return false;
	}

	char *ptr = strchr(verstring,' ');
	ptr++;		// skip space after the colon

	sscanf(ptr,"%d.%d.%d ",&ver.MajorVer,&ver.MinorVer,&ver.SubMinorVer);

		// Sanity check: the world starts with Condor V6 !
	if (ver.MajorVer < 6  || ver.MinorVer > 99 || ver.SubMinorVer > 99) {
		myversion.MajorVer = 0;
		return false;
	}

	ver.Scalar = ver.MajorVer * 1000000 + ver.MinorVer * 1000 
					+ ver.SubMinorVer;

		// Now move ptr the next space, which should be 
		// right before the build date string.
	ptr = strchr(ptr,' ');
	if ( !ptr ) {
		myversion.MajorVer = 0;
		return false;
	}
	ptr++;	// skip space after the version numbers

		// Convert month to a number 
	const char *monthNames[] = { "Jan", "Feb", "Mar", "Apr", "May",
		"Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	int month = -1;
	for (int i=0; i<12; i++) {
		if (strncmp(monthNames[i],ptr,3) == 0) {
			month = i;
			break;
		}
	}

	ptr+= 4;	//skip month and space

		// Grab day of the month and year
	int date, year;
	date = year = -1;
	sscanf(ptr,"%d %d",&date,&year);

		// Sanity checks
	if ( month < 0 || month > 11 || date < 0 || date > 31 || year < 1997 
		|| year > 2036 ) {
		myversion.MajorVer = 0;
		return false;
	}

		// Make a struct tm
	struct tm build_date;
	build_date.tm_hour = 0;
	build_date.tm_isdst = 1;
	build_date.tm_mday = date;
	build_date.tm_min = 0;
	build_date.tm_mon = month;
	build_date.tm_sec = 0;
	build_date.tm_year = year - 1900;

	if ( (ver.BuildDate = mktime(&build_date)) == -1 ) {
		myversion.MajorVer = 0;
		return false;
	}

	return true;
}

							
bool
CondorVersionInfo::string_to_PlatformData(const char *platformstring, 
									 VersionData_t & ver)
{
	// platformstring looks like "$CondorPlatform: INTEL-LINUX_RH9 $"

	if ( !platformstring ) {
		// Use our own version number. 
		platformstring = CondorPlatform();

		// If we already computed myversion, we're done.
		if ( myversion.Arch ) {
			ver = myversion;
			return true;
		}
	}

	if ( strncmp(platformstring,"$CondorPlatform: ",17) != 0 ) {
		return false;
	}

	char *ptr = strchr(platformstring,' ');
	ptr++;		// skip space after the colon

	char *tempStr = strdup(ptr);	
	char *token; 
	token = strtok(tempStr, "-");
	if(token) ver.Arch = strdup(token);
		
	token = strtok(NULL, "-");
	if(token) ver.OpSys = strdup(token);

	if(ver.OpSys) {
		token = strchr(ver.OpSys, '$');
		if(token) *token = '\0';
	}		

	free(tempStr);

	return true;
}


