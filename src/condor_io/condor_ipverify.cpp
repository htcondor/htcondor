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

#include "sock.h"
#include "condor_secman.h"
#include "ipv6_hostname.h"
#include "condor_netaddr.h"
#include "store_cred.h"

// Externs to Globals

const char TotallyWild[] = "*";

#if defined(HAVE_INNETGR)
#include <netdb.h>
const std::string netgroup_detected = "***";
#endif

const char condor_at[] = "condor@";
const size_t condor_at_len = strlen(condor_at);
const char condor_pool_at[] = POOL_PASSWORD_USERNAME "@";
const size_t condor_pool_at_len = strlen(condor_pool_at);

// < operator for struct in6_addr, needed for std::map key
bool operator<(const struct in6_addr& a, const struct in6_addr& b) {
	return memcmp(&a, &b, sizeof(a)) < 0;
}

// Constructor
IpVerify::IpVerify() 
{
	did_init = FALSE;

	DCpermission perm;
	for (perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		PermTypeArray[perm] = NULL;
	}
}


// Destructor
IpVerify::~IpVerify()
{
	// Clear the Permission Type Array and Punched Hole Array
	DCpermission perm;
	for (perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		if ( PermTypeArray[perm] )
			delete PermTypeArray[perm];
	}	
}


int
IpVerify::Init()
{
	if (did_init) {
		return TRUE;
	}
	char *pAllow = NULL, *pDeny = NULL;
	DCpermission perm;
	auto ssinfo = get_mySubSystem();
	const char* const ssysname = ssinfo->getName();
	bool client_only = ssinfo->isType(SUBSYSTEM_TYPE_TOOL) || ssinfo->isType(SUBSYSTEM_TYPE_SUBMIT);

	did_init = TRUE;

	// Make sure that perm_mask_t is big enough to hold all possible
	// results of allow_mask() and deny_mask().
	ASSERT( sizeof(perm_mask_t)*8 - 2 > LAST_PERM );

	// Clear the Permission Hash Table in case re-initializing
	PermHashTable.clear();

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
		std::string allow_param, deny_param;

			// to avoid unneccesary DNS activity, the TOOL and SUBMIT
			// subsystems only load the CLIENT lists, since they have no
			// command port and don't need the other authorization lists.
		if ((perm == CLIENT_PERM) || ! client_only) {
			pAllow = SecMan::getSecSetting("ALLOW_%s",perm,&allow_param, ssysname );
			pDeny = SecMan::getSecSetting("DENY_%s",perm,&deny_param, ssysname );
		}
		if (IsDebugVerbose(D_SECURITY)) {
			if (pAllow) {
				dprintf(D_SECURITY|D_VERBOSE, "IPVERIFY: allow %s: %s (from config value %s)\n", PermString(perm),pAllow,allow_param.c_str());
			}
			if(pDeny) {
				dprintf(D_SECURITY|D_VERBOSE, "IPVERIFY: deny %s: %s (from config value %s)\n", PermString(perm),pDeny,deny_param.c_str());
			}
		}

		// Treat "*" or "*/*" specially, because that's an optimized default.
		bool allowAll = pAllow && (!strcmp(pAllow, "*") || !strcmp(pAllow, "*/*"));
		bool denyAll = pDeny && (!strcmp(pDeny, "*") || !strcmp(pDeny, "*/*"));

		// Optimized cases
		if (perm == ALLOW) {
			pentry->behavior = USERVERIFY_ALLOW;
		} else if (denyAll || (!pAllow && (perm != READ && perm != WRITE))) { // Deny everyone
			// READ or WRITE may be implicitly allowed by other permissions
			pentry->behavior = USERVERIFY_DENY;
			if ( ! client_only) dprintf( D_SECURITY|D_VERBOSE, "ipverify: %s optimized to deny everyone\n", PermString(perm) );
		} else if (allowAll) {
			if (!pDeny) { // Allow anyone
				pentry->behavior = USERVERIFY_ALLOW;
				if ( ! client_only) dprintf( D_SECURITY|D_VERBOSE, "ipverify: %s optimized to allow anyone\n", PermString(perm) );
			} else {
				pentry->behavior = USERVERIFY_ONLY_DENIES;
				fill_table( pentry, pDeny, false );
			}
		}

		// Only load table entries if necessary
		if (pentry->behavior == USERVERIFY_USE_TABLE) {
			if ( pAllow ) {
				fill_table( pentry, pAllow, true );
			}
			if ( pDeny ) {
				fill_table( pentry, pDeny, false );
			}
		}

		// Free up strings for next time around the loop
		if (pAllow) {
			free(pAllow);
			pAllow = NULL;
		}
		if (pDeny) {
			free(pDeny);
			pDeny = NULL;
		}
	}

	int dpf_level = D_SECURITY | (client_only ? D_VERBOSE : 0);
	if (IsDebugCatAndVerbosity(dpf_level)) {
		dprintf(dpf_level,"Initialized the following authorization table:\n");
		PrintAuthTable(dpf_level);
	}
	return TRUE;
}

