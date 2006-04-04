/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_classad.h"
#include "HashTable.h"
#include "hashkey.h"
#include "condor_attributes.h"

#ifndef WIN32
#include <netinet/in.h>
#endif

extern "C" char * sin_to_string(struct sockaddr_in *);

template class HashTable<AdNameHashKey, ClassAd *>;
template class HashTable<MyString, CollectorHashTable *>;

void AdNameHashKey::sprint (MyString &s)
{
	if (ip_addr.Length() )
		s.sprintf( "< %s , %s >", name.GetCStr(), ip_addr.GetCStr() );
	else
		s.sprintf( "< %s >", name.GetCStr() );
}

bool operator== (const AdNameHashKey &lhs, const AdNameHashKey &rhs)
{
    return (  ( lhs.name == rhs.name ) && ( lhs.ip_addr == rhs.ip_addr ) );
}

ostream &operator<< (ostream &out, const AdNameHashKey &hk)
{
	out << "Hashkey: (" << hk.name << "," << hk.ip_addr;
	out << ")" << endl;
	return out;
}

static int sumOverString(const MyString &str)
{
	int sum = 0;
	for (const char *p = str.GetCStr(); p && *p; p++) {
		sum += *p;
	}
	return sum;
}

int stringHashFunction (const MyString &str, int numBuckets)
{
	return sumOverString(str) % numBuckets;
}

int adNameHashFunction (const AdNameHashKey &key, int numBuckets)
{
    unsigned int bkt = 0;

    bkt += sumOverString(key.name);
    bkt += sumOverString(key.ip_addr);
    bkt %= numBuckets;

    return bkt;
}

// Log a missing attribute warning
void
logWarning( const char *ad_type,
			const char *attrname,
			const char *attrold = NULL,
			const char *attrextra = NULL)
{
	if ( attrold && attrextra ) {
		dprintf(D_FULLDEBUG,
				"%sAd Warning: No '%s' attribute; trying '%s' and '%s'\n",
				ad_type, attrname, attrold, attrextra );
	} else if ( attrold ) {
		dprintf(D_FULLDEBUG,
				"%sAd Warning: No '%s' attribute; trying '%s'\n",
				ad_type, attrname, attrold );
	} else {
		dprintf(D_FULLDEBUG,
				"%sAd Warning: No '%s' attribute; giving up\n",
				ad_type, attrname );
	}
}

// Log a missing attribute error
void
logError( const char *ad_type,
		  const char *attrname,
		  const char *attrold = NULL)
{
	if ( attrold ) {
		dprintf (D_ALWAYS,
				 "%sAd Error: Neither '%s' nor '%s' found in ad\n",
				 ad_type, attrname, attrold );
	} else if ( attrname ) {
		dprintf (D_ALWAYS,
				 "%sAd Error: '%s' not found in ad\n",
				 ad_type, attrname );
	} else {
		dprintf (D_ALWAYS,
				 "%sAd Error: invalid ad\n",
				 ad_type );
	}
}

// Look up an attribute in an ad, optionally fall back to an alternate
// and/or log errors
bool
adLookup( const char *ad_type,
		  const ClassAd *ad,
		  const char *attrname,
		  const char *attrold,
		  MyString &string,
		  bool log = true )
{
	char	buf[256];
	bool	rval = true;

    if ( !ad->LookupString( attrname, buf, sizeof(buf) ) ) {
		if ( log ) {
			logWarning( ad_type, attrname, attrold );
		}

		if ( !attrold ) {
			buf[0] = '\0';
			rval = false;
		} else {
			if ( !ad->LookupString( attrold, buf, sizeof(buf) ) ) {
				if ( log ) {
					logError( ad_type, attrname, attrold );
				}
				buf[0] = '\0';
				rval = false;
			}
		}
	}

	buf[sizeof(buf)-1] = '\0';
	string = buf;

	return rval;
}

// Look up an IP attribute in an ad, optionally fall back to an alternate
bool
getIpAddr( const char *ad_type,
		   const ClassAd *ad,
		   const char *attrname,
		   const char *attrold,
		   MyString &ip )
{
	MyString	tmp;

	// get the IP and port of the startd
	if ( !adLookup( ad_type, ad, attrname, attrold, tmp, true ) ) {
		return false;
	}

	// If no valid string, do our own thing..
	if ( ( tmp.Length() == 0 ) || ( !parseIpPort( tmp, ip ) )  ) {
		dprintf (D_ALWAYS, "%sAd: Invalid IP address in classAd\n", ad_type );
		return false;
	}

	return true;
}

// functions to make the hashkeys ...
// make hashkeys from the obtained ad
bool
makeStartdAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{

	// get the name of the startd;
	// this gets complicated with ID
	if ( !adLookup( "Start", ad, ATTR_NAME, NULL, hk.name, false ) ) {
		logWarning( "Start",
					ATTR_NAME, ATTR_MACHINE, ATTR_VIRTUAL_MACHINE_ID );

		// Get the machine name; if it's not there, give up
		if ( !adLookup( "Start", ad, ATTR_MACHINE, NULL, hk.name, false ) ) {
			logError( "Start", ATTR_NAME, ATTR_MACHINE );
			return false;
		}
		// Finally, if there is a virtual machine ID, append it
		int		vm;
		if ( ad->LookupInteger( ATTR_VIRTUAL_MACHINE_ID, vm ) ) {
			hk.name += ":";
			hk.name += vm;
		}
	}

	if ( !getIpAddr( "Start", ad, ATTR_STARTD_IP_ADDR,
					 "STARTD_IP_ADDR", hk.ip_addr ) ) {
		return false;
	}

	return true;
}

