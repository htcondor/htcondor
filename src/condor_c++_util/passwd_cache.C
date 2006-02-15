/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

/* implements a simple cache. Also handles refreshes to keep cache
elements from getting stale. 

IMPORTANT NOTE: Don't dprintf() in here, unless its a fatal error! */

#include "condor_common.h"
#include "passwd_cache.h"
#include "condor_config.h"

int compute_user_hash(const MyString &key, int numBuckets) {
    return ( key.Hash() % numBuckets );
};

passwd_cache::passwd_cache() {

	uid_table = new UidHashTable(10, compute_user_hash, updateDuplicateKeys);
	group_table = new 
		GroupHashTable(10, compute_user_hash, updateDuplicateKeys);
		/* set the number of seconds until a cache entry expires */
	Entry_lifetime = param_integer("PASSWD_CACHE_REFRESH", 300);
}
void
passwd_cache::reset() {

	group_entry *gent;
	uid_entry *uent;
	MyString index;

	group_table->startIterations();
	while ( group_table->iterate(index, gent) ) {
		delete[] gent->gidlist;
		delete gent;
		group_table->remove(index);
	}

	uid_table->startIterations();
	while ( uid_table->iterate(index, uent) ) {
		delete uent;
		uid_table->remove(index);
	}
}

passwd_cache::~passwd_cache() {

	reset();
	delete group_table;
	delete uid_table;
}

/* uses initgroups() and getgroups() to get the supplementary
   group info, then stashes it in our cache. We must be root when calling
 this function, since it calls initgroups(). */
bool passwd_cache::cache_groups(const char* user) {

	bool result;
	group_entry *group_cache_entry;
	gid_t user_gid;
	int count;
   
	group_cache_entry = NULL;
	count = 0;
	result = true;

	if ( user == NULL ) {
		return false;
	} else { 

		if ( !get_user_gid(user, user_gid) ) {
			dprintf(D_ALWAYS, "cache_groups(): get_user_gid() failed! "
				   "errno=%s\n", strerror(errno));
			return false;
		}

		if ( group_table->lookup(user, group_cache_entry) < 0 ) {
			init_group_entry(group_cache_entry);
		}

		/* We need to get the primary and supplementary group info, so
		 * we're going to call initgroups() first, then call get groups
		 * so we can cache whatever we get.*/

		count = 0;
		if ( initgroups(user, user_gid) != 0 ) {
			dprintf(D_ALWAYS, "passwd_cache: initgroups() failed! errno=%s\n",
					strerror(errno));
			return false;
		}

		/* get the number of groups from the OS first */
		group_cache_entry->gidlist_sz = ::getgroups(0, NULL);

		if ( group_cache_entry->gidlist_sz < 0 ) {
			result = false;
		} else {
			/* now get the group list */
			if ( group_cache_entry->gidlist != NULL ) {
				delete[] group_cache_entry->gidlist;
				group_cache_entry->gidlist = NULL;
			}
	   		group_cache_entry->gidlist = new
			  		 	gid_t[group_cache_entry->gidlist_sz];
			if (::getgroups( 	group_cache_entry->gidlist_sz,
					 		group_cache_entry->gidlist) < 0) {
				dprintf(D_ALWAYS, "cache_groups(): getgroups() failed! "
						"errno=%s\n", strerror(errno));
				result = false;
			} else {
				/* finally, insert info into our cache */
				group_cache_entry->lastupdated = time(NULL);
				group_table->insert(user, group_cache_entry);

			}
		}
		return result;
	}

}

/* this is the public interface to cache a user's uid.
 * give it a username, and it stashes its uid in our cache.*/
bool
passwd_cache::cache_uid(const char* user) {

	struct passwd *pwent;
	char *err_string;

	pwent = getpwnam(user);
	if ( pwent == NULL ) {
			// unix is lame: getpwnam() sets errno to ENOENT ("No such
			// file or directory") when it means "user not found"
		if( errno == ENOENT ) {
			static char *errno_clarification = "user not found";
			err_string = errno_clarification;
		} else {
			err_string = strerror( errno );
		}
		dprintf( D_ALWAYS, "passwd_cache::cache_uid(): getpwnam(\"%s\") "
				 "failed: %s\n", user, err_string );
		return false;
	} else {
	   	return cache_uid(pwent);
	}
}

