/**************************************************************
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright © 1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
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

#include "condor_common.h"
#include "condor_ipverify.h"
#include "internet.h"
#include "condor_config.h"

// Externs to Globals
extern char* mySubSystem;	// the subsys ID, such as SCHEDD, STARTD, etc. 

/*
  The "order" of entries in perm_names and perm_ints must match.

  These arrays are only for use inside condor_ipverify.C, and they
  include all the permission levels that we want ipverify to handle,
  not neccessarily all of the permission levels DaemonCore itself
  cares about (for example, there's nothing in here for
  "IMMEDIATE_FAMILY").  They can *not* be used to convert DCpermission
  enums into strings, you need to use PermString() for that.
*/
const char* IpVerify::perm_names[] = {"READ","WRITE","ADMINISTRATOR","OWNER","NEGOTIATOR","CONFIG",NULL};
const int IpVerify::perm_ints[] = {READ,WRITE,ADMINISTRATOR,OWNER,NEGOTIATOR,CONFIG_PERM,-1};  // must end with -1


const char*
PermString( DCpermission perm )
{
	switch( perm ) {
	case ALLOW:
		return "ALLOW";
	case READ:
		return "READ";
	case WRITE:
		return "WRITE";
	case NEGOTIATOR:
		return "NEGOTIATOR";
	case IMMEDIATE_FAMILY:
		return "IMMEDIATE_FAMILY";
	case ADMINISTRATOR:
		return "ADMINISTRATOR";
	case OWNER:
		return "OWNER";
	case CONFIG_PERM:
		return "CONFIG";
	}
	return "Unknown";
};


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
	for (i=0;i<IPVERIFY_MAX_PERM_TYPES;i++) {
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
	if (PermHashTable)
		delete PermHashTable;

	if( AllowHostsTable )
		delete AllowHostsTable;

	// Clear the Permission Type Array
	for (i=0;i<IPVERIFY_MAX_PERM_TYPES;i++) {
		if ( PermTypeArray[i] )
			delete PermTypeArray[i];
	}
}


int
IpVerify::Init()
{
	char buf[50];
	char *pAllow, *pDeny;
	int i;
	
	did_init = TRUE;
	
	// Clear the Permission Hash Table in case re-initializing
	if (PermHashTable)
		PermHashTable->clear();

	// and Clear the Permission Type Array
	for (i=0;i<IPVERIFY_MAX_PERM_TYPES;i++) {
		if ( PermTypeArray[i] ) {
			delete PermTypeArray[i];
			PermTypeArray[i] = NULL;
		}
	}

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
		

		sprintf(buf,"HOSTALLOW_%s_%s",perm_names[i],mySubSystem);
		if ( (pAllow = param(buf)) == NULL ) {
			sprintf(buf,"HOSTALLOW_%s",perm_names[i]);
			pAllow = param(buf);
		}

			// Treat a "*" for HOSTALLOW_XXX as if it's just
			// undefined. 
		if( pAllow && !strcmp(pAllow, "*") ) {
			free( pAllow );
			pAllow = NULL;
		}
	
		sprintf(buf,"HOSTDENY_%s_%s",perm_names[i],mySubSystem);
		if ( (pDeny = param(buf)) == NULL ) {
			sprintf(buf,"HOSTDENY_%s",perm_names[i]);
			pDeny = param(buf);
		}
	
		if ( !pAllow && !pDeny ) {
			if (perm_ints[i] == CONFIG_PERM) { 	  // deny all CONFIG requests 
				pentry->behavior = IPVERIFY_DENY; // by default
			} else {
				pentry->behavior = IPVERIFY_ALLOW;
			}
		} else {
			if ( pDeny && !pAllow ) {
				pentry->behavior = IPVERIFY_ONLY_DENIES;
			} else {
				pentry->behavior = IPVERIFY_USE_TABLE;
			}
			if ( pAllow ) {
				pentry->allow_hosts = new StringList(pAllow);
				fill_table( pentry->allow_hosts,
					allow_mask(perm_ints[i]) );
				free(pAllow);
			}
			if ( pDeny ) {
				pentry->deny_hosts = new StringList(pDeny);
				fill_table( pentry->deny_hosts,
					deny_mask(perm_ints[i]) );
				free(pDeny);
			}
		}

	}	// end of for i loop

		// Finally, check to see if we have an allow hosts that have
		// been added manually, in which case, re-add those.
	process_allow_hosts();

	return TRUE;
}


int
IpVerify::add_hash_entry(const struct in_addr & sin_addr,int new_mask)
{
	int old_mask = 0;  // must init old_mask to zero!!!

	// assert(PermHashTable);
	if ( PermHashTable->lookup(sin_addr,old_mask) != -1 ) {
		// found an existing entry.  
		
		// hueristic: if the mask already contains what we
		// want, we are done.
		if ( (old_mask & new_mask) == new_mask ) {
			return TRUE;
		}

		// remove it because we are going to edit the mask below
		// and re-insert it.
		PermHashTable->remove(sin_addr);
	}

	// insert entry with new mask
	if ( PermHashTable->insert(sin_addr,old_mask | new_mask) == 0 )
		return TRUE;	// successfully inserted 
	else
		return FALSE;	// error inserting into hash table
}


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

	if( is_ipaddr(addr,&sin_addr) == TRUE ) {
			// This address is an IP addr in numeric form.
			// Add it into the hash table.
		add_hash_entry(sin_addr,mask);
	} else {
			// This address is a hostname.  If it has no wildcards, resolve to
			// and IP address here, add it to our hashtable, and remove the entry
			// (i.e. treat it just like an IP addr).  If it has wildcards, then
			// leave it in this StringList.
		if ( strchr(addr,'*')  ) {
				// Contains wildcards, do nothing, and return false so
				// we leave the host in the string list
			result = false;
		} else {
				// No wildcards; resolve the name
			if ( (hostptr=gethostbyname(addr)) != NULL) {
				if ( hostptr->h_addrtype == AF_INET ) {
					for (int i=0; hostptr->h_addr_list[i]; i++) {
						add_hash_entry( (*(struct in_addr *)
										(hostptr->h_addr_list[i])),
										mask );
					}
				}
			}
		}
	}	
	return result;
}


