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
#ifndef CONDOR_VER_INFO_H
#define CONDOR_VER_INFO_H


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
	CondorVersionInfo::CondorVersionInfo(const char *versionstring = NULL, 
		char *subsystem = NULL, char *platformstring = NULL);

	/// Destructor.
	CondorVersionInfo::~CondorVersionInfo();

	static char *get_version_from_file(const char* filename, 
							char *ver = NULL, int maxlen = 0);
	static char *get_platform_from_file(const char* filename, 
							char *platform = NULL, int maxlen = 0);

	/** Return the first number in the version ID.
		@return -1 on error */
	int getMajorVer() 
		{ return myversion.MajorVer > 5 ? myversion.MajorVer : -1; }
	/** Return the second number in the version ID (the series id).
		@return -1 on error */
	int getMinorVer() 
		{ return myversion.MajorVer > 5 ? myversion.MinorVer : -1; }
	/** Return the third number in the version ID (release id within the series).
		@return -1 on error */
	int getSubMinorVer() 
		{ return myversion.MajorVer > 5 ? myversion.SubMinorVer : -1; }

 
 	/** Return the Arch this version is built for. */
 	char *getArchVer() 
 		{ return myversion.Arch; }	
 	char *getOpSysVer() 
 		{ return myversion.OpSys; }	
 	char *getLibcVer()
 		{ return myversion.Libc; }	

	
	/** Report if this version id represents a stable or developer series 
		release.
		@return true if a stable series, otherwise false. */
	bool is_stable_series() 
		{ return (myversion.MinorVer % 2 == 0);  }


	int compare_versions(const char* other_version_string);
	int compare_build_dates(const char* other_version_string);
	
	bool built_since_version(int MajorVer, int MinorVer, int SubMinorVer);

	// Note: for "month", 1=Jan, 2=Feb, etc, as you'd expect
	bool built_since_date(int month, int day, int year);

	bool is_compatible(const char* other_version_string, 
								 const char* other_subsys = NULL);

	bool is_valid(const char* VersionString = NULL);

	typedef struct VersionData {
		int MajorVer;
		int MinorVer;
		int SubMinorVer;
		int Scalar;
		time_t BuildDate;
		char *Arch;
		char *OpSys;
		char *Libc;
	} VersionData_t;


private:
	
	VersionData_t myversion;
	char *mysubsys;

	bool string_to_VersionData(const char *,VersionData_t &);
	bool string_to_PlatformData(const char *,VersionData_t &);
};



#endif
