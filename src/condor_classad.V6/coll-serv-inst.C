/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#include "condor_common.h"
#include "common.h"
#include "collection.h"
#include "collectionServer.h"
#include "transaction.h"

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