bool
IpVerify::LookupCachedVerifyResult( DCpermission perm, const struct in6_addr &sin6, const char * user, perm_mask_t & mask)
{
	ASSERT(user && *user);

	auto host_itr = PermHashTable.find(sin6);
	if (host_itr != PermHashTable.end()) {

		auto user_itr = host_itr->second.find(user);
		if (user_itr != host_itr->second.end()) {
			mask = user_itr->second;

				// We do not want to return true unless there is
				// a cached result for this specific perm level.

			if( mask & (allow_mask(perm)|deny_mask(perm)) ) {
				return true;
			}
		}
	}
	return false;
}

void
IpVerify::add_hash_entry(const struct in6_addr & sin6_addr, const char * user, perm_mask_t new_mask)
{
	ASSERT(user && *user);

	PermHashTable[sin6_addr][user] |= new_mask;

	if( IsFulldebug(D_FULLDEBUG) || IsDebugLevel(D_SECURITY) ) {
		std::string auth_str;
		AuthEntryToString(sin6_addr,user,new_mask, auth_str);
		dprintf(D_FULLDEBUG|D_SECURITY,
				"Adding to resolved authorization table: %s\n",
				auth_str.c_str());
	}
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
IpVerify::PermMaskToString(perm_mask_t mask, std::string &mask_str)
{
	DCpermission perm;
	for(perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		if( mask & allow_mask(perm) ) {
			if(!mask_str.empty()) {
				mask_str += ',';
			}
			mask_str += PermString(perm);
		}
		if( mask & deny_mask(perm) ) {
			if(!mask_str.empty()) {
				mask_str += ',';
			}
			mask_str += "DENY_";
			mask_str += PermString(perm);
		}
	}
}

void
IpVerify::UserHashToString(UserHash_t &user_hash, std::string &result)
{
	for (auto& [host, users] : user_hash) {
		for (auto& user : users) {
			formatstr_cat(result, " %s/%s", user.c_str(), host.c_str());
		}
	}
}

void
IpVerify::AuthEntryToString(const in6_addr & host, const char * user, perm_mask_t mask, std::string &result)
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

	std::string mask_str;
	PermMaskToString( mask, mask_str );
	formatstr(result, "%s/%s: %s", /* NOTE: this does not need a '\n', all the call sites add one. */
			user ? user : "(null)",
			buf,
			mask_str.c_str() );
}

void
IpVerify::PrintAuthTable(int dprintf_level) {

	for (auto& [host, ptable] : PermHashTable) {

		for (auto& [userid, mask] : ptable) {

			std::string auth_entry_str;
			AuthEntryToString(host,userid.c_str(),mask, auth_entry_str);
			dprintf(dprintf_level,"%s\n", auth_entry_str.c_str());
		}
	}

	dprintf(dprintf_level,"Authorizations yet to be resolved:\n");
	DCpermission perm;
	for ( perm=FIRST_PERM; perm < LAST_PERM; perm=NEXT_PERM(perm) ) {

		PermTypeEntry* pentry = PermTypeArray[perm];
		ASSERT( pentry );

		std::string allow_users,deny_users;

		UserHashToString(pentry->allow_users,allow_users);

		UserHashToString(pentry->deny_users,deny_users);

		if( allow_users.length() ) {
			dprintf(dprintf_level,"allow %s: %s\n",
					PermString(perm),
					allow_users.c_str());
		}

		if( deny_users.length() ) {
			dprintf(dprintf_level,"deny %s: %s\n",
					PermString(perm),
					deny_users.c_str());
		}
	}
}

