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



#if !defined( COMPRESS_H )
#define COMPRESS_H

#include "classad/classad_containers.h"
#include "classad/classad.h"

namespace classad {

class ClassAdBin
{
	public:
    ClassAdBin( );
    ~ClassAdBin( );

    int         count;
    ClassAd     *ad;
};

typedef classad_hash_map< std::string, ClassAdBin* > CompressedAds;

bool MakeSignature( ClassAd *ad, References &refs, std::string &sig );
bool Compress( ClassAdCollectionServer *server, LocalCollectionQuery *query,
			   const References &refs, CompressedAds& comp, 
			   std::list<ClassAd*> &rest);

}
#endif
