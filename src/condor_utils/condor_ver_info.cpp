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


#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_ver_info.h"
#include "condor_version.h"
#include "subsystem_info.h"
#include "condor_debug.h"
#include "filename_tools.h"
#include "stl_string_utils.h"

CondorVersionInfo::CondorVersionInfo(const char *versionstring, 
									 const char *subsystem,
									 const char *platformstring)
{
	myversion.MajorVer = 0;
	mysubsys = NULL;

	if ( ! versionstring || ! *versionstring ) {
		versionstring = CondorVersion();
	}
	if ( ! platformstring || ! *platformstring ) {
		platformstring = CondorPlatform();
	}

	string_to_VersionData(versionstring,myversion);
	string_to_PlatformData(platformstring,myversion);

	if ( subsystem && *subsystem ) {
		mysubsys = strdup(subsystem);
	} else {
		mysubsys = strdup(get_mySubSystem()->getName());
	}
}

CondorVersionInfo::CondorVersionInfo(int major, int minor, int subminor,
									 const char *rest,
									 const char *subsystem,
									 const char *platformstring)
{
	myversion.MajorVer = 0;
	mysubsys = NULL;

	if ( ! platformstring || ! *platformstring ) {
		platformstring = CondorPlatform();
	}

	numbers_to_VersionData( major, minor, subminor, rest, myversion );
	string_to_PlatformData( platformstring, myversion );

	if ( subsystem && *subsystem ) {
		mysubsys = strdup( subsystem );
	} else {
		mysubsys = strdup( get_mySubSystem()->getName() );
	}
}

CondorVersionInfo::CondorVersionInfo(CondorVersionInfo const &other)
{
	myversion = other.myversion;
	mysubsys = NULL;
	if( other.mysubsys ) {
		mysubsys = strdup(other.mysubsys);
	}
	myversion.Rest = other.myversion.Rest;
	myversion.Arch = other.myversion.Arch;
	myversion.OpSys = other.myversion.OpSys;
}

CondorVersionInfo::~CondorVersionInfo()
{
	if (mysubsys) free(mysubsys);
}


int
CondorVersionInfo::compare_versions(const CondorVersionInfo & other_version) const
{
	if ( other_version.myversion.Scalar < myversion.Scalar ) {
		return -1;
	}
	if ( other_version.myversion.Scalar > myversion.Scalar ) {
		return 1;
	}
	return 0;
}

int
CondorVersionInfo::compare_versions(const char* VersionString1) const
{
	VersionData_t ver1;

	ver1.Scalar = 0;
	string_to_VersionData(VersionString1,ver1);

	if ( ver1.Scalar < myversion.Scalar ) {
		return -1;
	}
	if ( ver1.Scalar > myversion.Scalar ) {
		return 1;
	}

	return 0;
}


