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


#include "condor_common.h"
#include "classad/common.h"
#include "classad/collection.h"
#include "collectionServer.h"
#include "classad/transaction.h"

using namespace std;

BEGIN_NAMESPACE( classad )

//-------------collection server templates-------------
    // view content
template multiset<ViewMember, ViewMemberLT>;
template multiset<ViewMember, ViewMemberLT>::iterator;
    // list of sub-views
template slist<View*>;
    // view registry
template hash_map<string,View*,StringHash>;
template hash_map<string,View*,StringHash>::iterator;
    // index
template hash_map<string,multiset<ViewMember,ViewMemberLT>::iterator,
    StringHash>::iterator;
template hash_map<string,multiset<ViewMember,ViewMemberLT>::iterator,
    StringHash>;
    // main classad table
template map<string, int>;
template hash_map<string,int,StringHash>;
template hash_map<string,int,StringHash>::iterator;


template hash_map<string, ClassAdProxy, StringHash>;
template hash_map<string, ClassAdProxy, StringHash>::iterator;
    // transaction registry
template hash_map<string, ServerTransaction*, StringHash>;
template hash_map<string, ServerTransaction*, StringHash>::iterator;
    // operations in transaction
template list<XactionRecord>;

END_NAMESPACE

