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


/*
 * Description:	 Condor IP Verify: Implementation to enforce a given
 * IP address-based security policy.  An IP address can be given a mask
 * that specifies level of access, such as READ, WRITE, ADMINISTRATOR,
 * whatever.  Methods specify the level of access for a given IP address,
 * or subnet, or domain name.  
 *
 * Most of the access levels form a hierarchy.
 * DAEMON and ADMINISTRATOR levels imply WRITE level.
 * WRITE, NEGOTIATOR, and CONFIG levels imply READ level.
 * Thus, a client that has DAEMON level access will be authorized if
 * the required access level is READ.
 */

#ifndef _CONDOR_IPVERIFY_H_
#define _CONDOR_IPVERIFY_H_

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_perms.h"
#include "condor_sockaddr.h"
#include <string>
#include <vector>

template <class Key, class Value> class HashTable; // forward declaration

/// Not_Yet_Ducumented
static const int USERVERIFY_ALLOW = 0;
/// Not_Yet_Ducumented
static const int USERVERIFY_USE_TABLE = 1;
/// Not_Yet_Ducumented
static const int USERVERIFY_ONLY_DENIES = 2;
/// Not_Yet_Ducumented
static const int USERVERIFY_DENY = 3;

/// type used for permission bit-mask; see allow_mask() and deny_mask()
typedef uint64_t perm_mask_t;

	/** PermString() converts the given DCpermission into the
		human-readable string version of the name.
		@param perm The permission you want to convert
		@return The string version of it
	*/
const char* PermString( DCpermission perm );

static const int USER_AUTH_FAILURE = 0;
static const int USER_AUTH_SUCCESS = 1;
static const int USER_ID_REQUIRED  = 2;

/** Not_Yet_Ducumented
 */
class IpVerify {

public:

	///
	IpVerify();

	///
	~IpVerify();

	/** Params information out of the condor_config file and
		sets up the initial permission hash table
		@return Not_Yet_Ducumented
	*/
	int Init();

	/** Tell IpVerify() to reconfigure itself.
	 *  This also happens to clear cached authorization information,
	 *  which serves as our "DNS cache".
	 */
	void reconfig();

	/** Tell IpVerify() to get rid of cached DNS information.
	 *  This just exists to make it clear what the caller wants.
	 *  It is currently just a synonymn for reconfig().
	 */
	void refreshDNS();

	/** Verify() method returns whether connection should be allowed or
		refused.
		@param perm		   Not_Yet_Ducumented
		@param addr		   Not_Yet_Ducumented
		@param user        NULL or "" or fully qualified username
		@param allow_reasy NULL or buffer to write explanation into
		@param deny_reason NULL or buffer to write explanation into
		@return USER_AUTH_SUCCESS -- if success, USER_AUTH_FAILURE -- if failer
                USER_ID_REQUIRED -- if user id is required but the caller did not pass in
	*/
	int Verify( DCpermission perm, const condor_sockaddr& addr, const char * user, std::string & allow_reason, std::string & deny_reason );

	/** Dynamically opens a hole in the authorization settings for the
	    given (user, IP) at the given perm level.
	        @param  perm The permission level to open.
	        @param  id   The user / IP to open a hole for, in the form
	                     "user/IP" or just "IP" for any user.
	        @return      true on success, false on failure.
	*/
	bool PunchHole(DCpermission perm, const std::string& id);

	/** Remove an authorization hole previously opened using PunchHole().
	        @param  perm The permission level that was opened.
	        @param  id   The user / IP that the hole was opened for.
	        @return      true on success, false on failure.
	*/
	bool FillHole(DCpermission perm, const std::string& id);

private:

    typedef std::map<std::string, perm_mask_t> UserPerm_t;     // <userid, mask> pair
    /* This is for listing users per host */
    typedef std::map<std::string, std::vector<std::string>> UserHash_t;

    typedef std::map <std::string, int> HolePunchTable_t;

    typedef std::vector<std::string> netgroup_list_t;

	class PermTypeEntry {
	public:
		int behavior;
		UserHash_t allow_users;
		UserHash_t deny_users;

        // used if netgroups are supported
        netgroup_list_t allow_netgroups;
        netgroup_list_t deny_netgroups;

		PermTypeEntry() {
			behavior = USERVERIFY_USE_TABLE;
		}
	};

	bool LookupCachedVerifyResult( DCpermission perm, const struct in6_addr &sin6, const char * user, perm_mask_t & mask);
	void add_hash_entry(const struct in6_addr & sin6_addr, const char * user, perm_mask_t new_mask);
	void fill_table( PermTypeEntry * pentry, char * list, bool allow);
    void split_entry(const char * entry, std::string& host, std::string& user);
	perm_mask_t allow_mask(DCpermission perm);
	perm_mask_t deny_mask(DCpermission perm);

	void PermMaskToString(perm_mask_t mask, std::string &mask_str);
	void UserHashToString(UserHash_t& user_hash, std::string &result);
	void AuthEntryToString(const struct in6_addr & host, const char * user, perm_mask_t mask, std::string &result);
	void PrintAuthTable(int dprintf_level);

		// See if there is an authorization policy entry for a specific user at
		// a specific ip/hostname.
	bool lookup_user_ip_allow(DCpermission perm, char const *user, char const *ip);
	bool lookup_user_ip_deny(DCpermission perm, char const *user, char const *ip);
	bool lookup_user_host_allow(DCpermission perm, char const *user, char const *hostname);
	bool lookup_user_host_deny(DCpermission perm, char const *user, char const *hostname);

		// This is the low-level function called by the other lookup_user functions.
	bool lookup_user(UserHash_t& users, netgroup_list_t& netgroups, char const *user, char const *ip, char const *hostname, bool is_allow_list);

	char * merge(char * newPerm, char * oldPerm);
	bool did_init;

	PermTypeEntry* PermTypeArray[LAST_PERM];

	HolePunchTable_t PunchedHoleArray[LAST_PERM];

	typedef std::map<struct in6_addr, UserPerm_t> PermHashTable_t;
	PermHashTable_t PermHashTable;
};
	


#endif // of ifndef _CONDOR_USERVERIFY_H_
