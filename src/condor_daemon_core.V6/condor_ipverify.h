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
 * Most of the access levels form a heirarchy.
 * DAEMON and ADMINISTRATOR levels imply WRITE level.
 * WRITE, NEGOTIATOR, and CONFIG levels imply READ level.
 * Thus, a client that has DAEMON level access will be authorized if
 * the required access level is READ.
 */

#ifndef _CONDOR_IPVERIFY_H_
#define _CONDOR_IPVERIFY_H_

#include "condor_common.h"
#include "condor_debug.h"
#include "string_list.h"
#include "net_string_list.h"
#include "MyString.h"
#include "condor_perms.h"

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

	/** Verify() method returns whether connection should be allowed or
		refused.
		@param perm		   Not_Yet_Ducumented
		@param sockaddr_in Not_Yet_Ducumented
		@return USER_AUTH_SUCCESS -- if success, USER_AUTH_FAILURE -- if failer
                USER_ID_REQUIRED -- if user id is required but the caller did not pass in
	*/
	int Verify( DCpermission perm, const struct sockaddr_in *sin, const char * user = NULL );

	/** Not_Yet_Ducumented
		@param flag TRUE or FALSE.	TRUE means cache resolver lookups in our
			   hashtable cache, FALSE means do a gethostbyaddr() lookup
			   every time.
	*/
	void CacheDnsResults(int flag) { cache_DNS_results = flag; }

	bool AddAllowHost( const char* host, DCpermission perm );
		/** AddAllowHost() method adds a new host to the hostallow
			table, for the given permission level.
			@param host IP address or hostname (no wildcards)
			@param perm Permission level to use
			@return TRUE if successful, FALSE on error
		*/
private:

    typedef HashTable <MyString, perm_mask_t> UserPerm_t;     // <userid, mask> pair
    /* This is for listing users per host */
    typedef HashTable <MyString, StringList *>    UserHash_t;


	class PermTypeEntry {
	public:
		int behavior;
		NetStringList* allow_hosts;
		NetStringList* deny_hosts;
		UserHash_t* allow_users;
		UserHash_t* deny_users;
		PermTypeEntry() {
			allow_hosts = NULL;
			deny_hosts  = NULL;
			allow_users = NULL;
			deny_users  = NULL;
			behavior = USERVERIFY_USE_TABLE;
		}
		~PermTypeEntry(); 
	};

    bool has_user(UserPerm_t * , const char *, perm_mask_t & , MyString &);
	int add_hash_entry(const struct in_addr & sin_addr, const char * user, perm_mask_t new_mask);
	void fill_table( PermTypeEntry * pentry, perm_mask_t mask, char * list, bool allow);
	int cache_DNS_results;
    void split_entry(const char * entry, char ** host, char ** user);
	inline perm_mask_t allow_mask(DCpermission perm) { return (1 << (1+2*perm)); }
	inline perm_mask_t deny_mask(DCpermission perm) { return (1 << (2+2*perm)); }

	void PermMaskToString(perm_mask_t mask, MyString &mask_str);
	bool lookup_user(StringList * list, const char * user);
	char * merge(char * newPerm, char * oldPerm);
	int did_init;

	bool add_host_entry( const char* addr, perm_mask_t new_mask );
	void process_allow_users( void );

	PermTypeEntry* PermTypeArray[LAST_PERM];

	typedef HashTable <struct in_addr, UserPerm_t *> PermHashTable_t;
	PermHashTable_t* PermHashTable;

	typedef HashTable <MyString, perm_mask_t> HostHashTable_t; // <host, mask> pair
	HostHashTable_t* AllowHostsTable;
};
	


#endif // of ifndef _CONDOR_USERVERIFY_H_
