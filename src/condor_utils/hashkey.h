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

#ifndef __COLLHASH_H__
#define __COLLHASH_H__

#include "condor_common.h"
#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <netinet/in.h>
#endif
#include "condor_classad.h"

#include "HashTable.h"

// this is the tuple that we will be hashing on
class AdNameHashKey
{
  public:
    MyString name;
    MyString ip_addr;

	void   sprint (MyString &);
    friend bool operator== (const AdNameHashKey &, const AdNameHashKey &);

};

// the hash functions
unsigned int adNameHashFunction (const AdNameHashKey &);
unsigned int stringHashFunction (const MyString &);

// type for the hash tables ...
typedef HashTable <AdNameHashKey, ClassAd *> CollectorHashTable;
typedef HashTable <MyString, CollectorHashTable *> GenericAdHashTable;

// functions to make the hashkeys
bool makeStartdAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeQuillAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeScheddAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeLicenseAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeMasterAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeCkptSrvrAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeCollectorAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeStorageAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeNegotiatorAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeHadAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeXferServiceAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeLeaseManagerAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeGridAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);
bool makeGenericAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);

// utility function:  parse the string <aaa.bbb.ccc.ddd:pppp>
bool parseIpPort( const MyString &ip_port_pair, MyString &ip_addr );

class HashString : public MyString
{
  public:
	HashString( void );
	HashString( const AdNameHashKey & );
	void Build( const AdNameHashKey & );
};

#endif /* __COLLHASH_H__ */
