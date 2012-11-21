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
#include "condor_ipverify.h"
#include "internet.h"
#include "condor_config.h"
#include "condor_perms.h"
#include "condor_netdb.h"
#include "subsystem_info.h"

#include "HashTable.h"
#include "sock.h"
#include "condor_secman.h"
#include "ipv6_hostname.h"
#include "condor_netaddr.h"

// Externs to Globals

const char TotallyWild[] = "*";


// Hash function for Permission hash table
static unsigned int
compute_perm_hash(const in6_addr &in_addr)
{
		// the hash function copied from MyString::Hash()
	int Len = sizeof(in6_addr);
	const unsigned char* Data = (const unsigned char*)&in_addr;
	int i;
	unsigned int result = 0;
	for(i = 0; i < Len; i++) {
		result = (result<<5) + result + Data[i];
	}
	return result;
}

// Hash function for HolePunchTable_t hash tables
static unsigned int
compute_host_hash( const MyString & str )
{
	return ( str.Hash() );
}

// == operator for struct in_addr, also needed for hash table template
bool operator==(const in6_addr& a, const in6_addr& b) {
	return IN6_ARE_ADDR_EQUAL(&a, &b);
}

// Constructor
IpVerify::IpVerify() 
{
	did_init = FALSE;

	DCpermission perm;
	for (perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		PermTypeArray[perm] = NULL;
		PunchedHoleArray[perm] = NULL;
	}

	PermHashTable = new PermHashTable_t(797, compute_perm_hash);
}


// Destructor
IpVerify::~IpVerify()
{

	// Clear the Permission Hash Table
	if (PermHashTable) {
		// iterate through the table and delete the entries
		in6_addr key;
		UserPerm_t * value;
		PermHashTable->startIterations();

		while (PermHashTable->iterate(key, value)) {
			delete value;
		}

		delete PermHashTable;
	}

	// Clear the Permission Type Array and Punched Hole Array
	DCpermission perm;
	for (perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		if ( PermTypeArray[perm] )
			delete PermTypeArray[perm];
		if ( PunchedHoleArray[perm] )
			delete PunchedHoleArray[perm];
	}	
}


