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
class AdNameHashKey
{
  public:
    MyString name;
    MyString ip_addr;

	void   sprint (MyString &);
	friend ostream& operator<< (ostream &out, const AdNameHashKey &); 
    friend bool operator== (const AdNameHashKey &, const AdNameHashKey &);

};

// the hash functions
int adNameHashFunction (const AdNameHashKey &, int);
int stringHashFunction (const MyString &, int);

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
bool makeGenericAdHashKey (AdNameHashKey &, ClassAd *, sockaddr_in *);

class HashString : public MyString
{
  public:
	HashString( void );
	HashString( const AdNameHashKey & );
	void Build( const AdNameHashKey & );
};

#endif /* __COLLHASH_H__ */