#if WANT_QUILL
bool
makeQuillAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from) {

	// get the name of the quill daemon
	if ( !adLookup( "Quill", ad, ATTR_NAME, ATTR_MACHINE, hk.name ) ) {
		return false;
	}
	
	// as in the case of submittor ads (see makeScheddAdHashKey), we
	// also use the schedd name to construct the hash key for a quill
	// ad.  this solves the problem of multiple quill daemons on the
	// same name on the same machine submitting to the same pool
	// -Ameet Kini <akini@cs.wisc.edu> 8/2005
	MyString	tmp;
	if ( adLookup( "Quill", ad, ATTR_SCHEDD_NAME, NULL, tmp, false ) ) {
		hk.name += tmp;
	}

	return true;
}
#endif /* WANT_QUILL */

bool
makeScheddAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{

	// get the name of the schedd
	if ( !adLookup( "Schedd", ad, ATTR_NAME, ATTR_MACHINE, hk.name ) ) {
		return false;
	}
	
	// this may be a submittor ad.  if so, we also want to append the
	// schedd name to the hash.  this will fix problems were submittor
	// ads will clobber one another if the more than one schedd runs
	// on the same IP address submitting into the same pool.
	// -Todd Tannenbaum <tannenba@cs.wisc.edu> 2/2005
	MyString	tmp;
	if ( adLookup( "Schedd", ad, ATTR_SCHEDD_NAME, NULL, tmp, false ) ) {
		hk.name += tmp;
	}

	// get the IP and port of the schedd 
	if ( !getIpAddr( "Schedd", ad, ATTR_SCHEDD_IP_ADDR,
					 "SCHEDD_IP_ADDR", hk.ip_addr ) ) {
		return false;
	}

	return true;
}


bool
makeLicenseAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{

	// get the name of the license
	if ( !adLookup( "License", ad, ATTR_NAME, ATTR_MACHINE, hk.name ) ) {
		return false;
	}
	
	// get the IP and port of the startd 
	if ( !getIpAddr( "License", ad, ATTR_MY_ADDRESS, NULL, hk.ip_addr ) ) {
		return false;
	}

	return true;
}


bool
makeMasterAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	hk.ip_addr = "";
	return adLookup( "Master", ad, ATTR_NAME, ATTR_MACHINE, hk.name );
}


bool
makeCkptSrvrAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	hk.ip_addr = "";
	return adLookup( "CheckpointServer", ad, ATTR_MACHINE, NULL, hk.name );
}

bool
makeCollectorAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	hk.ip_addr = "";
	return adLookup( "Collector", ad, ATTR_MACHINE, NULL, hk.name );
}

bool
makeStorageAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	hk.ip_addr = "";
	return adLookup( "Storage", ad, ATTR_NAME, NULL, hk.name );
}


bool
makeNegotiatorAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	hk.ip_addr = "";
	return adLookup( "Negotiator",  ad, ATTR_NAME, NULL, hk.name );
}


bool
makeHadAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	hk.ip_addr = "";
	return adLookup( "HAD", ad, ATTR_NAME, NULL, hk.name );
}

// for anything that sends its updates via UPDATE_AD_GENERIC, this
// needs to provide a key that will uniquely identify each entity
// with respect to all entities of that type
// (e.g. this wouldn't work for submitter ads - see code for
// makeScheddAdHashKey above)
bool
makeGenericAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	hk.ip_addr = "";
	return adLookup( "Generic", ad, ATTR_NAME, NULL, hk.name );
}

// utility function:  parse the string "<aaa.bbb.ccc.ddd:pppp>"
//  Extracts the ip address portion ("aaa.bbb.ccc.ddd")
bool 
parseIpPort (const MyString &ip_port_pair, MyString &ip_addr)
{
	ip_addr = "";

    const char *ip_port = ip_port_pair.GetCStr();
    if ( ! ip_port ) {
        return false;
    }
	ip_port++;			// Skip the leading "<"
    while ( *ip_port && *ip_port != ':')
    {
		ip_addr += *ip_port;
        ip_port++;
    }

	// don't care about port number
	return true;
}

// HashString
HashString::HashString( void )
{
}

HashString::HashString( const AdNameHashKey &hk )
		: MyString( )
{
	Build( hk );
}

void
HashString::Build( const AdNameHashKey &hk )
{
	if ( hk.ip_addr.Length() ) {
		sprintf( "< %s , %s >", hk.name.GetCStr(), hk.ip_addr.GetCStr() );
	} else {
		sprintf( "< %s >", hk.name.GetCStr() );
	}
}
