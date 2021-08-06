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
#include "condor_config.h"
#include "subsystem_info.h"
#include "nullfile.h"
#include "basename.h"
#include "condor_getcwd.h"
#include "directory_util.h"
#include "limit_directory_access.h"


bool allow_shadow_access(const char *path, bool init, const char *job_ad_whitelist, const char *spool_dir)
{
	bool allow = true;

	// Always allow access to /dev/null 
	if (path && nullFile(path)) {
		return true;
	}

	if (get_mySubSystem()->isType(SUBSYSTEM_TYPE_SHADOW)) {
		static StringList allow_path_prefix_list;
		static bool path_prefix_initialized = false;

		if (init == false && path_prefix_initialized == false) {
			EXCEPT("allow_shadow_access() invoked before intialized");
		}

		if ((init == false) && (job_ad_whitelist || spool_dir)) {
			EXCEPT("allow_shadow_access() invoked with init=false and job_ad_whitelist!=NULL");
		}


		if (init) {
			allow_path_prefix_list.clearAll();

			// If LIMIT_DIRECTORY_ACCESS is defined in the config file by the admin,
			// then honor it.  Only if it is empty to we then allow the user to 
			// specify the list in their job.
			StringList wlist;
			char *whitelist = param("LIMIT_DIRECTORY_ACCESS");
			if (whitelist) {
				wlist.initializeFromString(whitelist, ',');
				free(whitelist);
			} 			
			if (wlist.isEmpty() && job_ad_whitelist && job_ad_whitelist[0]) {
				wlist.initializeFromString(job_ad_whitelist, ',');
			}

			// If there are any limits, then add the job's SPOOL directory to the list
			if (!wlist.isEmpty() && spool_dir) {
				wlist.append(spool_dir);
				std::string tmpSpool(spool_dir);
				// Also add spool_dir with ".tmp", since FileTransfer will also use that.
				tmpSpool += ".tmp";
				wlist.append(tmpSpool.c_str());
			}

			// Now go through all the directories and attempt to cannonicalize them 
			const char *st;
			wlist.rewind();
			while ((st = wlist.next())) {
				char *strp = NULL;
				std::string item;
				if ((strp = realpath(st, NULL))) {
					item = strp;
					free(strp);
				}
				else {
					item = st;
				}
				if (item.empty()) continue;
				if (!IS_ANY_DIR_DELIM_CHAR(item.back()) && item.back()!='*') {
					item += DIR_DELIM_CHAR;
				}
				allow_path_prefix_list.append(item.c_str());
			}

			whitelist = allow_path_prefix_list.print_to_string();
			if (whitelist == NULL) {
				whitelist = strdup("<unset>");
			}
			dprintf(D_ALWAYS, "LIMIT_DIRECTORY_ACCESS = %s\n", whitelist);
			free(whitelist);

			path_prefix_initialized = true;
		}

		// If we are restricting pathnames the shadow can access, check now
		if (path && !allow_path_prefix_list.isEmpty()) {
			char *rpath = NULL;
			MyString full_pathname;

			// Make path fully qualified if it is relative to the cwd
			if (!fullpath(path)) {
				if (condor_getcwd(full_pathname)) {
					std::string result;
					full_pathname = dircat(full_pathname.c_str(), path, result);
					path = full_pathname.c_str();
				}
				else {
					allow = false;
					dprintf(D_ALWAYS, 
						"Access DENIED to file %s due to getcwd failure processing LIMIT_DIRECTORY_ACCESS\n", path);
				}
			}

			// Make our fully qualified path canonical via realpath(), to get rid of
			// dot-dots (e.g. /foo/../bar/xxx) and symlinks.
			if (allow) {
				rpath = realpath(path, nullptr);
				if (!rpath) {
					char *d = condor_dirname(path);
					rpath = realpath(d, nullptr);
					free(d);
					if (!rpath) {
						allow = false;
						dprintf(D_ALWAYS,
							"Access DENIED to file %s due to realpath failure processing LIMIT_DIRECTORY_ACCESS\n", path);
					}
				}
			}

			// Finally, see if the rpath of the file we are trying to access is contained
			// within any of the directories listed in allow_path_prefix_list.
			if (allow) {
#ifdef WIN32
				// On Win32 paths names are case insensitive
				allow = allow_path_prefix_list.prefix_anycase_withwildcard(rpath);
#else
				// On everything other than Win32, path names are case sensitive
				allow = allow_path_prefix_list.prefix_withwildcard(rpath);
#endif
			}
			free(rpath);
		}

	} // end of subsystem is SHADOW

	if (allow == false && path) {
		dprintf(D_ALWAYS, "Access DENIED to file %s due to LIMIT_DIRECTORY_ACCESS\n", path);
	}

	return allow;
}