int
IpVerify::Init()
{
	char *pAllow = NULL, *pDeny = NULL, *pOldAllow = NULL, *pOldDeny = NULL,
		*pNewAllow = NULL, *pNewDeny = NULL;
	DCpermission perm;
	const char* const ssysname = get_mySubSystem()->getName();	

	did_init = TRUE;

	// Make sure that perm_mask_t is big enough to hold all possible
	// results of allow_mask() and deny_mask().
	ASSERT( sizeof(perm_mask_t)*8 - 2 > LAST_PERM );

	// Clear the Permission Hash Table in case re-initializing
	if (PermHashTable) {
		// iterate through the table and delete the entries
		struct in6_addr key;
		UserPerm_t * value;
		PermHashTable->startIterations();

		while (PermHashTable->iterate(key, value)) {
			delete value;
		}

		PermHashTable->clear();
	}

	// and Clear the Permission Type Array
	for (perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		if ( PermTypeArray[perm] ) {
			delete PermTypeArray[perm];
			PermTypeArray[perm] = NULL;
		}
	}

	// This is the new stuff
	for ( perm=FIRST_PERM; perm < LAST_PERM; perm=NEXT_PERM(perm) ) {
		PermTypeEntry* pentry = new PermTypeEntry();
		ASSERT( pentry );
		PermTypeArray[perm] = pentry;
		MyString allow_param, deny_param;

		dprintf(D_SECURITY,"IPVERIFY: Subsystem %s\n",ssysname);
		dprintf(D_SECURITY,"IPVERIFY: Permission %s\n",PermString(perm));
		if(strcmp(ssysname,"TOOL")==0 || strcmp(ssysname,"SUBMIT")==0){
			// to avoid unneccesary DNS activity, the TOOL and SUBMIT
			// subsystems only load the CLIENT lists, since they have no
			// command port and don't need the other authorization lists.
			if(strcmp(PermString(perm),"CLIENT")==0){ 
				pNewAllow = SecMan::getSecSetting("ALLOW_%s",perm,&allow_param, ssysname );
				pOldAllow = SecMan::getSecSetting("HOSTALLOW_%s",perm,&allow_param, ssysname );
				pNewDeny = SecMan::getSecSetting("DENY_%s",perm,&deny_param, ssysname );
				pOldDeny = SecMan::getSecSetting("HOSTDENY_%s",perm,&deny_param, ssysname );
			}
		} else {
			pNewAllow = SecMan::getSecSetting("ALLOW_%s",perm,&allow_param, ssysname );
			pOldAllow = SecMan::getSecSetting("HOSTALLOW_%s",perm,&allow_param, ssysname );
			pNewDeny = SecMan::getSecSetting("DENY_%s",perm,&deny_param, ssysname );
			pOldDeny = SecMan::getSecSetting("HOSTDENY_%s",perm,&deny_param, ssysname );
		}
		// concat the two
		pAllow = merge(pNewAllow, pOldAllow);
		// concat the two
		pDeny = merge(pNewDeny, pOldDeny);
		if( pAllow ) {
			dprintf ( D_SECURITY, "IPVERIFY: allow %s: %s (from config value %s)\n", PermString(perm),pAllow,allow_param.Value());
		}
		if( pDeny ) {
			dprintf ( D_SECURITY, "IPVERIFY: deny %s: %s (from config value %s)\n", PermString(perm),pDeny,deny_param.Value());
		}
		// Treat a "*", "*/*" for ALLOW_XXX as if it's just undefined,
		// because that's the optimized default, except for
		// CONFIG_PERM which has a different default (see below).
		if( perm != CONFIG_PERM ) {
			if(pAllow && (!strcmp(pAllow, "*") || !strcmp(pAllow, "*/*"))) {
				free( pAllow );
				pAllow = NULL;
			}
		}
		if ( !pAllow && !pDeny ) {
			if (perm == CONFIG_PERM) { 	  // deny all CONFIG requests 
				pentry->behavior = USERVERIFY_DENY; // by default
				dprintf( D_SECURITY, "ipverify: %s optimized to deny everyone\n", PermString(perm) );
			} else {
				pentry->behavior = USERVERIFY_ALLOW;
				if( perm != ALLOW ) {
					dprintf( D_SECURITY, "ipverify: %s optimized to allow anyone\n", PermString(perm) );
				}
			}
		} else {
			if ( pDeny && !pAllow && perm != CONFIG_PERM ) {
				pentry->behavior = USERVERIFY_ONLY_DENIES;
			} else {
				pentry->behavior = USERVERIFY_USE_TABLE;
			}
			if ( pAllow ) {
				fill_table( pentry, pAllow, true );
				free(pAllow);
				pAllow = NULL;
			}
			if ( pDeny ) {
				fill_table( pentry,	pDeny, false );
				free(pDeny);
				pDeny = NULL;
			}
		}
		if (pAllow) {
			free(pAllow);
			pAllow = NULL;
		}
		if (pDeny) {
			free(pDeny);
			pDeny = NULL;
		}
		if (pOldAllow) {
			free(pOldAllow);
			pOldAllow = NULL;
		}
		if (pOldDeny) {
			free(pOldDeny);
			pOldDeny = NULL;
		}
		if (pNewAllow) {
			free(pNewAllow);
			pNewAllow = NULL;
		}
		if (pNewDeny) {
			free(pNewDeny);
			pNewDeny = NULL;
		}
	}
	dprintf(D_FULLDEBUG|D_SECURITY,"Initialized the following authorization table:\n");
	if(PermHashTable)	
		PrintAuthTable(D_FULLDEBUG|D_SECURITY);
	return TRUE;
}

char * IpVerify :: merge(char * pNewList, char * pOldList)
{
    char * pList = NULL;

    if (pOldList) {
        if (pNewList) {
            pList = (char *)malloc(strlen(pOldList) + strlen(pNewList) + 2);
            ASSERT( pList );
            sprintf(pList, "%s,%s", pNewList, pOldList);
        }
        else {
            pList = strdup(pOldList);
        }
    }
    else {
        if (pNewList) {
            pList = strdup(pNewList);
        }
    }
    return pList;
}

bool IpVerify :: has_user(UserPerm_t * perm, const char * user, perm_mask_t & mask )
{
    // Now, let's see if the user is there
    MyString user_key;
    assert(perm);

    if( !user || !*user ) {
		user_key = TotallyWild;
	}
	else {
		user_key = user;
	}

    return perm->lookup(user_key, mask) != -1;
}   


bool
IpVerify::LookupCachedVerifyResult( DCpermission perm, const struct in6_addr &sin6, const char * user, perm_mask_t & mask)
{
    UserPerm_t * ptable = NULL;

	if( PermHashTable->lookup(sin6, ptable) != -1 ) {

		if (has_user(ptable, user, mask)) {

				// We do not want to return true unless there is
				// a cached result for this specific perm level.

			if( mask & (allow_mask(perm)|deny_mask(perm)) ) {
				return true;
			}
		}
	}
	return false;
}

