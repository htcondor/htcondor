/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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


/* implements a simple cache. Also handles refreshes to keep cache
elements from getting stale. 

IMPORTANT NOTE: Don't dprintf() in here, unless its a fatal error! */

#include "condor_common.h"
#include "passwd_cache.unix.h"
#include "condor_config.h"
#include "HashTable.h"
#include "condor_random_num.h"


passwd_cache::passwd_cache() {

	uid_table = new UidHashTable(hashFunction);
	group_table = new GroupHashTable(hashFunction);
		/* set the number of seconds until a cache entry expires */
		// Randomize this timer a bit to decrease chances of lots of
		// processes all pounding on NIS at the same time.
	int default_lifetime = 72000 + get_random_int_insecure() % 60;
	Entry_lifetime = param_integer("PASSWD_CACHE_REFRESH", default_lifetime );
	loadConfig();
}
void
passwd_cache::reset() {

	group_entry *gent;
	uid_entry *uent;
	std::string index;

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

	loadConfig();
}

bool
parseUid(char const *str,uid_t *uid) {
	ASSERT( uid );
	char *endstr;
	*uid = strtol(str,&endstr,10);
	if( !endstr || *endstr ) {
		return false;
	}
	return true;
}

bool
parseGid(char const *str,gid_t *gid) {
	ASSERT( gid );
	char *endstr;
	*gid = strtol(str,&endstr,10);
	if( !endstr || *endstr ) {
		return false;
	}
	return true;
}

void
passwd_cache::getUseridMap(std::string &usermap)
{
	// fill in string with entries of form expected by loadConfig()
	uid_entry *uent;
	group_entry *gent;
	std::string index;

	uid_table->startIterations();
	while ( uid_table->iterate(index, uent) ) {
		if( !usermap.empty() ) {
			usermap += " ";
		}
		formatstr_cat(usermap, "%s=%ld,%ld",index.c_str(),(long)uent->uid,(long)uent->gid);
		if( group_table->lookup(index,gent) == 0 ) {
			unsigned i;
			for(i=0;i<gent->gidlist_sz;i++) {
				if( gent->gidlist[i] == uent->gid ) {
					// already included this gid, because it is the primary
					continue;
				}
				formatstr_cat(usermap, ",%ld",(long)gent->gidlist[i]);
			}
		}
		else {
			// indicate that supplemental groups are unknown
			formatstr_cat(usermap, ",?");
		}
	}
}

