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
#include "HashTable.h"
#include "exprTree.h"
#include "collection.h"
#include "collectionServer.h"
#include "transaction.h"

#include <list>

BEGIN_NAMESPACE( classad )

template class ExtArray<ExprTree*>;
template class ExtArray<char>;

template class HashTable<const ExprTree*, Value>;
template class HashBucket<const ExprTree*, Value>;
template class HashTable<const ExprTree*, bool>;
template class HashBucket<const ExprTree*, bool>;

template class map<string, FunctionCall::ClassAdFunc, CaseIgnLTStr>;
template class map<string, FunctionCall::ClassAdFunc, CaseIgnLTStr>::iterator;
template class map<string, ExprTree*, CaseIgnLTStr>;
template class map<string, ExprTree*, CaseIgnLTStr>::iterator;
template class vector< pair<string, ExprTree*> >;

template class multiset<ViewMember, ViewMemberLT>;
template class multiset<ViewMember, ViewMemberLT>::iterator;
template class slist<View*>;
template class hash_map<string,View*,StringHash>;
template class hash_map<string,View*,StringHash>::iterator;

template class hash_map<string,multiset<ViewMember,ViewMemberLT>::iterator,
	StringHash>::iterator;
template class hash_map<string,multiset<ViewMember,ViewMemberLT>::iterator,
	StringHash>;

template class hash_map<string, ClassAdProxy, StringHash>;
template class hash_map<string, ClassAdProxy, StringHash>::iterator;
template class hash_map<string, ClientTransaction*, StringHash>;
template class hash_map<string, ClientTransaction*, StringHash>::iterator;
template class hash_map<string, ServerTransaction*, StringHash>;
template class hash_map<string, ServerTransaction*, StringHash>::iterator;

template class hash_map<string, bool, StringHash>;
template class hash_map<string, bool, StringHash>::iterator;
template class vector<string>;
template class vector<ExprTree*>;
template class list<XactionRecord>;
template class list< pair<string,ClassAd*> >;


class _ClassAdInit 
{
	public:
		_ClassAdInit( ) { tzset( ); }
} __ClassAdInit;


END_NAMESPACE