int
IpVerify::add_hash_entry(const struct in6_addr & sin6_addr, const char * user, perm_mask_t new_mask)
{
    UserPerm_t * perm = NULL;
    perm_mask_t old_mask = 0;  // must init old_mask to zero!!!
    MyString user_key = user;

	// assert(PermHashTable);
	if ( PermHashTable->lookup(sin6_addr, perm) != -1 ) {
		// found an existing entry.  

		if (has_user(perm, user, old_mask)) {
			// remove it because we are going to edit the mask below
			// and re-insert it.
			perm->remove(user_key);
        }
	}
    else {
        perm = new UserPerm_t(42, compute_host_hash);
        if (PermHashTable->insert(sin6_addr, perm) != 0) {
            delete perm;
            return FALSE;
        }
    }

    perm->insert(user_key, old_mask | new_mask);

	if( IsFulldebug(D_FULLDEBUG) || IsDebugLevel(D_SECURITY) ) {
		MyString auth_str;
		AuthEntryToString(sin6_addr,user,new_mask, auth_str);
		dprintf(D_FULLDEBUG|D_SECURITY,
				"Adding to resolved authorization table: %s\n",
				auth_str.Value());
	}

    return TRUE;
}

perm_mask_t 
IpVerify::allow_mask(DCpermission perm) { 
	return ((perm_mask_t)1 << (1+2*perm));
}

perm_mask_t 
IpVerify::deny_mask(DCpermission perm) { 
	return ((perm_mask_t)1 << (2+2*perm));
}
 
void
IpVerify::PermMaskToString(perm_mask_t mask, MyString &mask_str)
{
	DCpermission perm;
	for(perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		if( mask & allow_mask(perm) ) {
			mask_str.append_to_list(PermString(perm));
		}
		if( mask & deny_mask(perm) ) {
			mask_str.append_to_list("DENY_");
			mask_str += PermString(perm);
		}
	}
}

void
IpVerify::UserHashToString(UserHash_t *user_hash, MyString &result)
{
	ASSERT( user_hash );
	user_hash->startIterations();
	MyString host;
	StringList *users;
	char const *user;
	while( user_hash->iterate(host,users) ) {
		if( users ) {
			users->rewind();
			while( (user=users->next()) ) {
				result.formatstr_cat(" %s/%s",
								   user,
								   host.Value());
			}
		}
	}
}

void
IpVerify::AuthEntryToString(const in6_addr & host, const char * user, perm_mask_t mask, MyString &result)
{
		// every address will be seen as IPv6 format. should do something
		// to print IPv4 address neatly.
	char buf[INET6_ADDRSTRLEN];
	memset((void*)buf, 0, sizeof(buf));
	const uint32_t* addr = (const uint32_t*)&host;
		// checks if IPv4-mapped-IPv6 address
	
	const char* ret = NULL;
	if (addr[0] == 0 && addr[1] == 0 && addr[2] == htonl(0xffff)) {
		ret = inet_ntop(AF_INET, (const void*)&addr[3], buf, sizeof(buf));
	} else {
		ret = inet_ntop(AF_INET6, &host, buf, sizeof(buf));
	}

#ifndef WIN32
	if (!ret) {
		dprintf(D_HOSTNAME, "IP address conversion failed, errno = %d\n",
				errno);
	}
#endif

	MyString mask_str;
	PermMaskToString( mask, mask_str );
	result.formatstr("%s/%s: %s", /* NOTE: this does not need a '\n', all the call sites add one. */
			user ? user : "(null)",
			buf,
			mask_str.Value() );
}

void
IpVerify::PrintAuthTable(int dprintf_level) {
	struct in6_addr host;
	UserPerm_t * ptable;
	PermHashTable->startIterations();

	while (PermHashTable->iterate(host, ptable)) {
		MyString userid;
		perm_mask_t mask;

		ptable->startIterations();
		while( ptable->iterate(userid,mask) ) {
				// Call has_user() to get the full mask
			has_user(ptable, userid.Value(), mask);

			MyString auth_entry_str;
			AuthEntryToString(host,userid.Value(),mask, auth_entry_str);
			dprintf(dprintf_level,"%s\n", auth_entry_str.Value());
		}
	}

	dprintf(dprintf_level,"Authorizations yet to be resolved:\n");
	DCpermission perm;
	for ( perm=FIRST_PERM; perm < LAST_PERM; perm=NEXT_PERM(perm) ) {

		PermTypeEntry* pentry = PermTypeArray[perm];
		ASSERT( pentry );

		MyString allow_users,deny_users;

		if( pentry->allow_users ) {
			UserHashToString(pentry->allow_users,allow_users);
		}

		if( pentry->deny_users ) {
			UserHashToString(pentry->deny_users,deny_users);
		}

		if( allow_users.Length() ) {
			dprintf(dprintf_level,"allow %s: %s\n",
					PermString(perm),
					allow_users.Value());
		}

		if( deny_users.Length() ) {
			dprintf(dprintf_level,"deny %s: %s\n",
					PermString(perm),
					deny_users.Value());
		}
	}
}

