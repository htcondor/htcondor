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

#ifndef CONDOR_VER_INFO_H
#define CONDOR_VER_INFO_H

#include <string>

/** Class to interpret the Condor Version string. 
Every Condor binary contains a version string embedded into it at
compile time.  This class extracts that version string and can parse it.
Furthermore, this class can be used to determine if different components
of Condor are compatible with one another.	*/
class CondorVersionInfo
{
public:

	/** Constructor.  Pass in the version string and subsystem to parse.
		Typically these parameters are NULL, then the version string and 
		subsystem compiled into this binary are used.  Other common sources 
		for a versionstring could be from the get_version_from_file() 
		method (which is static, so it can be invoked before calling
		the constructor), or passed as part of a protocol, etc.
		@param versionstring See condor_version.C for format.
		@param subssytem One of SHADOW, STARTER, TOOL, SCHEDD, COLLECTOR, etc.
	*/
	CondorVersionInfo(const char *versionstring = NULL, 
		const char *subsystem = NULL, const char *platformstring = NULL);

	CondorVersionInfo(int major, int minor, int subminor, const char *rest = NULL,
		const char *subsystem = NULL, const char *platformstring = NULL);

	CondorVersionInfo(CondorVersionInfo const &);

	/// Destructor.
	~CondorVersionInfo();

	static char *get_version_from_file(const char* filename, 
							char *ver = NULL, int maxlen = 0);
	static char *get_platform_from_file(const char* filename, 
							char *platform = NULL, int maxlen = 0);

	/** Return the first number in the version ID.
		@return -1 on error */
	int getMajorVer() const
		{ return myversion.MajorVer > 5 ? myversion.MajorVer : -1; }
	/** Return the second number in the version ID (the series id).
		@return -1 on error */
	int getMinorVer() const
		{ return myversion.MajorVer > 5 ? myversion.MinorVer : -1; }
	/** Return the third number in the version ID (release id within the series).
		@return -1 on error */
	int getSubMinorVer() const
		{ return myversion.MajorVer > 5 ? myversion.SubMinorVer : -1; }

 
	/** Return the Arch this version is built for. */
	const char *getArchVer() const
		{ return myversion.Arch.c_str(); }
	const char *getOpSysVer() const
		{ return myversion.OpSys.c_str(); }
	
	/** Report if this version id represents a stable or developer series 
		release.
		@return true if a stable series, otherwise false. */
	bool is_stable_series() const
		{ return (myversion.MinorVer % 2 == 0);  }


	int compare_versions(const char* other_version_string) const;
	int compare_versions(const CondorVersionInfo & other_version) const;
	
	bool built_since_version(int MajorVer, int MinorVer, int SubMinorVer) const;

	bool is_compatible(const char* other_version_string, 
					   const char* other_subsys = NULL) const;

	bool is_valid(const char* VersionString = NULL) const;

		// Constructs version string from version info.
		// Caller should free returned string when done with it.
		// Returns NULL on error.
	char *get_version_string() const;

	std::string get_version_stdstring() const;

	typedef struct VersionData {
		int MajorVer;
		int MinorVer;
		int SubMinorVer;
		int Scalar;
		std::string Rest;
		std::string Arch;
		std::string OpSys;
	} VersionData_t;


private:
	
	VersionData_t myversion;
	char *mysubsys;

	bool numbers_to_VersionData( int major, int minor, int subminor,
								 const char *rest, VersionData_t &ver ) const;
	bool string_to_VersionData(const char *,VersionData_t &) const;
	bool string_to_PlatformData(const char *,VersionData_t &) const;
};



#endif
