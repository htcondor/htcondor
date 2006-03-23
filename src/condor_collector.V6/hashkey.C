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
#include "condor_debug.h"
#include "condor_classad.h"
#include "HashTable.h"
#include "hashkey.h"
#include "condor_attributes.h"

#ifndef WIN32
#include <netinet/in.h>
#endif

extern "C" char * sin_to_string(struct sockaddr_in *);

template class HashTable<HashKey, ClassAd *>;
int parseIpPort (const MyString &, MyString &);

void HashKey::sprint (MyString &s)
{
	if (ip_addr.Length() )
		s.sprintf( "< %s , %s >", name.GetCStr(), ip_addr.GetCStr() );
	else
		s.sprintf( "< %s >", name.GetCStr() );
}

bool operator== (const HashKey &lhs, const HashKey &rhs)
{
    return (  ( lhs.name == rhs.name ) && ( lhs.ip_addr == rhs.ip_addr ) );
}

ostream &operator<< (ostream &out, const HashKey &hk)
{
	out << "Hashkey: (" << hk.name << "," << hk.ip_addr;
	out << ")" << endl;
	return out;
}

int hashFunction (const HashKey &key, int numBuckets)
{
    unsigned int bkt = 0;
	const char *p;

    for (p = key.name.GetCStr(); p && *p; bkt += *p++);
    for (p = key.ip_addr.GetCStr(); p && *p; bkt += *p++);

    bkt %= numBuckets;
    return bkt;
}

bool lookup( ClassAd *ad,
			const char *attrname,
			const char *attrold,
			MyString &string,
			bool warn = false )
{
	char	buf[128];
	bool	rval = true;

    if ( !ad->LookupString( attrname, buf, sizeof(buf) ) ) {
		if ( warn ) {
			dprintf(D_FULLDEBUG,
					"Warning: No '%s' attribute; trying '%s'\n",
					attrname, attrold );
		}

		if ( !attrold ) {
			buf[0] = '\0';
			rval = false;
		} else {
			if ( !ad->LookupString( attrold, buf, sizeof(buf) ) ) {
				if ( warn ) {
					dprintf (D_ALWAYS,
							 "Error: Neither '%s' nor '%s' specified\n",
							 attrname, attrold );
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

int getIpAddr( ClassAd *ad,
			   const char * attrname,
			   const char *attrold,
			   sockaddr_in *from,
			   MyString &ip )
{

	// get the IP and port of the startd
	lookup( ad, attrname, attrold, ip, false );

	// If no valid string, do our own thing..
	if ( ip.Length() == 0 )
	{
		ip = sin_to_string( from );

		MyString	buf2;

		buf2.sprintf( "%s = \"%s\"", attrname, ip.GetCStr() );
		ad->Insert( buf2.GetCStr() );
		dprintf (D_ALWAYS , "WARNING: No %s; inferring address %s\n",
				 attrname, buf2.GetCStr() );
	}

	return 0;
}

// functions to make the hashkeys ...
// make hashkeys from the obtained ad
bool
makeStartdAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	MyString	buffer;
	MyString	buf2;

	// get the name of the startd
	if ( !lookup( ad, "Name", "Machine", hk.name, true ) ) {
		return false;
	}

	getIpAddr( ad, ATTR_STARTD_IP_ADDR, "STARTD_IP_ADDR", from, buffer );
	if ( parseIpPort (buffer, hk.ip_addr) < 0 ) {
		return false;
	}

	return true;
}


bool
makeScheddAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	MyString	buffer;
	MyString	buf2;

	// get the name of the schedd
	if ( !lookup( ad, "Name", "Machine", hk.name, true ) ) {
		return false;
	}

	// get the IP and port of the schedd 
	getIpAddr( ad, ATTR_SCHEDD_IP_ADDR, "SCHEDD_IP_ADDR", from, buffer );
	if ( parseIpPort (buffer, hk.ip_addr) < 0 ) {
		return false;
	}

	return true;
}


bool
makeLicenseAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	MyString buffer;
	MyString buf2;

	// get the name of the license
	if ( !lookup( ad, "Name", "Machine", hk.name, true ) ) {
		return false;
	}
	
	// get the IP and port of the startd 
	getIpAddr( ad, ATTR_MY_ADDRESS, NULL, from, buffer );
	if ( parseIpPort (buffer, hk.ip_addr) < 0 ) {
		return false;
	}

	return true;
}


bool
makeMasterAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{

	// get the name of the license
	if ( !lookup( ad, "Name", "Machine", hk.name, true ) ) {
		return false;
	}

	// ip_addr not necessary
	hk.ip_addr = "";

	return true;
}


bool
makeCkptSrvrAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
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
makeCollectorAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{

	if ( !lookup( ad, "Machine", NULL, hk.name, true ) ) {
		return false;
	}
	hk.ip_addr = "";
	return true;
}

bool
makeStorageAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	if ( !lookup( ad, "Name", NULL, hk.name, true ) ) {
		return false;
	}
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

HashString::HashString( const HashKey &hk )
		: MyString( )
{
	Build( hk );
}

void
HashString::Build( const HashKey &hk )
{
	if ( hk.ip_addr.Length() ) {
		sprintf( "< %s , %s >", hk.name.GetCStr(), hk.ip_addr.GetCStr() );
	} else {
		sprintf( "< %s >", hk.name.GetCStr() );
	}
}