// If host is a valid hostname, add its IP addresses to the list.
// Always add host itself to the list.
static void
ExpandHostAddresses(char const *host,StringList *list)
{
	list->append(host);
	condor_netaddr netaddr;

	if (strchr(host,'*') || strchr(host,'/') || netaddr.from_net_string(host)) {
		return; // not a valid hostname, so don't bother trying to look it up
	}

	std::vector<condor_sockaddr> addrs = resolve_hostname(host);
	for (std::vector<condor_sockaddr>::iterator iter = addrs.begin();
		 iter != addrs.end();
		 ++iter) {
		const condor_sockaddr& addr = *iter;
		list->append(addr.to_ip_string().Value());
	}
}

void
IpVerify::fill_table(PermTypeEntry * pentry, char * list, bool allow)
{
    assert(pentry);

	NetStringList * whichHostList = new NetStringList();
    UserHash_t * whichUserHash = new UserHash_t(1024, compute_host_hash);

    StringList slist(list);
	char *entry, * host, * user;
	slist.rewind();
	while ( (entry=slist.next()) ) {
		if (!*entry) {
			// empty string?
			slist.deleteCurrent();
			continue;
		}
		split_entry(entry, &host, &user);
		ASSERT( host );
		ASSERT( user );

			// If this is a hostname, get all IP addresses for it and
			// add them to the list.  This ensures that if we are given
			// a cname, we do the right thing later when trying to match
			// this record with the official hostname.
		StringList host_addrs;
		ExpandHostAddresses(host,&host_addrs);
		host_addrs.rewind();

		char const *host_addr;
		while( (host_addr=host_addrs.next()) ) {
			MyString hostString(host_addr);
			StringList * userList = 0;
				// add user to user hash, host to host list
			if (whichUserHash->lookup(hostString, userList) == -1) {
				whichUserHash->insert(hostString, new StringList(user)); 
				whichHostList->append(hostString.Value());
			}
			else {
				userList->append(user);
			}
		}

		free(host);
		free(user);
	}

    if (allow) {
        pentry->allow_hosts = whichHostList;
        pentry->allow_users  = whichUserHash;
    }
    else {
        pentry->deny_hosts = whichHostList;
        pentry->deny_users = whichUserHash;
    }
}

void IpVerify :: split_entry(const char * perm_entry, char ** host, char** user)
{
    char * slash0;
    char * slash1;
    char * at;
    char * permbuf;

	if (!perm_entry || !*perm_entry) {
		EXCEPT("split_entry called with NULL or &NULL!");
	}

    // see if there is a user specified... here are the
	// rules we use:
	//
	// if there are two slashes, the format is always
	// user/net/mask
	// 
	// if there is one slash, it could be either
	// net/mask  or  user/host  if it comes down to
	// the ambiguous */x, it will be user=*/host=x
	//
	// if there are zero slashes it could be either
	// user, host, or wildcard... look for an @ sign
	// to see if it is a user

	// make a local copy
	permbuf = strdup( perm_entry );
	ASSERT( permbuf );

    slash0 = strchr(permbuf, '/');
	if (!slash0) {
		at = strchr(permbuf, '@');
		if (at) {
			*user = strdup(permbuf);
			*host = strdup("*");
		} else {
			*user = strdup("*");

			// [IPV6] WHY DOES IT LOOK FOR COLON?
			// COLON IS ESSENTIAL PART OF IPV6 ADDRESS
			// look for a colon

//			char * colon;
//			colon = strchr(permbuf, ':');
//			if (colon) {
//				// colon points into permbuf.  permbuf is a local
//				// copy of the data made above, so we can modify it.
//				// drop a null in place of the colon so everything
//				// from the colon and beyond is gone.
//				*colon = 0;
//			}

			// now dup it
			*host = strdup(permbuf);
		}
	} else {
		// okay, there was one slash... look for another
		slash1 = strchr(slash0 + 1, '/');
		if (slash1) {
			// form is user/net/mask
			*slash0++ = 0;
			*user = strdup(permbuf);
			*host = strdup(slash0);
		} else {
			// could be either user/host or net/mask
			// handle */x case now too
			at = strchr(permbuf, '@');
			if ((at && at < slash0) || permbuf[0] == '*') {
				*slash0++ = 0;
				*user = strdup(permbuf);
				*host = strdup(slash0);
			} else {
				condor_netaddr netaddr;
				if (netaddr.from_net_string(permbuf)) {
				//if (is_valid_network(permbuf, NULL, NULL)) {
					*user = strdup("*");
					*host = strdup(permbuf);
				} else {
					dprintf (D_SECURITY, "IPVERIFY: warning, strange entry %s\n", permbuf);
					*slash0++ = 0;
					*user = strdup(permbuf);
					*host = strdup(slash0);
				}
			}
		}
	}
	free( permbuf );
}

