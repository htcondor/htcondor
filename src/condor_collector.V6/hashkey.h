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
#ifndef __COLLHASH_H__
#define __COLLHASH_H__

#include "condor_common.h"
#include <iostream.h>
#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <netinet/in.h>
#endif
#include "condor_classad.h"

#include "HashTable.h"

// this is the tuple that we will be hashing on
class HashKey
{
    public:

    char name    [64];
    char ip_addr [16];

	void   sprint (char *);
	friend ostream& operator<< (ostream &out, const HashKey &); 
    friend bool operator== (const HashKey &, const HashKey &);
};

// the hash functions
int hashFunction (const HashKey &, int);
int hashOnName   (const HashKey &, int);

// type for the hash tables ...
typedef HashTable <HashKey, ClassAd *> CollectorHashTable;

// functions to make the hashkeys
bool makeStartdAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeScheddAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeLicenseAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeMasterAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeCkptSrvrAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeCollectorAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeStorageAdHashKey (HashKey &, ClassAd *, sockaddr_in *);

#endif /* __COLLHASH_H__ */





