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
#include "condor_common.h"
#include "condor_ipverify.h"
#include "internet.h"
#include "condor_config.h"
#include "condor_perms.h"

// Externs to Globals
extern char* mySubSystem;	// the subsys ID, such as SCHEDD, STARTD, etc. 

/*
  The "order" of entries in perm_names and perm_ints must match.

  These arrays are only for use inside condor_userverify.C, and they
  include all the permission levels that we want userverify to handle,
  not neccessarily all of the permission levels DaemonCore itself
  cares about (for example, there's nothing in here for
  "IMMEDIATE_FAMILY").  They can *not* be used to convert DCpermission
  enums into strings, you need to use PermString() (in the util lib) for
  that.
*/
const char* IpVerify::perm_names[] = {"READ","WRITE","DAEMON", "ADMINISTRATOR","OWNER","NEGOTIATOR","CONFIG",NULL};
const int IpVerify::perm_ints[] = {READ,WRITE,DAEMON,ADMINISTRATOR,OWNER,NEGOTIATOR,CONFIG_PERM,-1};  // must end with -1
const char TotallyWild[] = "*";


// Hash function for Permission hash table
static int
compute_perm_hash(const struct in_addr &in_addr, int numBuckets)
{
	unsigned int h = *((unsigned int *)&in_addr);
	return ( h % numBuckets );
}

// Hash function for AllowHosts hash table
static int
compute_host_hash( const MyString & str, int numBuckets )
{
	return ( str.Hash() % numBuckets );
}

// == operator for struct in_addr, also needed for hash table template
bool operator==(const struct in_addr a, const struct in_addr b) {
	return a.s_addr == b.s_addr;
}

// Constructor
IpVerify::IpVerify() 
{
	int i;

	did_init = FALSE;
	for (i=0;i<USERVERIFY_MAX_PERM_TYPES;i++) {
		PermTypeArray[i] = NULL;
	}

	cache_DNS_results = TRUE;

	PermHashTable = new PermHashTable_t(797, compute_perm_hash);
	AllowHostsTable = new HostHashTable_t(53, compute_host_hash);
}


// Destructor
IpVerify::~IpVerify()
{
	int i;

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
	for (i=0;i<USERVERIFY_MAX_PERM_TYPES;i++) {
		if ( PermTypeArray[i] )
			delete PermTypeArray[i];
	}
}