void
IpVerify::reconfig()
{
	did_init = false;
}

void
IpVerify::refreshDNS() {
		// NOTE: this may be called when the daemon is starting a reconfig.
		// The new configuration may not have been read yet, but we do
		// not care, because this just sets a flag and we do the actual
		// work later.
	reconfig();
}

int
IpVerify::Verify( DCpermission perm, const condor_sockaddr& addr, const char * user, MyString *allow_reason, MyString *deny_reason )
{
	perm_mask_t mask;
	in6_addr sin6_addr;
	const char *thehost;
    const char * who = user;
	MyString peer_description; // we build this up as we go along (DNS etc.)

	if( !did_init ) {
		Init();
	}
	/*
	 * Be Warned:  careful about parameter "sin" being NULL.  It could be, in
	 * which case we should return FALSE (unless perm is ALLOW)
	 *
	 */

	switch ( perm ) {

	case ALLOW:
		return USER_AUTH_SUCCESS;
		break;

	default:
		break;
	}

	sin6_addr = addr.to_ipv6_address();
	mask = 0;	// must initialize to zero because we logical-or bits into this

    if (who == NULL || *who == '\0') {
        who = TotallyWild;
    }

	if ( perm >= LAST_PERM || !PermTypeArray[perm] ) {
		EXCEPT("IpVerify::Verify: called with unknown permission %d\n",perm);
	}


		// see if a authorization hole has been dyamically punched (via
		// PunchHole) for this perm / user / IP
		// Note that the permission hierarchy is dealt with in
		// PunchHole(), by punching a hole for all implied levels.
		// Therefore, if there is a hole or an implied hole, we will
		// always find it here before we get into the subsequent code
		// which recursively calls Verify() to traverse the hierarchy.
		// This is important, because we do not want holes to find
		// there way into the authorization cache.
		//
	if ( PunchedHoleArray[perm] != NULL ) {
		HolePunchTable_t* hpt = PunchedHoleArray[perm];
		MyString ip_str_buf = addr.to_ip_string();
		const char* ip_str = ip_str_buf.Value();
		MyString id_with_ip;
		MyString id;
		int count;
		if ( who != TotallyWild ) {
			id_with_ip.formatstr("%s/%s", who, ip_str);
			id = who;
			if ( hpt->lookup(id, count) != -1 )	{
				if( allow_reason ) {
					allow_reason->formatstr(
						"%s authorization has been made automatic for %s",
						PermString(perm), id.Value() );
				}
				return USER_AUTH_SUCCESS;
			}
			if ( hpt->lookup(id_with_ip, count) != -1 ) {
				if( allow_reason ) {
					allow_reason->formatstr(
						"%s authorization has been made automatic for %s",
						PermString(perm), id_with_ip.Value() );
				}
				return USER_AUTH_SUCCESS;
			}
		}
		id = ip_str;
		if ( hpt->lookup(id, count) != -1 ) {
			if( allow_reason ) {
				allow_reason->formatstr(
					"%s authorization has been made automatic for %s",
					PermString(perm), id.Value() );
			}
			return USER_AUTH_SUCCESS;
		}
	}

	if ( PermTypeArray[perm]->behavior == USERVERIFY_ALLOW ) {
			// allow if no HOSTALLOW_* or HOSTDENY_* restrictions 
			// specified.
		if( allow_reason ) {
			allow_reason->formatstr(
				"%s authorization policy allows access by anyone",
				PermString(perm));
		}
		return USER_AUTH_SUCCESS;
	}
		
	if ( PermTypeArray[perm]->behavior == USERVERIFY_DENY ) {
			// deny
		if( deny_reason ) {
			deny_reason->formatstr(
				"%s authorization policy denies all access",
				PermString(perm));
		}
		return USER_AUTH_FAILURE;
	}
		
	if( LookupCachedVerifyResult(perm,sin6_addr,who,mask) ) {
		if( deny_reason && (mask&deny_mask(perm)) ) {
			deny_reason->formatstr(
				"cached result for %s; see first case for the full reason",
				PermString(perm));
		}
		else if( allow_reason && (mask&allow_mask(perm)) ) {
			allow_reason->formatstr(
				"cached result for %s; see first case for the full reason",
				PermString(perm));
		}
	}
	else {
		mask = 0;

			// if the deny bit is already set, skip further DENY analysis
		perm_mask_t const deny_resolved = deny_mask(perm);
			// if the allow or deny bit is already set,
			// skip further ALLOW analysis
		perm_mask_t const allow_resolved = allow_mask(perm)|deny_mask(perm);

			// check for matching subnets in ip/mask style
		char ipstr[INET6_ADDRSTRLEN] = { 0, };
   		addr.to_ip_string(ipstr, INET6_ADDRSTRLEN);

		peer_description = addr.to_ip_string();

		if ( !(mask&deny_resolved) && lookup_user_ip_deny(perm,who,ipstr)) {
			mask |= deny_mask(perm);
			if( deny_reason ) {
				deny_reason->formatstr(
					"%s authorization policy denies IP address %s",
					PermString(perm), addr.to_ip_string().Value() );
			}
		}

		if ( !(mask&allow_resolved) && lookup_user_ip_allow(perm,who,ipstr)) {
			mask |= allow_mask(perm);
			if( allow_reason ) {
				allow_reason->formatstr(
					"%s authorization policy allows IP address %s",
					PermString(perm), addr.to_ip_string().Value() );
			}
		}


		std::vector<MyString> hostnames;
		// now scan through hostname strings
		if( !(mask&allow_resolved) || !(mask&deny_resolved) ) {
			hostnames = get_hostname_with_alias(addr);
		}

		for (unsigned int i = 0; i < hostnames.size(); ++i) {
			thehost = hostnames[i].Value();
			peer_description.append_to_list(thehost);

			if ( !(mask&deny_resolved) && lookup_user_host_deny(perm,who,thehost) ) {
				mask |= deny_mask(perm);
				if( deny_reason ) {
					deny_reason->formatstr(
						"%s authorization policy denies hostname %s",
						PermString(perm), thehost );
				}
			}

			if ( !(mask&allow_resolved) && lookup_user_host_allow(perm,who,thehost) ) {
				mask |= allow_mask(perm);
				if( allow_reason ) {
					allow_reason->formatstr(
						"%s authorization policy allows hostname %s",
						PermString(perm), thehost );
				}
			}
		}
			// if we found something via our hostname or subnet mactching, we now have 
			// a mask, and we should add it into our table so we need not
			// do a gethostbyaddr() next time.  if we still do not have a mask
			// (perhaps because this host doesn't appear in any list), create one
			// and then add to the table.
			// But first, check our parent permission levels in the
			// authorization heirarchy.
			// DAEMON and ADMINISTRATOR imply WRITE.
			// WRITE, NEGOTIATOR, and CONFIG_PERM imply READ.
		bool determined_by_parent = false;
		if ( mask == 0 ) {
			if ( PermTypeArray[perm]->behavior == USERVERIFY_ONLY_DENIES ) {
				dprintf(D_SECURITY,"IPVERIFY: %s at %s not matched to deny list, so allowing.\n",who, addr.to_sinful().Value());
				if( allow_reason ) {
					allow_reason->formatstr(
						"%s authorization policy does not deny, so allowing",
						PermString(perm));
				}

				mask |= allow_mask(perm);
			} else {
				DCpermissionHierarchy hierarchy( perm );
				DCpermission const *parent_perms =
					hierarchy.getPermsIAmDirectlyImpliedBy();
				bool parent_allowed = false;
				for( ; *parent_perms != LAST_PERM; parent_perms++ ) {
					if( Verify( *parent_perms, addr, user, allow_reason, NULL ) == USER_AUTH_SUCCESS ) {
						determined_by_parent = true;
						parent_allowed = true;
						dprintf(D_SECURITY,"IPVERIFY: allowing %s at %s for %s because %s is allowed\n",who, addr.to_sinful().Value(),PermString(perm),PermString(*parent_perms));
						if( allow_reason ) {
							MyString tmp = *allow_reason;
							allow_reason->formatstr(
								"%s is implied by %s; %s",
								PermString(perm),
								PermString(*parent_perms),
								tmp.Value());
						}
						break;
					}
				}
				if( parent_allowed ) {
					mask |= allow_mask(perm);
				}
				else {
					mask |= deny_mask(perm);

					if( !determined_by_parent && deny_reason ) {
							// We don't just allow anyone, and this request
							// did not match any of the entries we do allow.
							// In case the reason we didn't match is
							// because of a typo or a DNS problem, record
							// all the hostnames we searched for.
						deny_reason->formatstr(
							"%s authorization policy contains no matching "
							"ALLOW entry for this request"
							"; identifiers used for this host: %s, hostname size = %lu, "
							"original ip address = %s",
							PermString(perm),
							peer_description.Value(),
							(unsigned long)hostnames.size(),
							ipstr);
					}
				}
			}
		}

		if( !determined_by_parent && (mask&allow_mask(perm)) ) {
			// In case we are allowing because of not matching a DENY
			// entry that the user expected us to match (e.g. because
			// of typo or DNS problem), record all the hostnames we
			// searched for.
			if( allow_reason && !peer_description.IsEmpty() ) {
				allow_reason->formatstr_cat(
					"; identifiers used for this remote host: %s",
					peer_description.Value());
			}
		}

			// finally, add the mask we computed into the table with this IP addr
			add_hash_entry(sin6_addr, who, mask);			
	}  // end of if find_match is FALSE

		// decode the mask and return True or False to the user.
	if ( mask & deny_mask(perm) ) {
		return USER_AUTH_FAILURE;
	}

	if ( mask & allow_mask(perm) ) {
		return USER_AUTH_SUCCESS;
	}

	return USER_AUTH_FAILURE;
}