void
IpVerify::fill_table(StringList *slist, int mask)
{
	char *addr;
	slist->rewind();
	while ( (addr=slist->next()) ) {
		if( add_host_entry(addr, mask) ) {
            slist->deleteCurrent();
		}
	}
}


int
IpVerify::Verify( DCpermission perm, const struct sockaddr_in *sin )
{
	int mask, found_match, i, temp_mask, j;
	struct in_addr sin_addr;
	char *thehost;
	char **aliases;
	
	memcpy(&sin_addr,&sin->sin_addr,sizeof(sin_addr));
	mask = 0;	// must initialize to zero because we logical-or bits into this

	switch ( perm ) {

	case ALLOW:
		return TRUE;
		break;

	default:
		if ( perm >= IPVERIFY_MAX_PERM_TYPES || !PermTypeArray[perm] ) {
			EXCEPT("IpVerify::Verify: called with unknown permission %d\n",perm);
		}
		
		if ( PermTypeArray[perm]->behavior == IPVERIFY_ALLOW ) {
			// allow if no HOSTALLOW_* or HOSTDENY_* restrictions 
			// specified.
			return TRUE;
		}
		
		if ( PermTypeArray[perm]->behavior == IPVERIFY_DENY ) {
			// allow if no HOSTALLOW_* or HOSTDENY_* restrictions 
			// specified.
			return FALSE;
		}
		
		PermHashTable->lookup(sin_addr,mask);

		if ( ( (mask & allow_mask(perm)) == 0 ) && ( (mask & deny_mask(perm)) == 0 ) ) {
			found_match = FALSE;
		} else {
			found_match = TRUE;
		}

		if ( found_match == FALSE ) {
			// did not find an existing entry, so try subnets
			unsigned char *cur_byte = (unsigned char *) &sin_addr;
			for (i=3; i>0; i--) {
				cur_byte[i] = (unsigned char) 255;
				if ( PermHashTable->lookup(sin_addr,temp_mask) != -1 ) {
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
			}  // end of for

			// now scan through hostname strings
			thehost = sin_to_hostname(sin,&aliases);
			i = 0;
			while ( thehost ) {
				if ( PermTypeArray[perm]->allow_hosts &&
					 PermTypeArray[perm]->allow_hosts->contains_withwildcard(thehost) == TRUE )
					mask |= allow_mask(perm);
				if ( PermTypeArray[perm]->deny_hosts &&
					 PermTypeArray[perm]->deny_hosts->contains_withwildcard(thehost) == TRUE )
					mask |= deny_mask(perm);

				// check all aliases for this IP as well
				thehost = aliases[i++];
			}
			// if we found something via our hostname or subnet mactching, we now have 
			// a mask, and we should add it into our table so we need not
			// do a gethostbyaddr() next time.  if we still do not have a mask
			// (perhaps because this host doesn't appear in any list), create one
			// and then add to the table.
			if ( mask == 0 ) {
				if ( PermTypeArray[perm]->behavior == IPVERIFY_ONLY_DENIES ) 
					mask |= allow_mask(perm);
				else 
					mask |= deny_mask(perm);
			}

			// finally, add the mask we computed into the table with this IP addr
			if ( cache_DNS_results == TRUE ) {
				add_hash_entry(sin->sin_addr,mask);			
			}
		}  // end of if find_match is FALSE

		// decode the mask and return True or False to the user.
		if ( mask & deny_mask(perm) )
			return FALSE;
		if ( mask & allow_mask(perm) )
			return TRUE;
		else
			return FALSE;

		break;
	
	}	// end of switch(perm)

	// should never make it here
	EXCEPT("IP Verify: could not decide, should never make it here!");
	return FALSE;
}


void
IpVerify::process_allow_hosts( void )
{
	MyString host;
	int mask;
	AllowHostsTable->startIterations();
	while( AllowHostsTable->iterate(host,mask) ) {
		add_host_entry( host.Value(), mask );
	}
}
	

bool
IpVerify::AddAllowHost( const char* host, DCpermission perm )
{
	int new_mask = allow_mask(perm);
	MyString addr(host);
	int *mask_p = NULL;

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
	if( add_host_entry(host, new_mask) ) {
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
	IpVerify* ipverify;

	config();

#ifdef WIN32
	_CrtMemCheckpoint( &s1 );
#endif

	ipverify = new IpVerify();

	ipverify->Init();

	buf[0] = '\0';

	while( 1 ) {
		printf("Enter test:\n");
		scanf("%s",buf);
		if ( strncmp(buf,"exit",4) == 0 )
			break;
		if ( strncmp(buf,"reinit",6) == 0 ) {
			config();
			ipverify->Init();
			continue;
		}
		printf("Verifying %s ... ",buf);
		sprintf(buf1,"<%s:1970>",buf);
		string_to_sin(buf1,&sin);
		if ( ipverify->Verify(WRITE,&sin) == TRUE )
			printf("ALLOW\n");
		else
			printf("DENY\n");
	}
	
	delete ipverify;

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