int
IpVerify::Init()
{
	char buf[50];
	char *pAllow, *pDeny, *pOldAllow, *pOldDeny, *pNewAllow = NULL, *pNewDeny = NULL;
    char *pCopyAllow=NULL, *pCopyDeny=NULL ;
	int i;
    bool useOldPermSetup = true;
	
	did_init = TRUE;
	
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
	for (i=0;i<USERVERIFY_MAX_PERM_TYPES;i++) {
		if ( PermTypeArray[i] ) {
			delete PermTypeArray[i];
			PermTypeArray[i] = NULL;
		}
	}

    // This is the new stuff
	for ( i=0; perm_ints[i] >= 0; i++ ) {
			// We're broken if either: there's no name for this kind
			// of DC permission, _or_ there _is_ an entry for it
			// already (since we thought we just deleted them all). 
		if ( !perm_names[i] || PermTypeArray[perm_ints[i]] ) {
			EXCEPT("IP VERIFY code misconfigured");
		}

		PermTypeEntry* pentry = new PermTypeEntry();
		if ( !pentry ) {
			EXCEPT("IP VERIFY out of memory");
		} else {
			PermTypeArray[perm_ints[i]] = pentry;
		}
		
        sprintf(buf,"ALLOW_%s_%s",perm_names[i],mySubSystem);
		if ( (pNewAllow = param(buf)) == NULL ) {
			sprintf(buf,"ALLOW_%s", perm_names[i]);
			pNewAllow = param(buf);
		}

        // This is the old stuff, eventually it will be gone
        sprintf(buf,"HOSTALLOW_%s_%s",perm_names[i],mySubSystem);
        if ( (pOldAllow = param(buf)) == NULL ) {
            sprintf(buf,"HOSTALLOW_%s",perm_names[i]);
            pOldAllow = param(buf);
        }

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
        if (perm_ints[i] != DAEMON) {
            pAllow = merge(pNewAllow, pOldAllow);
            if ((perm_ints[i] == WRITE) && pAllow) {
                pCopyAllow = strdup(pAllow);
            }
        }
        else {
            if (pNewAllow) {
                pAllow = merge(pNewAllow, 0);
            }
            else {
                pAllow = merge(0, pCopyAllow);
            }
            if (pCopyAllow) {
                free(pCopyAllow);
                pCopyAllow = NULL;
            }
        }

		sprintf(buf,"DENY_%s_%s",perm_names[i],mySubSystem);
		if ( (pNewDeny = param(buf)) == NULL ) {
			sprintf(buf,"DENY_%s",perm_names[i]);
			pNewDeny = param(buf);
		}

        sprintf(buf,"HOSTDENY_%s_%s",perm_names[i],mySubSystem);
        if ( (pOldDeny = param(buf)) == NULL ) {
            sprintf(buf,"HOSTDENY_%s",perm_names[i]);
            pOldDeny = param(buf);
        }

        if (perm_ints[i] != DAEMON) {
            pDeny = merge(pNewDeny, pOldDeny);
            if ((perm_ints[i] == WRITE) && pDeny) {
                pCopyDeny = strdup(pDeny);
            }
        }
        else {
            if (pNewDeny) {
                pDeny = merge(pNewDeny, 0);
            }
            else {
                pDeny = merge(0, pCopyDeny);
            }
            if (pCopyDeny) {
                free(pCopyDeny);
                pCopyDeny = NULL;
            }
        }

		if (pAllow && (DebugFlags & D_FULLDEBUG)) {
			dprintf ( D_SECURITY, "IPVERIFY: allow: %s\n", pAllow);
		}
		if (pDeny && (DebugFlags & D_FULLDEBUG)) {
			dprintf ( D_SECURITY, "IPVERIFY: deny: %s\n", pDeny);
		}

		if ( !pAllow && !pDeny ) {
			if (perm_ints[i] == CONFIG_PERM) { 	  // deny all CONFIG requests 
				pentry->behavior = USERVERIFY_DENY; // by default
			} else {
				pentry->behavior = USERVERIFY_ALLOW;
			}
		} else {
            useOldPermSetup = false;        // new permission setup!
			if ( pDeny && !pAllow ) {
				pentry->behavior = USERVERIFY_ONLY_DENIES;
			} else {
				pentry->behavior = USERVERIFY_USE_TABLE;
			}
			if ( pAllow ) {
                fill_table( pentry, allow_mask(perm_ints[i]), pAllow, true );
				free(pAllow);
                pAllow = NULL;
			}
			if ( pDeny ) {
				fill_table( pentry,	deny_mask(perm_ints[i]), pDeny, false );
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

bool IpVerify :: has_user(UserPerm_t * perm, const char * user, int & mask, MyString & userid)
{
    // Now, let's see if the user is there
    int             found = -1;
    assert(perm);

    if (user && *user && (strcmp("*", user) != 0)) {
        userid = MyString(user);
        if ( (found = perm->lookup(userid, mask)) == -1 ) {
            // try *@..../...
            char *tmp;
            char buf[256];
            memset(buf, 0, 256);
            if ((tmp = strchr( user, '@')) == NULL) {
                dprintf(D_SECURITY, "IPVERIFY: Malformed user name: %s\n", user);
            }      
            else {
                if (strlen(tmp) > 255) {
                    dprintf(D_SECURITY, "IPVERIFY: User name is too long (over 255): %s\n", user);
                }  
                else {
                    sprintf(buf, "*%s", tmp);
                    userid = MyString(buf);
                    found = perm->lookup(user, mask);
                }
            }
        }
    }

    // Last resort, see if the wild card is in the list
    if (found == -1) {
        MyString tmp;
        // see if the user is total wild
        userid = TotallyWild;
        found  = perm->lookup(userid, mask);
    }
    return (found != -1);
}   

int
IpVerify::add_hash_entry(const struct in_addr & sin_addr, const char * user, int new_mask)
{
    UserPerm_t * perm = NULL;
    int old_mask = 0;  // must init old_mask to zero!!!
    MyString userid;

	// assert(PermHashTable);
	if ( PermHashTable->lookup(sin_addr, perm) != -1 ) {
		// found an existing entry.  

		if (has_user(perm, user, old_mask, userid)) {
            // hueristic: if the mask already contains what we
            // want, we are done.
            if ( (old_mask & new_mask) == new_mask ) {
                return TRUE;
            }

            // remove it because we are going to edit the mask below
            // and re-insert it.
            perm->remove(MyString(user));
        }
        userid = MyString(user);
	}
    else {
        // Convert domain to lower case 
        char * lower = strdup(user);
        char * at = strchr(lower, '@');
        if (at) {
            while (*(++at) != '\0') {
                *at = tolower((int) *at);
            }
        }
        free(lower);
        perm = new UserPerm_t(42, compute_host_hash);
        if (PermHashTable->insert(sin_addr, perm) != 0) {
            delete perm;
            return FALSE;
        }
        userid = MyString(user);
    }

    perm->insert(userid, old_mask | new_mask);

    return TRUE;
}


// The form of the addr is: joe@some.domain.name/host.name
// For example, joe@cs.wisc.edu/perdita.cs.wisc.edu
//              joe@cs.wisc.edu/*.cs.wisc.edu    -- wild
//              joe@cs.wisc.edu/*                -- wild
//              */*.cs.wisc.edu                  -- wild
//              */perdita.cs.wisc.edu            
//              *@cs.wisc.edu/*                  -- wild
//              *@cs.wisc.edu/*.cs.wisc.edu      -- wild
//              *@cs.wisc.edu/perdita.cs.wisc.edu
//              * and */* are considered as invalid and are discarded earlier
//
// This returns true if we successfully added the given entry to the
// hash table.  If we didn't, it's because the addr we were passed
// contains a wildcard, in which case we want to keep that in the
// string list.
bool
IpVerify::add_host_entry( const char* addr, int mask ) 
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

    if( is_ipaddr(host,&sin_addr) == TRUE ) {
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
			if ( (hostptr=gethostbyname(host)) != NULL) {
				if ( hostptr->h_addrtype == AF_INET ) {
					for (int i=0; hostptr->h_addr_list[i]; i++) {
						add_hash_entry( (*(struct in_addr *)
                                         (hostptr->h_addr_list[i])),
                                        user, mask );
					}   
				}
				if (DebugFlags & D_FULLDEBUG) {
					dprintf (D_SECURITY, "IPVERIFY: succesfully added %s\n", host);
				}
			} else {
				dprintf (D_SECURITY, "IPVERIFY: unable to resolve %s\n", host);
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
IpVerify::fill_table(PermTypeEntry * pentry, int mask, char * list, bool allow)
{
    StringList * whichHostList = NULL;
    UserHash_t * whichUserHash = NULL;
    StringList * slist;

    assert(pentry);

    if (whichHostList == NULL) {
        whichHostList = new StringList();
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
    char permbuf[512];

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
	strcpy (permbuf, perm_entry);

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
}

int
IpVerify::Verify( DCpermission perm, const struct sockaddr_in *sin, const char * user )
{
	int mask, found_match, i, temp_mask, j;
	struct in_addr sin_addr;
	char *thehost;
	char **aliases;
    UserPerm_t * ptable = NULL;
    MyString     userid;
    const char * who = user;

	memcpy(&sin_addr,&sin->sin_addr,sizeof(sin_addr));
	mask = 0;	// must initialize to zero because we logical-or bits into this

    if (who == NULL) {
        who = TotallyWild;
    }
	switch ( perm ) {

	case ALLOW:
		return USER_AUTH_SUCCESS;
		break;

	default:
		if ( perm >= USERVERIFY_MAX_PERM_TYPES || !PermTypeArray[perm] ) {
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

            if (has_user(ptable, who, mask, userid)) {
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
			// did not find an existing entry, so try subnets
			unsigned char *cur_byte = (unsigned char *) &sin_addr;
			for (i=3; i>0; i--) {
				cur_byte[i] = (unsigned char) 255;

				if ( PermHashTable->lookup(sin_addr, ptable) != -1 ) {
                    if (has_user(ptable, who, temp_mask, userid)) {
                        j = (temp_mask & ( allow_mask(perm) | deny_mask(perm) ));
                        if ( j != 0 ) {
                            // We found a subnet match.  Logical-or it into our mask.
                            // But only add in the bits that relate to this perm, else
                            // we may not check the hostname strings for a different
                            // perm.
                            mask |= j;
                            break;
                        }
                    }
				}
			}  // end of for

			// used in next chunks
            const char * hoststring = NULL;

			// check for matching subnets in ip/mask style
			char tmpbuf[16];
			cur_byte = (unsigned char *) &(sin->sin_addr);
			sprintf(tmpbuf, "%u.%u.%u.%u", cur_byte[0], cur_byte[1], cur_byte[2], cur_byte[3]);

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
                StringList * userList;
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
			if ( mask == 0 ) {
				if ( PermTypeArray[perm]->behavior == USERVERIFY_ONLY_DENIES ) 
					mask |= allow_mask(perm);
				else 
					mask |= deny_mask(perm);
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
	int mask;
	AllowHostsTable->startIterations();
	while( AllowHostsTable->iterate(user, mask) ) {
		add_host_entry( user.Value(), mask );
	}
}
	

// AddAllowHost is now equivalent to adding */host, where host 
// is actual host name with no wild card!
bool
IpVerify::AddAllowHost( const char* host, DCpermission perm )
{
	int new_mask = allow_mask(perm);
	int *mask_p = NULL;
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
					 "IpVerify::AddAllowHost: Changing mask, was %d, adding %d)\n",
					 *mask_p, new_mask );
			*mask_p = *mask_p | new_mask;
		} else {
			dprintf( D_DAEMONCORE, 
					 "IpVerify::AddAllowHost: Already have %s with %d\n", 
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