void
passwd_cache::loadConfig() {
		// initialize cache with any configured mappings
	char *usermap_str = param("USERID_MAP");
	if( !usermap_str ) {
		return;
	}

		// format is "username=uid,gid,gid2,gid3,... user2=uid2,gid2,..."
		// first split on spaces, which separate the records
		// If gid2 is "?", then we assume that supplemental groups
		// are unknown.
	StringList usermap(usermap_str," ");
	free( usermap_str );

	char *username;
	usermap.rewind();
	while( (username=usermap.next()) ) {
		char *userids = strchr(username,'=');
		ASSERT( userids );
		*userids = '\0';
		userids++;

			// the user ids are separated by commas
		StringList ids(userids,",");
		ids.rewind();

		struct passwd pwent;

		char const *idstr = ids.next();
		uid_t uid;
		gid_t gid;
		if( !idstr || !parseUid(idstr,&uid) ) {
			EXCEPT("Invalid USERID_MAP entry %s=%s",username,userids);
		}
		idstr = ids.next();
		if( !idstr || !parseGid(idstr,&gid) ) {
			EXCEPT("Invalid USERID_MAP entry %s=%s",username,userids);
		}
		pwent.pw_name = username;
		pwent.pw_uid = uid;
		pwent.pw_gid = gid;
		cache_uid(&pwent);

		idstr = ids.next();
		if( idstr && !strcmp(idstr,"?") ) {
			continue; // no information about supplemental groups
		}

		ids.rewind();
		ids.next(); // go to first group id

		group_entry *group_cache_entry;
		if ( group_table->lookup(username, group_cache_entry) < 0 ) {
			init_group_entry(group_cache_entry);
			group_table->insert(username, group_cache_entry);
		}

			/* now get the group list */
		if ( group_cache_entry->gidlist != NULL ) {
			delete[] group_cache_entry->gidlist;
			group_cache_entry->gidlist = NULL;
		}
		group_cache_entry->gidlist_sz = ids.number()-1;
		group_cache_entry->gidlist = new
			gid_t[group_cache_entry->gidlist_sz];

		unsigned g;
		for(g=0; g<group_cache_entry->gidlist_sz; g++) {
			idstr = ids.next();
			ASSERT( idstr );

			if( !parseGid(idstr,&group_cache_entry->gidlist[g]) ) {
				EXCEPT("Invalid USERID_MAP entry %s=%s",username,userids);
			}
		}

			/* finally, insert info into our cache */
		group_cache_entry->lastupdated = time(NULL);
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
   
	group_cache_entry = NULL;
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
		} else {
			// The code below assumes the entry is not in the cache.
			group_table->remove(user);
		}

		/* We need to get the primary and supplementary group info, so
		 * we're going to call initgroups() first, then call get groups
		 * so we can cache whatever we get.*/

		if ( initgroups(user, user_gid) != 0 ) {
			dprintf(D_ALWAYS, "passwd_cache: initgroups() failed! errno=%s\n",
					strerror(errno));
			delete group_cache_entry;
			return false;
		}

		/* get the number of groups from the OS first */
		int ret = ::getgroups(0,NULL);

		if ( ret < 0 ) {
			delete group_cache_entry;
			result = false;
		} else {
			group_cache_entry->gidlist_sz = ret;

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
				delete group_cache_entry;
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
	const char *err_string;

	errno = 0;
	pwent = getpwnam(user);
	if ( pwent == NULL ) {
			// According to POSIX, to differentiate the case between
			// getpwnam() legitimately not finding a user and having an
			// error not finding a user, in the former case NULL is
			// returned and errno is left unchanged. In the latter case
			// NULL is returned and errno is set appropriately. So to
			// deal with the former case properly, I've set errno to be
			// some known value I can check here.
			
			// Under linux, getpwnam sets errno to ENOENT the former case, so
			// we consider that as well.
		if( errno == 0 
#if defined(LINUX)
			|| errno == ENOENT
#endif
		) 
		{
			static const char *errno_clarification = "user not found";
			err_string = errno_clarification;
		} else {
			err_string = strerror( errno );
		}
		dprintf( D_ALWAYS, "passwd_cache::cache_uid(): getpwnam(\"%s\") "
				 "failed: %s\n", user, err_string );
		return false;
	}

	// check for root priv
	if (0 == pwent->pw_uid) {
		dprintf(D_ALWAYS, "WARNING: getpwnam(%s) returned ZERO!\n", user);
		// ZKM: should we bail?
	} else {
		dprintf(D_PRIV, "getpwnam(%s) returned (%i)\n", user, pwent->pw_uid);
	}

   	return cache_uid(pwent);
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
	std::string index;

   
	if ( pwent == NULL ) {
			/* a little sanity check */
		return false;
	} else {

		index = pwent->pw_name;

		if ( uid_table->lookup(index, cache_entry) < 0 ) {
				/* if we don't already have this entry, create a new one */
			init_uid_entry(cache_entry);
			uid_table->insert(index, cache_entry);
		}

	   	cache_entry->uid = pwent->pw_uid;
	   	cache_entry->gid = pwent->pw_gid;
			/* reset lastupdated */
		cache_entry->lastupdated = time(NULL);
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
	std::string index;

	uid_table->startIterations();
	while ( uid_table->iterate(index, ent) ) {
		if ( ent->uid == uid ) {
			user = strdup(index.c_str());
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
passwd_cache::init_groups( const char* user, gid_t additional_gid ) {

	gid_t *gid_list;
	bool result;
	int siz;

	siz = num_groups(user);
	result = true;
	gid_list = NULL;

	if ( siz > 0 ) {

		gid_list = new gid_t[siz + 1];

		if ( get_groups(user, siz, gid_list) ) { 

			if (additional_gid != 0) {
				gid_list[siz] = additional_gid;
				siz++;
			}

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
		dprintf(D_ALWAYS,
		        "passwd_cache: num_groups( %s ) returned %d\n",
		        user,
		        siz);
		result = false;
	}

	if ( gid_list ) { delete[] gid_list; }
	return result;
}

/* wrapper function around hashtable->lookup() that also decides 
 * when to refresh the requested entry */
bool
passwd_cache::lookup_uid(const char *user, uid_entry *&uce) {

	if ( !user || uid_table->lookup(user, uce) < 0 ) {
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

	if ( !user || group_table->lookup(user, gce) < 0 ) {
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
