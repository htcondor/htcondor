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
#include "condor_random_num.h"


passwd_cache::passwd_cache() {

		/* set the number of seconds until a cache entry expires */
		// Randomize this timer a bit to decrease chances of lots of
		// processes all pounding on NIS at the same time.
	int default_lifetime = 72000 + get_random_int_insecure() % 60;
	Entry_lifetime = param_integer("PASSWD_CACHE_REFRESH", default_lifetime );
	loadConfig();
}
void
passwd_cache::reset() {

	group_table.clear();
	uid_table.clear();

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
parseGid(char const *str,gid_t &gid) {
	char *endstr;
	gid = strtol(str,&endstr,10);
	if( !endstr || *endstr ) {
		return false;
	}
	return true;
}

void
passwd_cache::getUseridMap(std::string &usermap)
{
	// fill in string with entries of form expected by loadConfig()

	for (auto& [index, uent] : uid_table) {
		if( !usermap.empty() ) {
			usermap += ' ';
		}
		formatstr_cat(usermap, "%s=%ld,%ld",index.c_str(),(long)uent.uid,(long)uent.gid);
		auto it = group_table.find(index);
		if (it != group_table.end()) {
			for (auto gid: it->second.gidlist) {
				if( gid == uent.gid ) {
					// already included this gid, because it is the primary
					continue;
				}
				formatstr_cat(usermap, ",%ld",(long)gid);
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
	std::string usermap_str;
	param(usermap_str, "USERID_MAP");
	if (usermap_str.empty()) {
		return;
	}
	
		// format is "username=uid,gid,gid2,gid3,... user2=uid2,gid2,..."
		// first split on spaces, which separate the records
		// If gid2 is "?", then we assume that supplemental groups
		// are unknown.
	for (const auto &name_equal_uid_gids: StringTokenIterator(usermap_str, " ")) {
		size_t pos = name_equal_uid_gids.find('=');
		ASSERT(pos != std::string::npos);
		std::string username = name_equal_uid_gids.substr(0, pos);
		std::string uid_gids = name_equal_uid_gids.substr(pos + 1);

			// the user/group ids are separated by commas
		std::vector<std::string> ids = split(uid_gids,",");

		if (ids.size() < 2) {
			EXCEPT("INVALID USERID_MAP entry %s=%s", username.c_str(), uid_gids.c_str());
		}
		struct passwd pwent;
		const std::string &idstr = ids.front();
		uid_t uid ;
		gid_t gid;
		if( !parseUid(idstr.c_str(),&uid) ) {
			EXCEPT("INVALID USERID_MAP entry %s=%s", username.c_str(), uid_gids.c_str());
		}
		const std::string &gidstr = ids[1];
		if(!parseGid(gidstr.c_str(), gid) ) {
			EXCEPT("INVALID USERID_MAP entry %s=%s", username.c_str(), uid_gids.c_str());
		}
		pwent.pw_name = const_cast<char *>(username.c_str());
		pwent.pw_uid = uid;
		pwent.pw_gid = gid;
		cache_uid(&pwent);

		const std::string supgidstr = ids.size() > 2 ? ids[2] : std::string("");
		if( supgidstr == "?") {
			continue; // no information about supplemental groups
		}

		auto [it, success] = group_table.emplace(username, group_entry());
		group_entry& group_cache_entry = it->second;

			/* now get the supplemental group list */
		for (auto gid_it = ids.begin() + 1;
				gid_it != ids.end();
				gid_it++) {

			if( !parseGid(gid_it->c_str(), gid) ) {
				EXCEPT("INVALID USERID_MAP entry %s=%s", username.c_str(), uid_gids.c_str());
			}
			group_cache_entry.gidlist.emplace_back(gid);
		}
			/* finally, insert info into our cache */
		group_cache_entry.lastupdated = time(nullptr);
	}
}

passwd_cache::~passwd_cache() {

	reset();
}

/* uses initgroups() and getgroups() to get the supplementary
   group info, then stashes it in our cache. We must be root when calling
 this function, since it calls initgroups(). */
bool passwd_cache::cache_groups(const char* user) {

	bool result;
	gid_t user_gid;
   
	result = true;

	if ( user == NULL ) {
		return false;
	} else { 

		if ( !get_user_gid(user, user_gid) ) {
			dprintf(D_ALWAYS, "cache_groups(): get_user_gid() failed! "
				   "errno=%s\n", strerror(errno));
			return false;
		}

		auto [it, success] = group_table.insert({user, group_entry()});
		group_entry& group_cache_entry = it->second;

		/* We need to get the primary and supplementary group info, so
		 * we're going to call initgroups() first, then call get groups
		 * so we can cache whatever we get.*/

		if ( initgroups(user, user_gid) != 0 ) {
			dprintf(D_ALWAYS, "passwd_cache: initgroups() failed! errno=%s\n",
					strerror(errno));
			group_table.erase(it);
			return false;
		}

		/* get the number of groups from the OS first */
		int ret = ::getgroups(0,NULL);

		if ( ret < 0 ) {
			group_table.erase(it);
			result = false;
		} else {
			group_cache_entry.gidlist.resize(ret);

			/* now get the group list */
			if (::getgroups( group_cache_entry.gidlist.size(),
			                 group_cache_entry.gidlist.data()) < 0) {
				dprintf(D_ALWAYS, "cache_groups(): getgroups() failed! "
						"errno=%s\n", strerror(errno));
				group_table.erase(it);
				result = false;
			} else {
				/* finally, update the timestamp */
				group_cache_entry.lastupdated = time(nullptr);

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
	
	std::string index;
   
	if ( pwent == NULL ) {
			/* a little sanity check */
		return false;
	} else {

		index = pwent->pw_name;

		auto [it, success] = uid_table.insert({index, uid_entry()});
		it->second.uid = pwent->pw_uid;
		it->second.gid = pwent->pw_gid;
			/* reset lastupdated */
		it->second.lastupdated = time(nullptr);
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
	return (int)cache_entry->gidlist.size();
}

/* retrieves user's groups from cache */
bool
passwd_cache::get_groups( const char *user, size_t groupsize, gid_t gid_list[] ) {

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

	if ( cache_entry->gidlist.size() > groupsize ) {
		dprintf(D_ALWAYS, "Inadequate size for gid list!\n");
		return false;
	}

	std::copy(cache_entry->gidlist.begin(), cache_entry->gidlist.end(), gid_list);

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

	struct passwd *pwd;

	for (const auto& [index, ent] : uid_table) {
		if ( ent.uid == uid ) {
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

	if (!user) {
		return false;
	}
	auto it = uid_table.find(user);
	if (it == uid_table.end()) {
		/* cache miss */
		return false;
	} else {
		uce = &it->second;
		if ( (time(NULL) - it->second.lastupdated) > Entry_lifetime ) {
			/* time to refresh the entry! */
			cache_uid(user);
		} else {
			/* entry is still considered valid, so just return */
		}
		return true;
	}
}

/* wrapper function around hashtable->lookup() that also decides 
 * when to refresh the requested entry */

bool
passwd_cache::lookup_group(const char *user, group_entry *&gce) {

	if (!user) {
		return false;
	}
	auto it = group_table.find(user);
	if (it == group_table.end()) {
			/* cache miss */
		return false;
	} else {
		gce = &it->second;
		if ( (time(NULL) - it->second.lastupdated) > Entry_lifetime ) {
				/* time to refresh the entry! */
				/* If the cache_groups() fails, then the old entry is purged */
			return cache_groups(user);
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