/* uses standard system functions to get user's uid, 
 * then stashes it in our cache. This function is kept private
 * since we don't want to expose the ability to supply your own 
 * passwd struct to the end user. Internally, we can supply our
 * own passwd struct if we've already looked it up for some other
 * reason. */
bool
passwd_cache::cache_uid(const struct passwd *pwent) {
	
	uid_entry *cache_entry;
	MyString index;

   
	if ( pwent == NULL ) {
			/* a little sanity check */
		return false;
	} else {

		index = pwent->pw_name;

		if ( uid_table->lookup(index.Value(), cache_entry) < 0 ) {
				/* if we don't already have this entry, create a new one */
			init_uid_entry(cache_entry);
		}

	   	cache_entry->uid = pwent->pw_uid;
	   	cache_entry->gid = pwent->pw_gid;
			/* reset lastupdated */
		cache_entry->lastupdated = time(NULL);
		uid_table->insert(index, cache_entry);
		return true;
	}
}

/* gives us the number of groups a user is a member of */
int
passwd_cache::num_groups(const char* user) {

	group_entry *cache_entry;

	if ( !lookup_group( user, cache_entry) ) {
			/* CACHE MISS */

			/* the user isn't cached, so load it in first */
		if ( cache_groups(user) ) {
				/* if cache user succeeded, this should always succeed */
			lookup_group(user, cache_entry);
		} else {
			dprintf(D_ALWAYS, "Failed to cache info for user %s\n",
				   	user);
			return -1;
		}
	} else {
		/* CACHE HIT */
	}
	return cache_entry->gidlist_sz;
}

/* retrieves user's groups from cache */
bool
passwd_cache::get_groups( const char *user, size_t groupsize, gid_t gid_list[] ) {

    unsigned int i;
	group_entry *cache_entry;


		/* , check the cache for an existing entry */
	if ( !lookup_group( user, cache_entry) ) {
			/* CACHE MISS */

			/* the user isn't cached, so load it in first */
		if ( cache_groups(user) ) {
				/* if cache user succeeded, this should always succeed */
			lookup_group(user, cache_entry);
		} else {
			dprintf(D_ALWAYS, "Failed to cache info for user %s\n",
				   	user);
			return false;
		}
	} else {
			/* CACHE HIT */
	}

	if ( cache_entry->gidlist_sz > groupsize ) {
		dprintf(D_ALWAYS, "Inadequate size for gid list!\n");
		return false;
	}

		/* note that if groupsize is 0, only the size is returned. */
	for (i=0; (i<groupsize && i<cache_entry->gidlist_sz); i++) {
		gid_list[i] = cache_entry->gidlist[i];
	}
	return true;
}


bool
passwd_cache::get_user_uid( const char* user, uid_t &uid )
{
	uid_entry *cache_entry;
	if( ! lookup_uid_entry(user, cache_entry) ) {
		return false;
	}
	uid = cache_entry->uid;
	return true;
}


bool
passwd_cache::get_user_gid( const char* user, gid_t &gid )
{
	uid_entry *cache_entry;
	if( ! lookup_uid_entry(user, cache_entry) ) {
		return false;
	}
	gid = cache_entry->gid;
	return true;
}


bool
passwd_cache::get_user_ids( const char* user, uid_t &uid, gid_t &gid )
{
	uid_entry *cache_entry;
	if( ! lookup_uid_entry(user, cache_entry) ) {
		return false;
	}
	uid = cache_entry->uid;
	gid = cache_entry->gid;
	return true;
}


bool
passwd_cache::lookup_uid_entry( const char* user, uid_entry *&uce )
{
	if( !lookup_uid( user, uce) ) {
			/* CACHE MISS */
		if( cache_uid(user) ) {
			if( !lookup_uid(user, uce) ) {
				dprintf( D_ALWAYS, "Failed to cache user info for user %s\n", 
						 user );
				return false;
			}
		} else {
				// cache_user() failure. Not much we can do there.
			return false;
		}
	} 
	return true;
}