bool
IpVerify::lookup_user_ip_allow(DCpermission perm, char const *user, char const *ip)
{
	PermTypeEntry *permentry = PermTypeArray[perm];
	return lookup_user(permentry->allow_hosts,permentry->allow_users,user,ip,NULL,true);
}

bool
IpVerify::lookup_user_ip_deny(DCpermission perm, char const *user, char const *ip)
{
	PermTypeEntry *permentry = PermTypeArray[perm];
	return lookup_user(permentry->deny_hosts,permentry->deny_users,user,ip,NULL,false);
}

bool
IpVerify::lookup_user_host_allow(DCpermission perm, char const *user, char const *hostname)
{
	PermTypeEntry *permentry = PermTypeArray[perm];
	return lookup_user(permentry->allow_hosts,permentry->allow_users,user,NULL,hostname,true);
}

bool
IpVerify::lookup_user_host_deny(DCpermission perm, char const *user, char const *hostname)
{
	PermTypeEntry *permentry = PermTypeArray[perm];
	return lookup_user(permentry->deny_hosts,permentry->deny_users,user,NULL,hostname,false);
}

bool
IpVerify::lookup_user(NetStringList *hosts, UserHash_t *users, char const *user, char const *ip, char const *hostname, bool is_allow_list)
{
	if( !hosts || !users ) {
		return false;
	}
	ASSERT( user );

		// we look up by ip OR by hostname, not both
	ASSERT( !ip || !hostname );
	ASSERT( ip || hostname);

	StringList hostmatches;
	if( ip ) {
		hosts->find_matches_withnetwork(ip,&hostmatches);
	}
	else if( hostname ) {
		hosts->find_matches_anycase_withwildcard(hostname,&hostmatches);
	}

	char const * hostmatch;
	hostmatches.rewind();
	while( (hostmatch=hostmatches.next()) ) {
		StringList *userlist;
		ASSERT( users->lookup(hostmatch,userlist) != -1 );

		if (userlist->contains_anycase_withwildcard(user)) {
			dprintf ( D_SECURITY, "IPVERIFY: matched user %s from %s to %s list\n",
					  user, hostmatch, is_allow_list ? "allow" : "deny" );
			return true;
		}
	}

	return false;
}

