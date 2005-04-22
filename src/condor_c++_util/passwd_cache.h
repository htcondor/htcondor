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

#ifndef __PASSWD_CACHE_H
#define __PASSWD_CACHE_H

#include <grp.h>
#include "MyString.h"
#include "HashTable.h"


typedef struct group_entry {
	gid_t 	*gidlist;		/* groups this user is a member of */
	size_t 	gidlist_sz;		/* size of group list */
	time_t 	lastupdated;	/* timestamp of when this entry was updated */
} group_entry;

typedef struct uid_entry {
	uid_t 	uid;			/* user's uid */
	gid_t	gid;			/* user's primary gid */
	time_t 	lastupdated;	/* timestamp of when this entry was updated */
} uid_entry;

/* define our hash function */
int compute_user_hash(const MyString &key, int numBuckets);

typedef HashTable <MyString, uid_entry*> UidHashTable;
typedef HashTable <MyString, group_entry*> GroupHashTable;

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
		 * The additional group 'group' is added to the list.
		 * returns true on success. */
		bool init_groups(const char* user);
		
		/* for testing purposes only. 
		 * returns the age of a given cache entry.
		 * returns -1 on failure. */
		int get_uid_entry_age(const char *user);
		int get_group_entry_age(const char *user);

		/* also for testing.
		 * Returns maximum lifetime of a cache entry, in seconds.
		 */
		time_t get_entry_lifetime() { return Entry_lifetime; }


	private:

		/* stashes the user's uid in our cache, using information
		   from the supplied passwd struct. Returns true
		   on success. */
		bool cache_uid(const struct passwd *pwent);

		/* allocates and zeros out a cache entry */
		void init_uid_entry(uid_entry *&uce);
		void init_group_entry(group_entry *&gce);

		/* retrieves cache entry for the given user name.
		 * returns true on success. */
		bool lookup_uid(const char* user, uid_entry *&uce);
		bool lookup_group(const char* user, group_entry *&gce);

		/* helper for get_user_* methods that handles shared code */
		bool lookup_uid_entry( const char* user, uid_entry *&uce );

		/* maximum number of groups allowed for a 
		 * give user */
		long groups_max;

		/* number of seconds a cache entry will be considered valid */
		time_t Entry_lifetime;

		/* hash tables of for cached uids and groups */
		UidHashTable *uid_table;
		GroupHashTable *group_table;
};

#endif /* __PASSWD_CACHE_H */
