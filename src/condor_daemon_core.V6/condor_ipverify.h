/**************************************************************
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright © 1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.	
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.	For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
**************************************************************/

/*
 * Description:	 Condor IP Verify: Implementation to enforce a given
 * IP address-based security policy.  An IP address can be given a mask
 * that specifies level of access, such as READ, WRITE, ADMINISTRATOR,
 * whatever.  Methods specify the level of access for a given IP address,
 * or subnet, or domain name.  
 */

#ifndef _CONDOR_IPVERIFY_H_
#define _CONDOR_IPVERIFY_H_

#include "condor_common.h"
#include "condor_debug.h"
#include "HashTable.h"
#include "string_list.h"
#include "MyString.h"

// IMPORTANT NOTE:  If you add a new enum value here, please add a new
// case to PermString() in condor_ipverify.C.
/// enum for Daemon Core socket/command/signal permissions
enum DCpermission {
	/** Not_Yet_Ducumented */ ALLOW,
	/** Not_Yet_Ducumented */ READ,
	/** Not_Yet_Ducumented */ WRITE,
	/** Not_Yet_Ducumented */ NEGOTIATOR,
	/** Not_Yet_Ducumented */ IMMEDIATE_FAMILY,
	/** Not_Yet_Ducumented */ ADMINISTRATOR,
	/** Not_Yet_Ducumented */ OWNER,
	/** Not_Yet_Ducumented */ CONFIG_PERM
};


/// Not_Yet_Ducumented
static const int IPVERIFY_ALLOW = 0;
/// Not_Yet_Ducumented
static const int IPVERIFY_USE_TABLE = 1;
/// Not_Yet_Ducumented
static const int IPVERIFY_ONLY_DENIES = 2;
/// Not_Yet_Ducumented
static const int IPVERIFY_DENY = 3;
///	Not_Yet_Ducumented.  15 is max for 32-bit mask
static const int IPVERIFY_MAX_PERM_TYPES = 15;
		
	/** PermString() converts the given DCpermission into the
		human-readable string version of the name.
		@param perm The permission you want to convert
		@return The string version of it
	*/
const char* PermString( DCpermission perm );

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
		@return TRUE if allowed, FALSE if refused
	*/
	int Verify( DCpermission perm, const struct sockaddr_in *sin );

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

	// perm_names and perm_ints must match up.	this is to map
	// permission enums (ints) with their human-readable string name.
	static const char* perm_names[];
	static const int perm_ints[];

	int add_hash_entry(const struct in_addr & sin_addr,int new_mask);
	void fill_table( StringList *slist, int mask );
	int cache_DNS_results;

	inline int allow_mask(int perm) { return (1 << (1+2*perm)); }
	inline int deny_mask(int perm) { return (1 << (2+2*perm)); }
	
	int did_init;

	bool add_host_entry( const char* addr, int new_mask );
	void process_allow_hosts( void );

	class PermTypeEntry {
	public:
		int behavior;
		StringList* allow_hosts;
		StringList* deny_hosts;
		PermTypeEntry() {
			allow_hosts = NULL;
			deny_hosts = NULL;
			behavior = IPVERIFY_USE_TABLE;
		}
		~PermTypeEntry() {
			if (allow_hosts)
				delete allow_hosts;
			if (deny_hosts)
				delete deny_hosts;
		}
	};
	
	PermTypeEntry* PermTypeArray[IPVERIFY_MAX_PERM_TYPES];

	typedef HashTable <struct in_addr, int> PermHashTable_t;
	PermHashTable_t* PermHashTable;

	typedef HashTable <MyString, int> HostHashTable_t;
	HostHashTable_t* AllowHostsTable;
};
	


#endif // of ifndef _CONDOR_IPVERIFY_H_
