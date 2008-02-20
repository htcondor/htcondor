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

#include "HashTable.h"
#include "sock.h"
#include "condor_secman.h"

// Externs to Globals
extern char* mySubSystem;	// the subsys ID, such as SCHEDD, STARTD, etc. 

const char TotallyWild[] = "*";


// Hash function for Permission hash table
static unsigned int
compute_perm_hash(const struct in_addr &in_addr)
{
	const unsigned int h = *((const unsigned int *)&in_addr);
	return h;
}

// Hash function for AllowHosts hash table
static unsigned int
compute_host_hash( const MyString & str )
{
	return ( str.Hash() );
}

// == operator for struct in_addr, also needed for hash table template
bool operator==(const struct in_addr a, const struct in_addr b) {
	return a.s_addr == b.s_addr;
}

// Constructor
IpVerify::IpVerify() 
{
	did_init = FALSE;

	DCpermission perm;
	for (perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		PermTypeArray[perm] = NULL;
	}

	cache_DNS_results = TRUE;

	PermHashTable = new PermHashTable_t(797, compute_perm_hash);
	AllowHostsTable = new HostHashTable_t(53, compute_host_hash);
}


// Destructor
IpVerify::~IpVerify()
{

	// Clear the Permission Hash Table
	if (PermHashTable) {
		// iterate through the table and delete the entries
		struct in_addr key;
		UserPerm_t * value;
		PermHashTable->startIterations();

		while (PermHashTable->iterate(key, value)) {
			delete value;
		}

		delete PermHashTable;
	}

	if( AllowHostsTable ) { 
		delete AllowHostsTable;
	}

	// Clear the Permission Type Array
	DCpermission perm;
	for (perm=FIRST_PERM; perm<LAST_PERM; perm=NEXT_PERM(perm)) {
		if ( PermTypeArray[perm] )
			delete PermTypeArray[perm];
	}
}


