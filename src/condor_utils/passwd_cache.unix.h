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


#ifndef __PASSWD_CACHE_H
#define __PASSWD_CACHE_H

#include <grp.h>
#include <string>
#include <map>
#include <vector>

struct group_entry {
	std::vector<gid_t> gidlist;	/* groups this user is a member of */
	time_t 	lastupdated;	/* timestamp of when this entry was updated */
};

struct uid_entry {
	uid_t 	uid;			/* user's uid */
	gid_t	gid;			/* user's primary gid */
	time_t 	lastupdated;	/* timestamp of when this entry was updated */
};

/*
Don't declare your own instances of this.  Instead see
the pcache() function declared in condor_includes/condor_uid.h.
*/
class passwd_cache {

	public:

		passwd_cache();
		~passwd_cache();

		/* clear all contents of cache and start over */
		void reset();

		/* Inserts given user's uid into uid table.
		 * returns true on success. */
		bool cache_uid(const char* user);

		/* Inserts given user's group list into the group table.
		 * returns true on success. */
		bool cache_groups(const char* user);

		/* returns the number of supplementary groups a user has.
		 * We usually use this function to figure out how large
		 * of a gid_t[] we need to pass to get_groups(). Returns
		 * -1 if the user isn't found. */
		int num_groups(const char* user); 

		/* Gets a list of gid's for the given uid.
		 * Returns true on success. */
		bool get_groups(const char* user, size_t groupsize, gid_t list[]); 

		/// Gets the uid of the given user.
		bool get_user_uid(const char* user, uid_t &uid);

		/// Gets the gid of the given user.
		bool get_user_gid(const char* user, gid_t &gid);

		/// Gets the uid and gid of the given user.
		bool get_user_ids(const char* user, uid_t &uid, gid_t &gid);

		/* gets the username from the uid. 
		 * Allocates a new string that must be free'd.
		 * Returns true on success. */
		bool get_user_name(const uid_t uid, char *&user);

		/* initializes group access list for given user. 
		 * The additional group is added to the list if given.
		 * returns true on success. */
		bool init_groups(const char* user, gid_t additional_gid = 0);
		
		/* for testing purposes only. 
		 * returns the age of a given cache entry.
		 * returns -1 on failure. */
		int get_uid_entry_age(const char *user);
		int get_group_entry_age(const char *user);

		/* also for testing.
		 * Returns maximum lifetime of a cache entry, in seconds.
		 */
		time_t get_entry_lifetime() const { return Entry_lifetime; }

		// builds a string in the format expected for the
		// configuration variable USERID_MAP
		void getUseridMap(std::string &usermap);

	private:

		void loadConfig();

		/* stashes the user's uid in our cache, using information
		   from the supplied passwd struct. Returns true
		   on success. */
		bool cache_uid(const struct passwd *pwent);

		/* retrieves cache entry for the given user name.
		 * returns true on success. */
		bool lookup_uid(const char* user, uid_entry *&uce);
		bool lookup_group(const char* user, group_entry *&gce);

		/* helper for get_user_* methods that handles shared code */
		bool lookup_uid_entry( const char* user, uid_entry *&uce );

		/* number of seconds a cache entry will be considered valid */
		time_t Entry_lifetime;

		/* hash tables of for cached uids and groups */
		std::map<std::string, uid_entry> uid_table;
		std::map<std::string, group_entry> group_table;
};

#endif /* __PASSWD_CACHE_H */