bool
CondorVersionInfo::is_compatible(const char* other_version_string, 
								 const char*  /*other_subsys*/) const
{
	VersionData_t other_ver;

	if ( ! string_to_VersionData(other_version_string,other_ver) ) {
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
									   int SubMinorVer) const
{
	int Scalar = MajorVer * 1'000'000 + MinorVer * 1'000 
					+ SubMinorVer;

	return ( myversion.Scalar >= Scalar );
}


bool
CondorVersionInfo::is_valid(const char* VersionString) const
{
	bool ret_value;
	VersionData_t ver1;

	if ( !VersionString || ! *VersionString ) {
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

	FILE *fp = safe_fopen_wrapper_follow(filename,readonly);

	if (!fp) {
		// file not found, try alternate exec pathname
		char *altname = alternate_exec_pathname( filename );
		if ( altname ) {
			fp = safe_fopen_wrapper_follow(altname,readonly);
			free(altname);
		}
	}

	if ( !fp ) {
		// file not found
		return NULL;
	}
		
	if (!ver) {
		if ( !(ver = (char *)malloc(100)) ) {
			// out of memory
			fclose(fp);
			return NULL;
		}
		must_free = true;
		maxlen = 100;
	}

		// Look for the magic version string
		// '$CondorVersion: x.y.z <date> <extra info> $' in the file.
		// What we look for is a string that begins with '$CondorVersion: '
		// and continues with a non-NULL character. We need to be careful
		// not to match the string '$CondorVersion: \0' which this file
		// includes as static data in a Condor executable.
	int i = 0;
	bool got_verstring = false;
	const char* verprefix = "$CondorVersion: ";
	int ch;
	while( (ch=fgetc(fp)) != EOF ) {
		if ( verprefix[i] == '\0' && ch != '\0' ) {
			do {
				ver[i++] = ch;
				if ( ch == '$' ) {
					got_verstring = true;
					ver[i] = '\0';
					break;
				}
			} while ( (i < maxlen) && ((ch=fgetc(fp)) != EOF) );
			break;
		}

		if ( ch != verprefix[i] ) {
			i = 0;
			if ( ch != verprefix[0] ) {
				continue;
			}
		}

		ver[i++] = ch;
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

	FILE *fp = safe_fopen_wrapper_follow(filename,readonly);

	if (!fp) {
		// file not found, try alternate exec pathname
		char *altname = alternate_exec_pathname( filename );
		if ( altname ) {
			fp = safe_fopen_wrapper_follow(altname,readonly);
			free(altname);
		}
	}

	if ( !fp ) {
		// file not found
		return NULL;
	}
		
	if (!platform) {
		if ( !(platform = (char *)malloc(100)) ) {
			// out of memory
			fclose(fp);
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

char *
CondorVersionInfo::get_version_string() const
{
	return strdup(get_version_stdstring().c_str());
}

std::string
CondorVersionInfo::get_version_stdstring() const {
	std::string result;
	formatstr(result,"$%s: %d.%d.%d %s $", 
					 ATTR_CONDOR_VERSION, // avoid having false "$CondorVersion: ..." show up in strings
					 myversion.MajorVer, myversion.MinorVer, myversion.SubMinorVer,
					 myversion.Rest.c_str());
	return result;
}

bool
CondorVersionInfo::numbers_to_VersionData( int major, int minor, int subminor,
										   const char *rest, VersionData_t & ver ) const
{
	ver.MajorVer = major;
	ver.MinorVer = minor;
	ver.SubMinorVer = subminor;

		// Sanity check: the world starts with Condor V6 !
	if ( ver.MajorVer < 6  || ver.MinorVer > 99 || ver.SubMinorVer > 99 ) {
		ver.MajorVer = 0;
		return false;
	}

	ver.Scalar = ver.MajorVer * 1'000'000 + ver.MinorVer * 1'000 
					+ ver.SubMinorVer;

	if ( rest ) {
		ver.Rest = rest;
	} else {
		ver.Rest = "";
	}

	return true;
}

bool
CondorVersionInfo::string_to_VersionData(const char *verstring, 
										 VersionData_t & ver) const
{
	// verstring looks like "$CondorVersion: 6.1.10 Nov 23 1999 $"

	if ( !verstring || !*verstring ) {
		// Use our own version number. 
		ver = myversion;
		return true;
	}

	if ( strncmp(verstring,"$CondorVersion: ",16) != 0 ) {
		return false;
	}

	char const *ptr = strchr(verstring,' ');
	if (ptr == NULL) {
		// must not be a version string at all
		ver.MajorVer = 0;
		return false;
	}

	ptr++;		// skip space after the colon

	int cfld = sscanf(ptr,"%d.%d.%d ",&ver.MajorVer,&ver.MinorVer,&ver.SubMinorVer);

		// Sanity check: the world starts with Condor V6 !
	if (cfld != 3 || (ver.MajorVer < 6  || ver.MinorVer > 99 || ver.SubMinorVer > 99)) {
		ver.MajorVer = 0;
		return false;
	}

	ver.Scalar = ver.MajorVer * 1'000'000 + ver.MinorVer * 1'000 
					+ ver.SubMinorVer;

		// Now move ptr the next space, which should be 
		// right before the build date string.
	ptr = strchr(ptr,' ');
	if ( !ptr ) {
		ver.MajorVer = 0;
		return false;
	}
	ptr++;	// skip space after the version numbers

		// There is a date and other things at the end of the string,
		// but we're not using them anymore, but others may be so we
		// hold on to them.  See CondorVersion() for complete format.
	ver.Rest = ptr;
		// Strip the trailing " $"
	ver.Rest.erase( ver.Rest.find( " $" ) );

	return true;
}

							
bool
CondorVersionInfo::string_to_PlatformData(const char *platformstring, 
										  VersionData_t & ver) const
{
	// platformstring looks like "$CondorPlatform: INTEL-LINUX_RH9 $"

	if ( !platformstring || ! *platformstring ) {
		// Use our own version number. 
		ver = myversion;
		return true;
	}

	if ( strncmp(platformstring,"$CondorPlatform: ",17) != 0 ) {
		return false;
	}

	char const *ptr = strchr(platformstring,' ');

	// No space mean ill-formed string, punt to our own version number
	if (!ptr) {
		ver = myversion;
		return true;
	}

	ptr++;		// skip space after the colon

	size_t len = strcspn(ptr,"-");
	if( len ) {
		ver.Arch = ptr;
		ver.Arch.erase( len );
		ptr += len;
	}

	if( *ptr == '-' ) {
		ptr++;
	}

	len = strcspn(ptr," $");
	if( len ) {
		ver.OpSys = ptr;
		ver.OpSys.erase( len );
		ptr += len;
	}

	return true;
}


