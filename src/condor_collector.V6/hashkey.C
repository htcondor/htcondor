#include <stdlib.h>

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_classad.h"
#ifdef __GNUG__
#pragma implementation "HashTable.h"
#endif
#include "HashTable.h"
#include "hashkey.h"
#include "sched.h"
#include "condor_attributes.h"

#ifndef WIN32
#include <netinet/in.h>
#endif

extern "C" char * sin_to_string(struct sockaddr_in *);

template class HashTable<HashKey, ClassAd *>;
extern void parseIpPort (char *, char *);

void HashKey::sprint (char *s)
{
	if (ip_addr[0])
		sprintf (s, "< %s , %s >", name, ip_addr);
	else
		sprintf (s, "< %s >", name);
}

bool operator== (HashKey &lhs, HashKey &rhs)
{
    return (strcmp (lhs.name, rhs.name) == 0    &&
			strcmp (lhs.ip_addr, rhs.ip_addr) == 0);
}

ostream &operator<< (ostream &out, const HashKey &hk)
{
	out << "Hashkey: (" << hk.name << "," << hk.ip_addr;
	out << ")" << endl;
	return out;
}

int hashFunction (HashKey &key, int numBuckets)
{
    unsigned int bkt = 0;
    int i;

    for (i = 0; key.name[i]   ; bkt += key.name[i++]);
    for (i = 0; key.ip_addr[i]; bkt += key.ip_addr[i++]);

    bkt %= numBuckets;

    return bkt;
}

int hashOnName (HashKey &key, int numBuckets)
{
	unsigned int bkt = 0;
	int i;

	for (i = 0; key.name [i]; bkt += key.name[i++]);
	bkt %= numBuckets;

	return bkt;
}

// functions to make the hashkeys ...
// make hashkeys from the obtained ad
bool
makeStartdAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree *tree;
	char buffer [30];
	char buf2   [64];
	int  inferred = 0;

	// get the name of the startd
	if (!(tree = ad->Lookup ("Name")))
	{		
		// ... if name was not specified
		dprintf(D_FULLDEBUG,"Warning: No 'Name' attribute; trying 'Machine'\n");
		tree = ad->Lookup ("Machine");
	}

	if (tree)
	{
		strcpy (hk.name, ((String *)tree->RArg())->Value());
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
		strcpy (buffer, ((String *)tree->RArg())->Value());
	}
	else
	{
		dprintf (D_FULLDEBUG,"Warning: No STARTD_IP_ADDR; inferring address\n");
		strcpy (buffer, sin_to_string (from));	

		sprintf (buf2, "%s = \"%s\"", ATTR_STARTD_IP_ADDR, buffer);
		ad->Insert (buf2);
		dprintf (D_FULLDEBUG, "(Inferred address: %s)\n", buf2);
		inferred = 1;
	}

	parseIpPort (buffer, hk.ip_addr);

	return true;
}


bool
makeScheddAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree *tree;
	char buffer [30];
	char buf2   [64];
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
		strcpy (hk.name, ((String *)tree->RArg())->Value());
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
		strcpy (buffer, ((String *)tree->RArg())->Value());
	}
	else
	{
		dprintf(D_FULLDEBUG,"Warning: No SCHEDD_IP_ADDR; inferring address\n");
		strcpy (buffer, sin_to_string (from));

        // since we have done the work ...
        sprintf (buf2, "%s = \"%s\"", ATTR_SCHEDD_IP_ADDR, buffer);
		ad->Insert (buf2);
		dprintf (D_FULLDEBUG, "(Inferred address: %s)\n", buf2);
		inferred = 1;
	}

	parseIpPort (buffer, hk.ip_addr);

	return true;
}


bool
makeMasterAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	ExprTree *tree;

	tree = ad->Lookup ("Machine");
	if (tree)
	{
		strcpy (hk.name, ((String *)tree->RArg())->Value ());
	}
	else
	{
		dprintf (D_ALWAYS, "Error: No 'Machine' attribute\n");
		return false;
	}

	// ip_addr not necessary
	hk.ip_addr [0] = '\0';

	return true;
}


bool
makeCkptSrvrAdHashKey (HashKey &hk, ClassAd *ad, sockaddr_in *from)
{
	if (!ad->LookupString ("Machine", hk.name))
	{
		dprintf (D_ALWAYS, "Error:  No 'Machine' attribute\n");
		return false;
	}

	hk.ip_addr[0] = '\0';

	return true;
}

// utility function:  parse the string <aaa.bbb.ccc.ddd:pppp>
void 
parseIpPort (char *ip_port_pair, char *ip_addr)
{
    char *ip_port = ip_port_pair + 1;
    char *ip = ip_addr;
    while (*ip_port != ':')
    {
        *ip = *ip_port;
        ip++;
        ip_port++;
    }

    *ip = '\0';

	// don't care about port number
}


