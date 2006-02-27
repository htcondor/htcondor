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

int parseIpPort (const MyString &, MyString &);

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

int getIpAddr( const ClassAd *ad,
			   const char *attrname,
			   const char *attrold,
			   MyString &ip )
{
	ExprTree	*tree;

	// Reset IP string
	ip = "";

	// get the IP and port of the startd 
	tree = ad->Lookup( attrname );
	if ( tree ) {
		dprintf (D_ALWAYS, "Found %s\n", attrname );
		ip = ((String *)tree->RArg())->Value();
	}
	
	// if not there, try to lookup the old style "STARTD_IP_ADDR" string
	if ( ! tree && attrold ) {
		tree = ad->Lookup( attrold );
		if ( tree ) {
			dprintf (D_ALWAYS, "Found %s\n", attrold );
			ip = ((String *)tree->RArg())->Value();
		}
	}
	
	// Finally, try to lookup "MyAddress"...
	if ( ip.Length() == 0 ) {
		tree = ad->Lookup( ATTR_MY_ADDRESS );
		if ( tree ) {
			dprintf (D_ALWAYS, "Found %s\n", ATTR_MY_ADDRESS );
			ip = ((String *)tree->RArg())->Value();
		}
	}
	dprintf( D_ALWAYS, "Got IP = '%s'\n", ip.GetCStr() );

	// If no valid string, do our own thing..
	if ( ip.Length() == 0 )
	{
		dprintf (D_ALWAYS, "No IP address in classAd \n");
		return -1;
	}

	return 0;
}

// functions to make the hashkeys ...
// make hashkeys from the obtained ad
bool
makeStartdAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree	*tree;
	MyString	buffer;

	// get the name of the startd
	if (!(tree = ad->Lookup ("Name")))
	{		
		// ... if name was not specified
		dprintf(D_FULLDEBUG,"Warning: No 'Name' attribute; trying 'Machine'\n");
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		hk.name = ((String *)tree->RArg())->Value();
	}
	else
	{
		// neither Name nor Machine specified in ad
		dprintf (D_ALWAYS, "Error: Neither 'Name' nor 'Machine' specified\n");
		return false;
	}

	if ( getIpAddr( ad, ATTR_STARTD_IP_ADDR, "STARTD_IP_ADDR", buffer ) < 0 ) {
		dprintf (D_ALWAYS, "Error: Invalid StartAd\n");
		return false;
	}
	if ( parseIpPort (buffer, hk.ip_addr) < 0 ) {
		return false;
	}

	return true;
}

#if WANT_QUILL
bool
makeQuillAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from) {
	ExprTree	*tree;
	MyString	buffer;
	MyString	buf2;

	// get the name of the quill daemon
	if (!(tree = ad->Lookup ("Name")))
	{
		dprintf(D_FULLDEBUG,"Warning: No 'Name' attribute; trying 'Machine'\n");
		// ... if name was not specified
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		hk.name = ((String *)tree->RArg())->Value();
	}
	else
	{
		dprintf (D_ALWAYS, "Error: Neither 'Name' nor 'Machine' specified\n");
		// neither Name nor Machine specified
		return false;
	}
	
	// as in the case of submittor ads (see makeScheddAdHashKey), we also use the 
	// schedd name to construct the hash key for a quill ad.  this 
	// solves the problem of multiple quill daemons on the same name on the same 
	// machine submitting to the same pool
	// -Ameet Kini <akini@cs.wisc.edu> 8/2005
	if ( (tree = ad->Lookup(ATTR_SCHEDD_NAME)) ) {
		hk.name += (((String *)tree->RArg())->Value());
	}

	return true;
}
#endif /* WANT_QUILL */

bool
makeScheddAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree	*tree;
	MyString	buffer;

	// get the name of the startd
	if (!(tree = ad->Lookup ("Name")))
	{
		dprintf(D_FULLDEBUG,"Warning: No 'Name' attribute; trying 'Machine'\n");
		// ... if name was not specified
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		hk.name = ((String *)tree->RArg())->Value();
	}
	else
	{
		dprintf (D_ALWAYS, "Error: Neither 'Name' nor 'Machine' specified\n");
		// neither Name nor Machine specified
		return false;
	}
	
	// this may be a submittor ad.  if so, we also want to append the schedd name to the
	// hash.  this will fix problems were submittor ads will clobber one another
	// if the more than one schedd runs on the same IP address submitting into the same pool.
	// -Todd Tannenbaum <tannenba@cs.wisc.edu> 2/2005
	if ( (tree = ad->Lookup(ATTR_SCHEDD_NAME)) ) {
		hk.name += (((String *)tree->RArg())->Value());
	}

	// get the IP and port of the startd 
	tree = ad->Lookup (ATTR_SCHEDD_IP_ADDR);
	
	if (!tree)
		tree = ad->Lookup ("SCHEDD_IP_ADDR");

	// get the IP and port of the schedd 
	if ( getIpAddr( ad, ATTR_SCHEDD_IP_ADDR, "SCHEDD_IP_ADDR", buffer ) < 0 ) {
		dprintf (D_ALWAYS, "Error: Invalid SchedAd\n");
		return false;
	}
	if ( parseIpPort (buffer, hk.ip_addr) < 0 ) {
		return false;
	}

	return true;
}


