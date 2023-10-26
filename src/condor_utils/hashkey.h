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
#include "condor_sockaddr.h"

// this is the tuple that we will be hashing on
class AdNameHashKey
{
  public:
	std::string name;
	std::string ip_addr;

	void sprint( std::string & ) const;
    friend bool operator== (const AdNameHashKey &, const AdNameHashKey &);

};

// the hash functions
size_t adNameHashFunction (const AdNameHashKey &);

// functions to make the hashkeys
bool makeStartdAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeScheddAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeLicenseAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeMasterAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeCkptSrvrAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeCollectorAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeStorageAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeAccountingAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeNegotiatorAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeHadAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeGridAdHashKey (AdNameHashKey &, const ClassAd *);
bool makeGenericAdHashKey (AdNameHashKey &, const ClassAd *);

#endif /* __COLLHASH_H__ */