// PunchHole - dynamically opens up a perm level to the
// given user / IP. The hole can be removed with FillHole.
// Additions persist across a reconfig.  This is intended
// for transient permissions (like to automatic permission
// granted to a remote startd host when a shadow starts up.)
//
bool
IpVerify::PunchHole(DCpermission perm, MyString& id)
{
	int count = 0;
	if (PunchedHoleArray[perm] == NULL) {
		PunchedHoleArray[perm] =
			new HolePunchTable_t(compute_host_hash);
		ASSERT(PunchedHoleArray[perm] != NULL);
	}
	else {
		int c;
		if (PunchedHoleArray[perm]->lookup(id, c) != -1) {
			count = c;
			if (PunchedHoleArray[perm]->remove(id) == -1) {
				EXCEPT("IpVerify::PunchHole: "
				           "table entry removal error");
			}
		}
	}

	count++;
	if (PunchedHoleArray[perm]->insert(id, count) == -1) {
		EXCEPT("IpVerify::PunchHole: table entry insertion error");
	}

	if (count == 1) {
		dprintf(D_SECURITY,
		        "IpVerify::PunchHole: opened %s level to %s\n",
		        PermString(perm),
		        id.Value());
	}
	else {
		dprintf(D_SECURITY,
		        "IpVerify::PunchHole: "
			    "open count at level %s for %s now %d\n",
		        PermString(perm),
		        id.Value(),
		        count);
	}

	DCpermissionHierarchy hierarchy( perm );
	DCpermission const *implied_perms=hierarchy.getImpliedPerms();
	for(; implied_perms[0] != LAST_PERM; implied_perms++ ) {
		if( perm != implied_perms[0] ) {
			PunchHole(implied_perms[0],id);
		}
	}

	return true;
}