bool
makeLicenseAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree *tree;
	MyString buffer;

	// get the name of the startd
	if (!(tree = ad->Lookup ("Name")))
	{
		dprintf(D_FULLDEBUG,"Warning: No 'Name' attribute; trying 'Machine'\n");
		// ... if name was not specified
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		hk.name = ((String *)tree->RArg())->Value();
	}
	else
	{
		dprintf (D_ALWAYS, "Error: Neither 'Name' nor 'Machine' specified\n");
		// neither Name nor Machine specified
		return false;
	}
	
	// get the IP and port of the startd 
	if ( getIpAddr( ad, ATTR_MY_ADDRESS, NULL, buffer ) < 0 ) {
		dprintf (D_ALWAYS, "Error: Invalid LicenseAd\n");
		return false;
	}
	if ( parseIpPort (buffer, hk.ip_addr) < 0 ) {
		return false;
	}

	return true;
}


bool
makeMasterAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree *tree;

	if (!(tree = ad->Lookup ("Name")))
	{
		dprintf( D_FULLDEBUG, 
				 "Warning: No 'Name' attribute; trying 'Machine'\n" );
			// ... if name was not specified
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		hk.name = ((String *)tree->RArg())->Value ();
	}
	else
	{
		dprintf (D_ALWAYS, "Error: Neither 'Name' nor 'Machine' specified\n");
		// neither Name nor Machine specified
		return false;
	}

	// ip_addr not necessary
	hk.ip_addr = "";

	return true;
}


bool
makeCkptSrvrAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	char	*name = NULL;
	if (!ad->LookupString ("Machine", &name ))
	{
		dprintf (D_ALWAYS, "Error:  No 'Machine' attribute\n");
		return false;
	}

	hk.name = name;
	free( name );
	hk.ip_addr = "";

	return true;
}

bool
makeCollectorAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	char	*name = NULL;
	if (!ad->LookupString ("Machine", &name ))
	{
		dprintf (D_ALWAYS, "Error:  No 'Machine' attribute\n");
		return false;
	}

	hk.name = name;
	free( name );
	hk.ip_addr = "";

	return true;
}

bool
makeStorageAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	char	*name = NULL;
	if (!ad->LookupString ("Name", &name ))
	{
		dprintf (D_ALWAYS, "Error:  No 'Name' attribute\n");
		return false;
	}

	hk.name = name;
	free( name );
	hk.ip_addr = "";

	return true;
}


bool
makeNegotiatorAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	char	*name = NULL;
	if( !ad->LookupString( ATTR_NAME, &name ) ) {
		dprintf( D_ALWAYS, "Error: No '%s' attribute\n", ATTR_NAME );
		return false;
	}

	hk.name = name;
	free( name );
	hk.ip_addr = "";

	return true;
}


bool
makeHadAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	char	*name = NULL;
	if (!ad->LookupString ("Name", &name ))
	{
		dprintf (D_ALWAYS, "Error:  No 'Name' attribute\n");
		return false;
	}

	hk.name = name;
	free( name );
	hk.ip_addr = "";

	return true;
}

// for anything that sends its updates via UPDATE_AD_GENERIC, this
// needs to provide a key that will uniquely identify each entity
// with respect to all entities of that type
// (e.g. this wouldn't work for submitter ads - see code for
// makeScheddAdHashKey above)
bool
makeGenericAdHashKey (AdNameHashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	char	*name = NULL;
	if (!ad->LookupString ("Name", &name ))
	{
		dprintf (D_ALWAYS, "Error:  No 'Name' attribute\n");
		return false;
	}
	
	hk.name = name;
	free( name );
	hk.ip_addr = "";
	
	return true;
}

// utility function:  parse the string "<aaa.bbb.ccc.ddd:pppp>"
//  Extracts the ip address portion ("aaa.bbb.ccc.ddd")
int 
parseIpPort (const MyString &ip_port_pair, MyString &ip_addr)
{
	ip_addr = "";

    const char *ip_port = ip_port_pair.GetCStr();
    if ( ! ip_port ) {
        return -1;
    }
	ip_port++;			// Skip the leading "<"
    while ( *ip_port && *ip_port != ':')
    {
		ip_addr += *ip_port;
        ip_port++;
    }

	// don't care about port number
	return 0;
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
