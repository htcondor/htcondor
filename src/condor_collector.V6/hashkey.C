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
extern void parseIpPort (const MyString &, MyString &);

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

// functions to make the hashkeys ...
// make hashkeys from the obtained ad
bool
makeStartdAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree	*tree;
	MyString	buffer;
	MyString	buf2;
	int		inferred = 0;

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
	
	// get the IP and port of the startd 
	tree = ad->Lookup (ATTR_STARTD_IP_ADDR);
	
	// if not there, try to lookup the old style "STARTD_IP_ADDR" string
	if (!tree) tree = ad->Lookup ("STARTD_IP_ADDR");

	if (tree)
	{
		buffer = ((String *)tree->RArg())->Value();
	}
	else
	{
		dprintf (D_FULLDEBUG,"Warning: No STARTD_IP_ADDR; inferring address\n");
		buffer = sin_to_string (from);	

		buf2.sprintf( "%s = \"%s\"", ATTR_STARTD_IP_ADDR, buffer.GetCStr() );
		ad->Insert( buf2.GetCStr() );
		dprintf (D_FULLDEBUG, "(Inferred address: %s)\n", buf2.GetCStr() );
		inferred = 1;
	}

	parseIpPort (buffer, hk.ip_addr);

	return true;
}


bool
makeScheddAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree	*tree;
	MyString	buffer;
	MyString	buf2;
	int		inferred = 0;

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
	tree = ad->Lookup (ATTR_SCHEDD_IP_ADDR);
	
	if (!tree)
		tree = ad->Lookup ("SCHEDD_IP_ADDR");

	if (tree)
	{
		buffer = ((String *)tree->RArg())->Value();
	}
	else
	{
		dprintf(D_FULLDEBUG,"Warning: No SCHEDD_IP_ADDR; inferring address\n");
		buffer = sin_to_string (from);

        // since we have done the work ...
        buf2.sprintf( "%s = \"%s\"", ATTR_SCHEDD_IP_ADDR, buffer.GetCStr() );
		ad->Insert( buf2.GetCStr() );
		dprintf (D_FULLDEBUG, "(Inferred address: %s)\n", buf2.GetCStr() );
		inferred = 1;
	}

	parseIpPort (buffer, hk.ip_addr);

	return true;
}


bool
makeLicenseAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree *tree;
	MyString buffer;
	MyString buf2;
	int  inferred = 0;

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
	tree = ad->Lookup (ATTR_MY_ADDRESS);
	
	if (tree)
	{
		buffer = ((String *)tree->RArg())->Value();
	}
	else
	{
		dprintf(D_FULLDEBUG,"Warning: No MY_ADDRESS; inferring address\n");
		buffer = sin_to_string (from);

        // since we have done the work ...
        buf2.sprintf( "%s = \"%s\"", ATTR_MY_ADDRESS, buffer.GetCStr() );
		ad->Insert (buf2.GetCStr() );
		dprintf (D_FULLDEBUG, "(Inferred address: %s)\n", buf2.GetCStr() );
		inferred = 1;
	}

	parseIpPort (buffer, hk.ip_addr);

	return true;
}


bool
makeMasterAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
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
makeStorageAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
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
makeNegotiatorAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
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

// utility function:  parse the string <aaa.bbb.ccc.ddd:pppp>
void 
parseIpPort (const MyString &ip_port_pair, MyString &ip_addr)
{
    const char *ip_port = ip_port_pair.GetCStr() + 1;
	ip_addr = "";
    while (*ip_port != ':')
    {
		ip_addr += *ip_port;
        ip_port++;
    }

	// don't care about port number
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