int
IpVerify::Init()
{
	char *pAllow, *pDeny, *pOldAllow, *pOldDeny, *pNewAllow = NULL, *pNewDeny = NULL;
	DCpermission perm;
	
	did_init = TRUE;

		// Make sure that perm_mask_t is big enough to hold all possible
		// results of allow_mask() and deny_mask().
	ASSERT( sizeof(perm_mask_t)*8 - 2 > LAST_PERM );

	// Clear the Permission Hash Table in case re-initializing
	if (PermHashTable) {
		// iterate through the table and delete the entries
		struct in_addr key;
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

		pNewAllow = SecMan::getSecSetting("ALLOW_%s",perm,&allow_param,mySubSystem);

        // This is the old stuff, eventually it will be gone
		pOldAllow = SecMan::getSecSetting("HOSTALLOW_%s",perm,&allow_param,mySubSystem);

        // Treat a "*", "*/*" for USERALLOW_XXX as if it's just
        // undefined. 
		if( pNewAllow && (!strcmp(pNewAllow, "*") 
                          || !strcmp(pNewAllow, "*/*") ) ) {
			free( pNewAllow );
			pNewAllow = NULL;
		}
        
        if (pOldAllow && (!strcmp(pOldAllow, "*"))) {
            free( pOldAllow );
            pOldAllow = NULL;
        }
        
		// concat the two
		pAllow = merge(pNewAllow, pOldAllow);

		pNewDeny = SecMan::getSecSetting("DENY_%s",perm,&deny_param,mySubSystem);

		pOldDeny = SecMan::getSecSetting("HOSTDENY_%s",perm,&deny_param,mySubSystem);

		// concat the two
		pDeny = merge(pNewDeny, pOldDeny);

		if( pAllow ) {
			dprintf ( D_SECURITY, "IPVERIFY: allow %s : %s (from config value %s)\n", PermString(perm),pAllow,allow_param.Value());
		}
		if( pDeny ) {
			dprintf ( D_SECURITY, "IPVERIFY: deny %s: %s (from config value %s)\n", PermString(perm),pDeny,deny_param.Value());
		}

		if ( !pAllow && !pDeny ) {
			if (perm == CONFIG_PERM) { 	  // deny all CONFIG requests 
				pentry->behavior = USERVERIFY_DENY; // by default
			} else {
				pentry->behavior = USERVERIFY_ALLOW;
			}
		} else {
			if ( pDeny && !pAllow ) {
				pentry->behavior = USERVERIFY_ONLY_DENIES;
			} else {
				pentry->behavior = USERVERIFY_USE_TABLE;
			}
			if ( pAllow ) {
                fill_table( pentry, allow_mask(perm), pAllow, true );
				free(pAllow);
                pAllow = NULL;
			}
			if ( pDeny ) {
				fill_table( pentry,	deny_mask(perm), pDeny, false );
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

    // Finally, check to see if we have an allow hosts that have
    // been added manually, in which case, re-add those.
	process_allow_users();

	dprintf(D_FULLDEBUG|D_SECURITY,"Initialized the following authorization table:\n");
	PrintAuthTable(D_FULLDEBUG|D_SECURITY);

	return TRUE;
}

char * IpVerify :: merge(char * pNewList, char * pOldList)
{
    char * pList = NULL;

    if (pOldList) {
        if (pNewList) {
            pList = (char *)malloc(strlen(pOldList) + strlen(pNewList) + 2);
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

bool IpVerify :: has_user(UserPerm_t * perm, const char * user, perm_mask_t & mask)
{
    // Now, let's see if the user is there
    int found_user = -1;
    int found_wild = -1;
    perm_mask_t user_mask = 0;
    perm_mask_t wild_mask = 0;
    MyString user_key;
    assert(perm);

    user_key = TotallyWild;
    found_wild = perm->lookup(user_key, wild_mask);

    if( user && *user && strcmp(user,TotallyWild)!=0 ) {
        user_key = user;
        found_user = perm->lookup(user_key, user_mask);
    }

    mask = user_mask | wild_mask;
    return found_wild != -1 || found_user != -1;
}   

int
IpVerify::add_hash_entry(const struct in_addr & sin_addr, const char * user, perm_mask_t new_mask)
{
    UserPerm_t * perm = NULL;
    perm_mask_t old_mask = 0;  // must init old_mask to zero!!!
    MyString user_key = user;

	// assert(PermHashTable);
	if ( PermHashTable->lookup(sin_addr, perm) != -1 ) {
		// found an existing entry.  

		if (has_user(perm, user, old_mask)) {
            // hueristic: if the mask already contains what we
            // want, we are done.
            if ( (old_mask & new_mask) == new_mask ) {
                return TRUE;
            }

            // remove it because we are going to edit the mask below
            // and re-insert it.
            perm->remove(user_key);
        }
	}
    else {
        perm = new UserPerm_t(42, compute_host_hash);
        if (PermHashTable->insert(sin_addr, perm) != 0) {
            delete perm;
            return FALSE;
        }
    }

    perm->insert(user_key, old_mask | new_mask);

    return TRUE;
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
			mask_str.append_to_list("DENY(");
			mask_str += PermString(perm);
			mask_str += ")";
		}
	}
}

void
IpVerify::PrintAuthTable(int dprintf_level) {
	struct in_addr host;
	UserPerm_t * ptable;
	PermHashTable->startIterations();

	while (PermHashTable->iterate(host, ptable)) {
		MyString userid;
		perm_mask_t mask;

		ptable->startIterations();
		while( ptable->iterate(userid,mask) ) {
				// Call has_user() to get the full mask, including user=*.
			has_user(ptable, userid.Value(), mask);

			MyString mask_str;
			PermMaskToString( mask, mask_str );
			dprintf(dprintf_level,"host %s: user %s: %s\n",
					inet_ntoa(host),
					userid.Value(),
					mask_str.Value() );
		}
	}
}

// The form of the addr is: joe@some.domain.name/host.name
// For example, joe@cs.wisc.edu/perdita.cs.wisc.edu
//              joe@cs.wisc.edu/*.cs.wisc.edu    -- wild
//              joe@cs.wisc.edu/*                -- wild
//              */*.cs.wisc.edu                  -- wild
//              */perdita.cs.wisc.edu            
//              *@cs.wisc.edu/*                  -- wild
//              *@cs.wisc.edu/*.cs.wisc.edu      -- wild
//              *@cs.wisc.edu/perdita.cs.wisc.edu-- wild
//              * and */* are considered as invalid and are discarded earlier
//
// This returns true if we successfully added the given entry to the
// hash table.  If we didn't, it's because the addr we were passed
// contains a wildcard, in which case we want to keep that in the
// string list.
bool
IpVerify::add_host_entry( const char* addr, perm_mask_t mask ) 
{
	struct hostent	*hostptr;
	struct in_addr	sin_addr;
	bool			result = true;	
    char  * host, * user;

	if (!addr || !*addr) {
		// returning true means we dealt with it, and
		// it will be deleted from parent list.  we aren't
		// actually dealing with it, because we shouldn't
		// be getting null here.
		return true;
	}

	split_entry(addr, &host, &user);

	// the host may be in netmask form:
	// a.b.c.d/len or a.b.c.d/m.a.s.k
	// is_ipaddr will still return false for those

	if ( strchr(user,'*') && strcmp(user,"*") ) {
		// Refuse usernames that contain wildcards.
		// The only exception is '*', which AddAllowHost likes to put
		// directly into the hashtable.
		result = false;
	} else if( is_ipaddr(host,&sin_addr) == TRUE ) {
        // This address is an IP addr in numeric form.
        // Add it into the hash table.
        add_hash_entry(sin_addr, user, mask);
    } else {
		// Could be a hostname or netmask.  a netmask will have a slash in it.
		// otherwise, This address is a hostname.  If it has no wildcards,
		// resolve to and IP address here, add it to our hashtable, and remove
		// the entry (i.e. treat it just like an IP addr).  If it has
		// wildcards or a slash, then leave it in this StringList.
		if ( strchr(host,'/') ) {
			// is a netmask (a form of wildcard really)
			result = false;
   		} else if ( strchr(host,'*') ) {
            // Contains wildcards, do nothing, and return false so
            // we leave the host in the string list
            result = false;
        } else {
            // No wildcards; resolve the name
			if ( (hostptr=condor_gethostbyname(host)) != NULL) {
				bool added = false;
				if ( hostptr->h_addrtype == AF_INET ) {
					for (int i=0; hostptr->h_addr_list[i]; i++) {
						add_hash_entry( (*(struct in_addr *)
                                         (hostptr->h_addr_list[i])),
                                        user, mask );
						added = true;
					}   
				}
				if ( added && (DebugFlags & D_FULLDEBUG) ) {
					MyString mask_str;
					PermMaskToString( mask, mask_str );
					dprintf (D_SECURITY, "IPVERIFY: successfully resolved and added %s to %s\n", host, mask_str.Value());
				}
			} else {
				dprintf (D_ALWAYS, "IPVERIFY: unable to resolve IP address of %s\n", host);
			}
		}
	}	
    if (host) {
        free(host);
    }
    if (user) {
        free(user);
    }
	return result;
}


void
IpVerify::fill_table(PermTypeEntry * pentry, perm_mask_t mask, char * list, bool allow)
{
    NetStringList * whichHostList = NULL;
    UserHash_t * whichUserHash = NULL;
    StringList * slist;

    assert(pentry);

    if (whichHostList == NULL) {
        whichHostList = new NetStringList();
    }
    if (whichUserHash == NULL) {
        whichUserHash = new UserHash_t(1024, compute_host_hash);
    }

    slist = new StringList(list);
	char *entry, * host, * user;
	slist->rewind();
	while ( (entry=slist->next()) ) {
		if (!*entry) {
			// empty string?
			slist->deleteCurrent();
		} else if( add_host_entry(entry, mask) ) {
			// it was dealt with, and should be removed
            slist->deleteCurrent();
		} else {
            split_entry(entry, &host, &user);
            MyString hostString(host);
            StringList * userList = 0;
            // add user to user hash, host to host list
            if (whichUserHash->lookup(hostString, userList) == -1) {
                whichUserHash->insert(hostString, new StringList(user)); 
                whichHostList->append(host);
            }
            else {
                userList->append(user);
            }
            if (host) {
                free(host);
            }
            if (user) {
                free(user);
            }
        }
	}

    if (allow) {
        pentry->allow_hosts = whichHostList;
        pentry->allow_users  = whichUserHash;
    }
    else {
        pentry->deny_hosts = whichHostList;
        pentry->deny_users = whichUserHash;
    }
    delete slist;
}

void IpVerify :: split_entry(const char * perm_entry, char ** host, char** user)
{
    char * slash0;
    char * slash1;
    char * at;
	char * colon;
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

			// look for a colon
			colon = strchr(permbuf, ':');
			if (colon) {
				// colon points into permbuf.  permbuf is a local
				// copy of the data made above, so we can modify it.
				// drop a null in place of the colon so everything
				// from the colon and beyond is gone.
				*colon = 0;
			}

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
				if (is_valid_network(permbuf, NULL, NULL)) {
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

int
IpVerify::Verify( DCpermission perm, const struct sockaddr_in *sin, const char * user )
{
	perm_mask_t mask, temp_mask, subnet_mask;
	int found_match, i;
	struct in_addr sin_addr;
	char *thehost;
	char **aliases;
    UserPerm_t * ptable = NULL;
    const char * who = user;

	memcpy(&sin_addr,&sin->sin_addr,sizeof(sin_addr));
	mask = 0;	// must initialize to zero because we logical-or bits into this

    if (who == NULL || *who == '\0') {
        who = TotallyWild;
    }
	switch ( perm ) {

	case ALLOW:
		return USER_AUTH_SUCCESS;
		break;

	default:
		if ( !PermTypeArray[perm] ) {
			EXCEPT("IpVerify::Verify: called with unknown permission %d\n",perm);
		}
		
		if ( PermTypeArray[perm]->behavior == USERVERIFY_ALLOW ) {
			// allow if no HOSTALLOW_* or HOSTDENY_* restrictions 
			// specified.
			return USER_AUTH_SUCCESS;
		}
		
		if ( PermTypeArray[perm]->behavior == USERVERIFY_DENY ) {
			// deny
			return USER_AUTH_FAILURE;
		}
		
		if (PermHashTable->lookup(sin_addr, ptable) != -1) {

            if (has_user(ptable, who, mask)) {
                if ( ( (mask & allow_mask(perm)) == 0 ) && ( (mask & deny_mask(perm)) == 0 ) ) {
                    found_match = FALSE;
                } else {
                    found_match = TRUE;
                }
            }
            else {
                found_match = FALSE;
            }
        }
        else {
            found_match = FALSE;
        }

		if ( found_match == FALSE ) {
			mask = 0;
			// did not find an existing entry, so try subnets
			unsigned char *cur_byte = (unsigned char *) &sin_addr;
			for (i=3; i>0; i--) {
				cur_byte[i] = (unsigned char) 255;

				if ( PermHashTable->lookup(sin_addr, ptable) != -1 ) {
                    if (has_user(ptable, who, temp_mask)) {
                        subnet_mask = (temp_mask & ( allow_mask(perm) | deny_mask(perm) ));
                        if ( subnet_mask != 0 ) {
                            // We found a subnet match.  Logical-or it into our mask.
                            // But only add in the bits that relate to this perm, else
                            // we may not check the hostname strings for a different
                            // perm.
                            mask |= subnet_mask;
                            break;
                        }
                    }
				}
			}  // end of for

			// used in next chunks
            const char * hoststring = NULL;

			// check for matching subnets in ip/mask style
			char tmpbuf[16];
			const unsigned char *byte_array = (const unsigned char *) &(sin->sin_addr);
			sprintf(tmpbuf, "%u.%u.%u.%u", byte_array[0], byte_array[1],
					byte_array[2], byte_array[3]);

			StringList * userList;
			if ( PermTypeArray[perm]->allow_hosts &&
					(hoststring = PermTypeArray[perm]->allow_hosts->
					string_withnetwork(tmpbuf))) {
				// See if the user exist
				if (PermTypeArray[perm]->allow_users->lookup(hoststring, userList) != -1) {
					if (lookup_user(userList, who)) {
						dprintf ( D_SECURITY, "IPVERIFY: matched with host %s and user %s\n",
								hoststring, (who && *who) ? who : "*");
						mask |= allow_mask(perm);
					} else {
						dprintf ( D_SECURITY, "IPVERIFY: matched with host %s, but not user %s!\n",
								hoststring, (who && *who) ? who : "*");
					}
				} else {
					dprintf( D_SECURITY, "IPVERIFY: ip not matched: %s\n", tmpbuf); }
				}
    
			if ( PermTypeArray[perm]->deny_hosts &&
				 (hoststring = PermTypeArray[perm]->deny_hosts->
				  string_withnetwork(tmpbuf))) {
				dprintf ( D_SECURITY, "IPVERIFY: matched with %s\n", hoststring);
				if (PermTypeArray[perm]->deny_users->lookup(hoststring, userList) != -1) {
					if (lookup_user(userList, who)) {  
						dprintf ( D_ALWAYS, "IPVERIFY: denied user %s from %s\n", who, hoststring);
						mask |= deny_mask(perm);
					}
				}
			}


			// now scan through hostname strings
			thehost = sin_to_hostname(sin,&aliases);
			i = 0;
			while ( thehost ) {
                MyString     host(thehost);
                if ( PermTypeArray[perm]->allow_hosts &&
                     (hoststring = PermTypeArray[perm]->allow_hosts->
                      string_anycase_withwildcard(thehost))) {
                    // See if the user exist
					dprintf ( D_SECURITY, "IPVERIFY: matched with %s\n", hoststring);
                    if (PermTypeArray[perm]->allow_users->lookup(hoststring, userList) != -1) {
                        if (lookup_user(userList, who)) {
                            mask |= allow_mask(perm);
                        }
                    }
                } else {
					dprintf( D_SECURITY, "IPVERIFY: hoststring: %s\n", thehost);
				}
                
                if ( PermTypeArray[perm]->deny_hosts &&
                     (hoststring = PermTypeArray[perm]->deny_hosts->
                      string_anycase_withwildcard(thehost))) {
                    if (PermTypeArray[perm]->deny_users->lookup(hoststring, userList) != -1) {
                        if (lookup_user(userList, who)) {  
							dprintf ( D_SECURITY, "IPVERIFY: matched %s at %s to deny list\n", who,hoststring);
                            mask |= deny_mask(perm);
                        }
                    }
                }
				// check all aliases for this IP as well
				thehost = aliases[i++];
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
			if ( mask == 0 ) {
				if ( PermTypeArray[perm]->behavior == USERVERIFY_ONLY_DENIES ) {
					dprintf(D_SECURITY,"IPVERIFY: %s at %s not matched to deny list, so allowing.\n",who,sin_to_string(sin));
					mask |= allow_mask(perm);
				} else {
					DCpermissionHierarchy hierarchy( perm );
					DCpermission const *parent_perms =
						hierarchy.getPermsIAmDirectlyImpliedBy();
					bool parent_allowed = false;
					for( ; *parent_perms != LAST_PERM; parent_perms++ ) {
						if( Verify( *parent_perms, sin, user) == USER_AUTH_SUCCESS ) {
							parent_allowed = true;
							dprintf(D_SECURITY,"IPVERIFY: allowing %s at %s for %s because %s is allowed\n",who,sin_to_string(sin),PermString(perm),PermString(*parent_perms));
							break;
						}
					}
					if( parent_allowed ) {
						mask |= allow_mask(perm);
					}
					else {
						mask |= deny_mask(perm);
					}
				}
			}

			// finally, add the mask we computed into the table with this IP addr
			if ( cache_DNS_results == TRUE ) {
				add_hash_entry(sin->sin_addr, who, mask);			
			}
		}  // end of if find_match is FALSE

		// decode the mask and return True or False to the user.
		if ( mask & deny_mask(perm) )
			return USER_AUTH_FAILURE;
		if ( mask & allow_mask(perm) )
			return USER_AUTH_SUCCESS;
		else
			return USER_AUTH_FAILURE;

		break;
	
	}	// end of switch(perm)

	// should never make it here
	EXCEPT("User Verify: could not decide, should never make it here!");
	return FALSE;
}

bool IpVerify :: lookup_user(StringList * list, const char * user)
{
    bool found = false;
    if (user == NULL) {
        if (list->contains("*")) {
            found = true;
        }
    }
    else {
        if (list->contains(user) || list->contains_anycase_withwildcard(user)) {
            found = true;
        }
    }
    return found;
}

void
IpVerify::process_allow_users( void )
{
	MyString user;
	perm_mask_t mask;
	AllowHostsTable->startIterations();
	while( AllowHostsTable->iterate(user, mask) ) {
		add_host_entry( user.Value(), mask );
	}
}
	

// AddAllowHost is now equivalent to adding */host, where host 
// is actual host name with no wild card!
//
// Additions persist across a reconfig.  This is intended
// for transient permissions (like to automatic permission
// granted to a remote startd host when a shadow starts up.)

bool
IpVerify::AddAllowHost( const char* host, DCpermission perm )
{
	perm_mask_t new_mask = allow_mask(perm);
	perm_mask_t *mask_p = NULL;
    int len = strlen(host);

    char * buf = (char *) malloc(len+3); // 2 for */
    memset(buf, 0, len+3);
    sprintf(buf, "*/%s", host);
    MyString addr(buf);    // */host
    free(buf);

	dprintf( D_DAEMONCORE, "Entered IpVerify::AddAllowHost(%s, %s)\n",
			 host, PermString(perm) );

		// First, see if we already have this host, and if so, we're
		// done. 
	if( AllowHostsTable->lookup( addr, mask_p) != -1) {
			// We found it.  Make sure the mask is cool
		if( ! (*mask_p & new_mask) ) {
				// The host was in there, but we need to add more bits
				// to the mask
			dprintf( D_DAEMONCORE, 
					 "IpVerify::AddAllowHost: Changing mask, was %llu, adding %llu)\n",
					 *mask_p, new_mask );
			new_mask = *mask_p | new_mask;
			*mask_p = new_mask;
			if( add_host_entry(addr.Value(), new_mask) ) {
				dprintf( D_DAEMONCORE, "Added.\n");
				return true;
			} else {
				return false;
			}
		} else {
			dprintf( D_DAEMONCORE, 
					 "IpVerify::AddAllowHost: Already have %s with %llu\n", 
					 host, new_mask );
		}
		return true;
	} 

		// If we're still here, it wasn't in our table, so we want to
		// add it to the real verify hash table, and if that works,
		// add it to both our table (so we remember it on reconfig).
	if( add_host_entry(addr.Value(), new_mask) ) {
		dprintf( D_DAEMONCORE, "Adding new hostallow (%s) entry for \"%s\"\n",  
				 PermString(perm), host );
		AllowHostsTable->insert( addr, new_mask );
		return true;
	} else {
		return false;
	}
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
char *mySubSystem = "COLLECTOR";
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
