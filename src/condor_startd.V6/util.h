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

#ifndef _UTIL_H
#define _UTIL_H

#include "condor_common.h"
#include "condor_classad.h"

class Stream;
class Resource;

// Our utilities 
void	cleanup_execute_dir(int pid, char const *exec_path, bool remove_exec_path, bool abnormal_exit);
void	cleanup_execute_dirs(const std::vector<std::string> &list);
void	check_execute_dir_perms(const std::vector<std::string> &list);
void	check_recovery_file( const char *sandbox_dir, bool abnormal_exit );

bool	reply( Stream*, int );
bool	refuse( Stream* );
bool	caInsert( ClassAd* target, ClassAd* source, const char* attr,
				  const char* prefix = NULL );
bool	caRevertToParent(ClassAd* target, const char * attr);
// delete in chained ad, and also in parent ad (ClassAd::Delete does not delete in parent)
void	caDeleteThruParent(ClassAd* target, const char * attr, const char * prefix = NULL);
bool	configInsert( ClassAd* ad, const char* attr, bool is_fatal );
bool	configInsert( ClassAd* ad, const char* param_name, 
					  const char* attr, bool is_fatal );
Resource* stream_to_rip( Stream* );

VacateType getVacateType( ClassAd* ad );

// This class holds the names of things that still need to be cleaned up because we failed
// to clean the the first time around. This happens mostly on Windows with execute directories
// because of antivirus software holding the directory or some files within it open.
// we will put instances of this class into a map and leave them there until we are able
// to finally clean them up.
//
class CleanupReminder {
public:
	enum category { exec_dir=0, account };
	std::string name; // name of resource to cleanup
	category cat;  // category of resource, e.g execute dir, account name, etc.
	int      opt; // options, meaning depends on category

	// so that some of the reminders can be case sensitive, and some not.
	// in the future, we may consider cat as well as OS in this function, so prepare for that now.
	bool case_sensitive(category /*cat*/) const {
#ifdef WIN32
		return false;
#else
		return true;
#endif
	}

	~CleanupReminder() {}
	//CleanupReminder(const char * n=NULL, category c=exec_dir, int o=0) : name(n), cat(c), opt(o) {}
	CleanupReminder(const std::string &n, category c, int o=0) : name(n), cat(c), opt(o) {}
	CleanupReminder(const CleanupReminder & cr) : name(cr.name), cat(cr.cat), opt(cr.opt) {}
	void operator =(const CleanupReminder & cr) { name = cr.name; cat = cr.cat; opt = cr.opt; }

	// the identity of the resource does not depend on the options flags
	// so we ignore options in == and < operators
	bool operator ==(const CleanupReminder &rhs) const {
		if (cat != rhs.cat) return false;
		// comparison of reminders can be case sensitive or case insensitive.
		if (case_sensitive(cat)) {
			return YourString(name) == rhs.name;
		} else {
			return YourStringNoCase(name) == rhs.name;
		}
	};
	bool operator <(const CleanupReminder &rhs) const {
		if (cat != rhs.cat) {
			return cat < rhs.cat;
		}
		if (case_sensitive(cat)) {
			return YourString(name) < rhs.name;
		} else {
			return YourStringNoCase(name) < rhs.name;
		}
	}

};

// map the cleanup reminder to a number of iterations of the cleanup loop.
// this is NOT the retry count since we use this number to calculate our exponential backoff and don't retry
// on ever iteration.
typedef std::map<CleanupReminder, int> CleanupReminderMap;

void add_exec_dir_cleanup_reminder(const std::string & dir, int options);
void add_account_cleanup_reminder(const std::string& name);

bool retry_cleanup_execute_dir(const std::string & path, int options, int &err);
bool retry_cleanup_user_account(const std::string & path, int options, int &err);

#endif /* _UTIL_H */
