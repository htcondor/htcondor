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