static void
ExpandHostAddresses( char const * entry, std::vector<std::string>& list )
{
	//
	// What we're actually passed are entries in a security list.  An entry
	// may be: an IP literal, a network literal or wildcard, a hostname
	// literal or wildcard, a fully-qualified HTCondor user name (as resulting
	// from the unified map file and including a '/'), or a (semi-)sinful
	// string.
	//
	// Nothing aside from a sinful allows '?', but we do have to check for
	// <host|ip>:<port> semi-sinful strings.
	//
	// It's not all clear that it's wise to include entries that we don't
	// recognize in the list, but for backwards-compatibility, we'll
	// try it.
	//
	list.emplace_back( entry );

	//
	// Because we allow hostnames in sinfuls, and allow unbracketed sinfuls,
	// the sinful parser is considerably more lax than I would like.  Detect
	// wildcards and FQUNs first; if the entry is wildcarded or a FQUN,
	// we're done.  (Determining if the entry is valid must be done later.)
	//
	// As far as I know, valid original and v1 Sinfuls don't use either of
	// these characters in their serialization.  This is something of a gotcha
	// waiting to happen, but doing things in the other order would require
	// doing DNS lookups during Sinful parsing, which is probably a Bad Idea.
	//
	if( strchr( entry, '*' ) || strchr( entry, '/' ) ) {
		return;
	}

	//
	// If it's a valid network specifier, we're done.  This includes IPv4
	// and IPv6 literals (which makes the ':' in the next test safe).
	//
	condor_netaddr netaddr;
	if( netaddr.from_net_string( entry ) ) {
		return;
	}

	//
	// If it's a Sinful string, we're done.
	//
	if(	   strchr( entry, '<' ) || strchr( entry, '>' )
		|| strchr( entry, '?' ) || strchr( entry, ':' ) ) {
		dprintf( D_ALWAYS, "WARNING: Not attempting to resolve '%s' from the "
			"security list: it looks like a Sinful string.  A Sinful string "
			"specifies how to contact a daemon, but not which address it "
			"uses when contacting others.  Use the bare hostname of the "
			"trusted machine, or an IP address (if known and unique).\n",
			entry );
		return;
	}

	std::vector<condor_sockaddr> addrs = resolve_hostname(entry);
	for (auto& addr : addrs) {
		list.emplace_back(addr.to_ip_string());
	}
}

void
IpVerify::fill_table(PermTypeEntry * pentry, char * list, bool allow)
{
    assert(pentry);

	std::string host;
	std::string user;
	std::string alt_user;
	bool use_pool_username_equiv = param_boolean("USE_POOL_USERNAME_EQUIVALENT", true);
	for (auto& entry : StringTokenIterator(list)) {
		if (entry.empty()) {
			// empty string?
			continue;
		}
		split_entry(entry.c_str(), host, user);

		alt_user.clear();
		if (use_pool_username_equiv) {
			// Add reciprocal entries for condor@... and condor_pool@...
			if (strncasecmp(user.c_str(), condor_at, condor_at_len) == 0) {
				alt_user = condor_pool_at + user.substr(condor_at_len);
			} else if (strncasecmp(user.c_str(), condor_pool_at, condor_pool_at_len) == 0) {
				alt_user = condor_at + user.substr(condor_pool_at_len);
			}
		}

#if defined(HAVE_INNETGR)
        if (netgroup_detected == user) {
            if (allow) {
                pentry->allow_netgroups.push_back(host);
            } else {
                pentry->deny_netgroups.push_back(host);
            }
            continue;
        }
#endif

			// If this is a hostname, get all IP addresses for it and
			// add them to the list.  This ensures that if we are given
			// a cname, we do the right thing later when trying to match
			// this record with the official hostname.
		std::vector<std::string> host_addrs;
		ExpandHostAddresses(host.c_str(), host_addrs);

		for (auto& host_addr : host_addrs) {
				// add user to user hash under host key
			if (allow) {
				pentry->allow_users[host_addr].emplace_back(user);
				if (!alt_user.empty()) {
					pentry->allow_users[host_addr].emplace_back(alt_user);
				}
			} else {
				pentry->deny_users[host_addr].emplace_back(user);
				if (!alt_user.empty()) {
					pentry->deny_users[host_addr].emplace_back(alt_user);
				}
			}
		}
	}
}

void IpVerify :: split_entry(const char * perm_entry, std::string& host, std::string& user)
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

#if defined(HAVE_INNETGR)
    // detect a netgroup entry
    // netgroup entries are of the form '+<groupname>', and may embody
    // information about both hosts and users
    if (permbuf[0] == '+') {
        user = netgroup_detected;
        host = (1+permbuf);
        free(permbuf);
        return;
    }
