/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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
	ExprTree *tree;
	MyString	buffer;
	MyString	buf2;
	int  inferred = 0;
	ClassAdUnParser unp;
	string buffString;

	// get the name of the startd
	if (!(tree = ad->Lookup ("Name")))
	{		
		// ... if name was not specified
		dprintf(D_FULLDEBUG,"Warning: No 'Name' attribute; trying 'Machine'\n");
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		unp.Unparse( buffString, tree );
//		strcpy (hk.name, ((String *)tree->RArg())->Value());
		hk.name = buffString.c_str( );
		buffString = "";
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
		unp.Unparse( buffString, tree );
//		strcpy (buffer, ((String *)tree->RArg())->Value());
		buffer = buffString.c_str( );
		buffString = "";
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
	ExprTree *tree;
	MyString	buffer;
	MyString	buf2;
	int  inferred = 0;
	ClassAdUnParser unp;
	string buffString;

	// get the name of the startd
	if (!(tree = ad->Lookup ("Name")))
	{
		dprintf(D_FULLDEBUG,"Warning: No 'Name' attribute; trying 'Machine'\n");
		// ... if name was not specified
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		unp.Unparse( buffString, tree );
//		strcpy (hk.name, ((String *)tree->RArg())->Value());
		hk.name = buffString.c_str( );
		buffString = "";
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
		unp.Unparse( buffString, tree );
//		strcpy (buffer, ((String *)tree->RArg())->Value());
		buffer = buffString.c_str( );
		buffString = "";
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
	ClassAdUnParser unp;
	string buffString;

	// get the name of the startd
	if (!(tree = ad->Lookup ("Name")))
	{
		dprintf(D_FULLDEBUG,"Warning: No 'Name' attribute; trying 'Machine'\n");
		// ... if name was not specified
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		unp.Unparse( buffString, tree );
//		strcpy (hk.name, ((String *)tree->RArg())->Value());
		hk.name = buffString.c_str( );
		buffString = "";
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
//		strcpy (buffer, ((String *)tree->RArg())->Value());
		buffer = buffString.c_str( );
		buffString = "";
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
	ClassAdUnParser unp;
	string buffString;

	if (!(tree = ad->Lookup ("Name")))
	{
		dprintf( D_FULLDEBUG, 
				 "Warning: No 'Name' attribute; trying 'Machine'\n" );
			// ... if name was not specified
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		unp.Unparse( buffString, tree );
//		strcpy (hk.name, ((String *)tree->RArg())->Value ());
		hk.name = buffString.c_str( );
		buffString = "";
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