// FillHole - plug up a dynamically punched authorization hole
//
bool
IpVerify::FillHole(DCpermission perm, MyString& id)
{
	HolePunchTable_t* table = PunchedHoleArray[perm];
	if (table == NULL) {
		return false;
	}

	int count;
	if (table->lookup(id, count) == -1) {
		return false;
	}
	if (table->remove(id) == -1) {
		EXCEPT("IpVerify::FillHole: table entry removal error");
	}

	count--;

	if (count != 0) {
		if (table->insert(id, count) == -1) {
			EXCEPT("IpVerify::FillHole: "
			           "table entry insertion error");
		}
	}

	if (count == 0) {
		dprintf(D_SECURITY,
		        "IpVerify::FillHole: "
		            "removed %s-level opening for %s\n",
		        PermString(perm),
		        id.Value());
	}
	else {
		dprintf(D_SECURITY,
		        "IpVerify::FillHole: "
		            "open count at level %s for %s now %d\n",
		        PermString(perm),
		        id.Value(),
		        count);
	}

	DCpermissionHierarchy hierarchy( perm );
	DCpermission const *implied_perms=hierarchy.getImpliedPerms();
	for(; implied_perms[0] != LAST_PERM; implied_perms++ ) {
		if( perm != implied_perms[0] ) {
			FillHole(implied_perms[0],id);
		}
	}

	return true;
}

IpVerify::PermTypeEntry::~PermTypeEntry() {
	if (allow_hosts)
		delete allow_hosts;
	if (deny_hosts)
		delete deny_hosts;
	if (allow_users) {
		MyString    key;
		StringList* value;
		allow_users->startIterations();
		while (allow_users->iterate(key, value)) {
			delete value;
		}
		delete allow_users;
	}
	if (deny_users) {
		MyString    key;
		StringList* value;
		deny_users->startIterations();
		while (deny_users->iterate(key, value)) {
			delete value;
		}
		delete deny_users;
	}
}


#ifdef WANT_STANDALONE_TESTING
#include "condor_io.h"
#ifdef WIN32
#	include <crtdbg.h>
   _CrtMemState s1, s2, s3;
#endif
int
main()
{
	char buf[50];
	char buf1[50];
	struct sockaddr_in sin;
	SafeSock ssock;
	IpVerify* userverify;

	set_mySubSystem( "COLLECTOR", SUBSYSTEM_TYPE_COLLECTOR );

	config();

#ifdef WIN32
	_CrtMemCheckpoint( &s1 );
#endif

	userverify = new IpVerify();

	userverify->Init();

	buf[0] = '\0';

	while( 1 ) {
		printf("Enter test:\n");
		scanf("%s",buf);
		if ( strncmp(buf,"exit",4) == 0 )
			break;
		if ( strncmp(buf,"reinit",6) == 0 ) {
			config();
			userverify->Init();
			continue;
		}
		printf("Verifying %s ... ",buf);
		sprintf(buf1,"<%s:1970>",buf);
		string_to_sin(buf1,&sin);
		if ( userverify->Verify(WRITE,&sin) == TRUE )
			printf("ALLOW\n");
		else
			printf("DENY\n");
	}
	
	delete userverify;

#ifdef WIN32
	_CrtMemCheckpoint( &s2 );
	// _CrtMemDumpAllObjectsSince( &s1 );
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
	if ( _CrtMemDifference( &s3, &s1, &s2 ) )
      _CrtMemDumpStatistics( &s3 );
	// _CrtDumpMemoryLeaks();	// report any memory leaks on Win32
#endif

	return TRUE;
}
#endif	// of WANT_STANDALONE_TESTING