#endif

    slash0 = strchr(permbuf, '/');
	if (!slash0) {
		at = strchr(permbuf, '@');
		if (at) {
			user = permbuf;
			host = '*';
		} else {
			user = '*';

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
			host = permbuf;
		}
	} else {
		// okay, there was one slash... look for another
		slash1 = strchr(slash0 + 1, '/');
		if (slash1) {
			// form is user/net/mask
			*slash0++ = 0;
			user = permbuf;
			host = slash0;
		} else {
			// could be either user/host or net/mask
			// handle */x case now too
			at = strchr(permbuf, '@');
			if ((at && at < slash0) || permbuf[0] == '*') {
				*slash0++ = 0;
				user = permbuf;
				host = slash0;
			} else {
				condor_netaddr netaddr;
				if (netaddr.from_net_string(permbuf)) {
					user = '*';
					host = permbuf;
				} else {
					dprintf (D_SECURITY, "IPVERIFY: warning, strange entry %s\n", permbuf);
					*slash0++ = 0;
					user = permbuf;
					host = slash0;
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
IpVerify::Verify( DCpermission perm, const condor_sockaddr& addr, const char * user, std::string & allow_reason, std::string & deny_reason )
{
	perm_mask_t mask;
	in6_addr sin6_addr;
	const char *thehost;
    const char * who = user;
	std::string peer_description; // we build this up as we go along (DNS etc.)

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
		EXCEPT("IpVerify::Verify: called with unknown permission %d",perm);
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
	if ( !PunchedHoleArray[perm].empty() ) {
		HolePunchTable_t& hpt = PunchedHoleArray[perm];
		std::string ip_str_buf = addr.to_ip_string();
		const char* ip_str = ip_str_buf.c_str();
		std::string id_with_ip;
		std::string id;
		if ( who != TotallyWild ) {
			formatstr(id_with_ip, "%s/%s", who, ip_str);
			id = who;
			if ( hpt.find(id) != hpt.end() ) {
				formatstr( allow_reason, 
						"%s authorization has been made automatic for %s",
						PermString(perm), id.c_str() );
				return USER_AUTH_SUCCESS;
			}
			if ( hpt.find(id_with_ip) != hpt.end() ) {
				formatstr( allow_reason,
						"%s authorization has been made automatic for %s",
						PermString(perm), id_with_ip.c_str() );
				return USER_AUTH_SUCCESS;
			}
		}
		id = ip_str;
		if ( hpt.find(id) != hpt.end() ) {
			formatstr( allow_reason,
					"%s authorization has been made automatic for %s",
					PermString(perm), id.c_str() );
			return USER_AUTH_SUCCESS;
		}
	}

	if ( PermTypeArray[perm]->behavior == USERVERIFY_ALLOW ) {
			// allow if no ALLOW_* or DENY_* restrictions 
			// specified.
		formatstr( allow_reason,
				"%s authorization policy allows access by anyone",
				PermString(perm));
		return USER_AUTH_SUCCESS;
	}
		
	if ( PermTypeArray[perm]->behavior == USERVERIFY_DENY ) {
			// deny
		formatstr(deny_reason,
				"%s authorization policy denies all access",
				PermString(perm));
		return USER_AUTH_FAILURE;
	}
		
	if( LookupCachedVerifyResult(perm,sin6_addr,who,mask) ) {
		if( mask & deny_mask(perm) ) {
			formatstr(deny_reason,
				"cached result for %s; see first case for the full reason",
				PermString(perm));
		}
		else if( mask & allow_mask(perm) ) {
			formatstr(allow_reason,
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
			formatstr(deny_reason,
					"%s authorization policy denies IP address %s",
					PermString(perm), addr.to_ip_string().c_str() );
		}

		if ( !(mask&allow_resolved) && lookup_user_ip_allow(perm,who,ipstr)) {
			mask |= allow_mask(perm);
			formatstr( allow_reason,
					"%s authorization policy allows IP address %s",
					PermString(perm), addr.to_ip_string().c_str() );
		}


		std::vector<std::string> hostnames;
		// now scan through hostname strings
		if( !(mask&allow_resolved) || !(mask&deny_resolved) ) {
			hostnames = get_hostname_with_alias(addr);
		}

		for (unsigned int i = 0; i < hostnames.size(); ++i) {
			thehost = hostnames[i].c_str();
			if (!peer_description.empty()) {
				peer_description += ',';
			}
			peer_description += thehost;

			if ( !(mask&deny_resolved) && lookup_user_host_deny(perm,who,thehost) ) {
				mask |= deny_mask(perm);
				formatstr(deny_reason,
						"%s authorization policy denies hostname %s",
						PermString(perm), thehost );
			}

			if ( !(mask&allow_resolved) && lookup_user_host_allow(perm,who,thehost) ) {
				mask |= allow_mask(perm);
				formatstr( allow_reason,
						"%s authorization policy allows hostname %s",
						PermString(perm), thehost );
			}
		}
			// if we found something via our hostname or subnet mactching, we now have 
			// a mask, and we should add it into our table so we need not
			// do a gethostbyaddr() next time.  if we still do not have a mask
			// (perhaps because this host doesn't appear in any list), create one
			// and then add to the table.
			// But first, check our parent permission levels in the
			// authorization hierarchy.
			// DAEMON and ADMINISTRATOR imply WRITE.
			// WRITE, NEGOTIATOR, and CONFIG_PERM imply READ.
		bool determined_by_parent = false;
		if ( mask == 0 ) {
			if ( PermTypeArray[perm]->behavior == USERVERIFY_ONLY_DENIES ) {
				dprintf(D_SECURITY|D_VERBOSE,"IPVERIFY: %s at %s not matched to deny list, so allowing.\n",who, addr.to_sinful().c_str());
				formatstr(allow_reason,
						"%s authorization policy does not deny, so allowing",
						PermString(perm));

				mask |= allow_mask(perm);
			} else {
				bool parent_allowed = false;
				for (auto parent_perm : DCpermissionHierarchy::DirectlyImpliedBy(perm)) {
					if( Verify( parent_perm, addr, user, allow_reason, deny_reason ) == USER_AUTH_SUCCESS ) {
						determined_by_parent = true;
						parent_allowed = true;
						dprintf(D_SECURITY|D_VERBOSE,"IPVERIFY: allowing %s at %s for %s because %s is allowed\n",who, addr.to_sinful().c_str(),PermString(perm),PermString(parent_perm));
						std::string tmp = allow_reason;
						formatstr(allow_reason,
								"%s is implied by %s; %s",
								PermString(perm),
								PermString(parent_perm),
								tmp.c_str());
						break;
					}
				}
				if( parent_allowed ) {
					mask |= allow_mask(perm);
				}
				else {
					mask |= deny_mask(perm);

					if( !determined_by_parent ) {
							// We don't just allow anyone, and this request
							// did not match any of the entries we do allow.
							// In case the reason we didn't match is
							// because of a typo or a DNS problem, record
							// all the hostnames we searched for.
						formatstr(deny_reason,
							"%s authorization policy contains no matching "
							"ALLOW entry for this request"
							"; identifiers used for this host: %s, hostname size = %lu, "
							"original ip address = %s",
							PermString(perm),
							peer_description.c_str(),
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
			if( !peer_description.empty() ) {
				formatstr_cat(allow_reason,
					"; identifiers used for this remote host: %s",
					peer_description.c_str());
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
	return lookup_user(permentry->allow_users,permentry->allow_netgroups,user,ip,NULL,true);
}

bool
IpVerify::lookup_user_ip_deny(DCpermission perm, char const *user, char const *ip)
{
	PermTypeEntry *permentry = PermTypeArray[perm];
	return lookup_user(permentry->deny_users,permentry->deny_netgroups,user,ip,NULL,false);
}

bool
IpVerify::lookup_user_host_allow(DCpermission perm, char const *user, char const *hostname)
{
	PermTypeEntry *permentry = PermTypeArray[perm];
	return lookup_user(permentry->allow_users,permentry->allow_netgroups,user,NULL,hostname,true);
}

bool
IpVerify::lookup_user_host_deny(DCpermission perm, char const *user, char const *hostname)
{
	PermTypeEntry *permentry = PermTypeArray[perm];
	return lookup_user(permentry->deny_users,permentry->deny_netgroups,user,NULL,hostname,false);
}

bool
IpVerify::lookup_user(UserHash_t& users, netgroup_list_t& netgroups, char const *user, char const *ip, char const *hostname, bool is_allow_list)
{
	if( users.empty() && netgroups.empty() ) {
		return false;
	}
	ASSERT( user );

		// we look up by ip OR by hostname, not both
	ASSERT( !ip || !hostname );
	ASSERT( ip || hostname);

	for (auto& [host_key, userlist] : users) {
		bool host_matches = false;
		if (ip) {
			host_matches = matches_withnetwork(host_key, ip);
		} else {
			host_matches = matches_anycase_withwildcard(host_key.c_str(), hostname);
		}

		if (host_matches && contains_anycase_withwildcard(userlist, user)) {
			dprintf ( D_SECURITY|D_VERBOSE, "IPVERIFY: matched user %s from %s to %s list\n",
					  user, host_key.c_str(), is_allow_list ? "allow" : "deny" );
			return true;
		}
	}

#if defined(HAVE_INNETGR)
    std::string canonical(user);
    std::string::size_type atpos = canonical.find_first_of('@');
    std::string username = canonical.substr(0, atpos);
    std::string domain = canonical.substr(1+atpos);
    std::string host = hostname?hostname:ip;

    for (netgroup_list_t::iterator grp(netgroups.begin());  grp != netgroups.end();  ++grp) {
        if (innetgr(grp->c_str(), host.c_str(), username.c_str(), domain.c_str())) {
            dprintf(D_SECURITY, "IPVERIFY: matched canonical user %s@%s/%s to netgroup %s on %s list\n",
                    username.c_str(), domain.c_str(), host.c_str(), grp->c_str(), is_allow_list ? "allow" : "deny");
            return true;
        }
    }
#endif

	return false;
}

// PunchHole - dynamically opens up a perm level to the
// given user / IP. The hole can be removed with FillHole.
// Additions persist across a reconfig.  This is intended
// for transient permissions (like to automatic permission
// granted to a remote startd host when a shadow starts up.)
//
bool
IpVerify::PunchHole(DCpermission perm, const std::string& id)
{
	int count = ++PunchedHoleArray[perm][id];
	if (count == 1) {
		dprintf(D_SECURITY|D_VERBOSE,
		        "IpVerify::PunchHole: opened %s level to %s\n",
		        PermString(perm),
		        id.c_str());
	}
	else {
		dprintf(D_SECURITY|D_VERBOSE,
		        "IpVerify::PunchHole: "
			    "open count at level %s for %s now %d\n",
		        PermString(perm),
		        id.c_str(),
		        count);
	}

#if 1
	// recursively iterate and puch holes for implied perms
	DCpermission implied_perm = DCpermissionHierarchy::nextImplied(perm);
	if (implied_perm < LAST_PERM && implied_perm != perm) {
		PunchHole(implied_perm,id);
	}
#else // this is what the pre 23.x code did...
	// iteratively and recursively puch holes for implied perms
	DCpermission implied_perm = perm;
	for ( ; implied_perm < LAST_PERM; implied_perm = DCpermissionHierarchy::nextImplied(implied_perm)) {
		if (perm != implied_perm) {
			PunchHole(implied_perm,id);
		}
	}
#endif

	return true;
}

// FillHole - plug up a dynamically punched authorization hole
//
bool
IpVerify::FillHole(DCpermission perm, const std::string& id)
{
	auto itr = PunchedHoleArray[perm].find(id);
	if (itr == PunchedHoleArray[perm].end()) {
		return false;
	}

	if (itr->second > 0) {
		itr->second--;
	}

	if (itr->second <= 0) {
		dprintf(D_SECURITY|D_VERBOSE,
		        "IpVerify::FillHole: "
		            "removed %s-level opening for %s\n",
		        PermString(perm),
		        id.c_str());
		PunchedHoleArray[perm].erase(itr);
	}
	else {
		dprintf(D_SECURITY|D_VERBOSE,
		        "IpVerify::FillHole: "
		            "open count at level %s for %s now %d\n",
		        PermString(perm),
		        id.c_str(),
		        itr->second);
	}

#if 1
	// recursively iterate and puch holes for implied perms
	DCpermission implied_perm = DCpermissionHierarchy::nextImplied(perm);
	if (implied_perm < LAST_PERM && implied_perm != perm) {
		FillHole(implied_perm,id);
	}
#else // this is what the pre 23.x code did.
	// iteratively and recursively puch holes for implied perms
	DCpermission implied_perm = perm;
	for ( ; implied_perm < LAST_PERM; implied_perm = DCpermissionHierarchy::nextImplied(implied_perm)) {
		if (perm != implied_perm) {
			FillHole(implied_perm,id);
		}
	}
#endif

	return true;
}