bool
passwd_cache::get_user_name(const uid_t uid, char *&user) {

	uid_entry *ent;
	struct passwd *pwd;
	MyString index;

	uid_table->startIterations();
	while ( uid_table->iterate(index, ent) ) {
		if ( ent->uid == uid ) {
			user = strdup(index.Value());
			return true;
		}
	}
	
	/* no cached entry, so we need to look up 
	 * the entry and cache it */

	pwd=getpwuid(uid);
	if ( pwd ) {

		/* get the user in the cache */
		cache_uid(pwd);

		user = strdup(pwd->pw_name);
		return true;
	} else {

		user = NULL;
		/* can't find a user with that uid, so fail. */
		return false;
	}
}

bool
passwd_cache::init_groups( const char* user ) {

	gid_t *gid_list;
	bool result;
	int siz;

	siz = num_groups(user);
	result = true;
	gid_list = NULL;

	if ( siz > 0 ) {

		gid_list = new gid_t[siz];

		if ( get_groups(user, siz, gid_list) ) { 

			if ( setgroups(siz, gid_list) != 0 ) {
				dprintf(D_ALWAYS, "passwd_cache: setgroups( %s ) failed.\n", user);
				result = false;
			} else {
					/* success */
				result = true;
			}
		} else {
			dprintf(D_ALWAYS, "passwd_cache: getgroups( %s ) failed.\n", user);
			result = false;
		}


	} else {
			/* error */
		dprintf(D_ALWAYS, "passwd_cache: getgroups( %s ) failed.\n", user);
		result = false;
	}

	if ( gid_list ) { delete[] gid_list; }
	return result;
}

/* wrapper function around hashtable->lookup() that also decides 
 * when to refresh the requested entry */
bool
passwd_cache::lookup_uid(const char *user, uid_entry *&uce) {

	if ( uid_table->lookup(user, uce) < 0 ) {
		/* cache miss */
		return false;
	} else {
		if ( (time(NULL) - uce->lastupdated) > Entry_lifetime ) {
			/* time to refresh the entry! */
			cache_uid(user);
			return (uid_table->lookup(user, uce) == 0);
		} else {
			/* entry is still considered valid, so just return */
			return true;
		}
	}
}

/* wrapper function around hashtable->lookup() that also decides 
 * when to refresh the requested entry */

bool
passwd_cache::lookup_group(const char *user, group_entry *&gce) {

	if ( group_table->lookup(user, gce) < 0 ) {
			/* cache miss */
		return false;
	} else {
		if ( (time(NULL) - gce->lastupdated) > Entry_lifetime ) {
				/* time to refresh the entry! */
			cache_groups(user);
			return (group_table->lookup(user, gce) == 0);
		} else {
			/* entry is still considered valid, so just return */
			return true;
		}
	}
}

/* For testing purposes only */
int
passwd_cache::get_uid_entry_age(const char *user) {

	uid_entry *uce;

	if ( !lookup_uid(user, uce) ) {
		return -1;
	} else {
		return (time(NULL) - uce->lastupdated);
	}
}

/* For testing purposes only */
int
passwd_cache::get_group_entry_age(const char *user) {

	group_entry *gce;

	if ( !lookup_group(user, gce) ) {
		return -1;
	} else {
		return (time(NULL) - gce->lastupdated);
	}
}

/* allocates new cache entry and zeros out all the fields */
void
passwd_cache::init_uid_entry(uid_entry *&uce) {
		
	uce = new uid_entry();
	uce->uid = INT_MAX; 
	uce->gid = INT_MAX; 
	uce->lastupdated = time(NULL);
}

/* allocates new cache entry and zeros out all the fields */
void
passwd_cache::init_group_entry(group_entry *&gce) {
		
	gce = new group_entry();
	gce->gidlist = NULL;
	gce->gidlist_sz = 0;
	gce->lastupdated = time(NULL);
}
