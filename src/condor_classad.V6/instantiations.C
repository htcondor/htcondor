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

#include "common.h"
#include "lexer.h"
#include "exprTree.h"
#include "collection.h"
#include "collectionBase.h"
#include "classad.h"

using namespace std;

BEGIN_NAMESPACE( classad )
//-------------classad templates --------------

template map<string, bool>;

// function table
template map<string, void*, CaseIgnLTStr>;
template map<string, void*, CaseIgnLTStr>::iterator;

// XML attributes
template map<string, string>;
template map<string, string>::iterator;

// attribute list
template hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>;
template hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>::iterator;
template hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>::const_iterator;
template set<string, CaseIgnLTStr>;
template set<string, CaseIgnLTStr>::iterator;

// expr evaluation cache
template hash_map<const ExprTree*, Value, ExprHash >;
template hash_map<const ExprTree*, Value, ExprHash >::iterator;

// component stuff
template vector< pair<string, ExprTree*> >;
template vector<ExprTree*>;

#ifdef CLASSAD_DISTRIBUTION
END_NAMESPACE
template vector<string>;
#include "transaction.h"
#include "view.h"
BEGIN_NAMESPACE(classad)

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
template hash_map<string, ClassAdProxy, StringHash>;
template hash_map<string, ClassAdProxy, StringHash>::iterator;

// index file
template map<string, int>;
template hash_map<string,int,StringHash>;
template hash_map<string,int,StringHash>::iterator;

// transaction registry
template hash_map<string, ServerTransaction*, StringHash>;
template hash_map<string, ServerTransaction*, StringHash>::iterator;

// operations in transaction
template list<XactionRecord>;

#endif

class _ClassAdInit 
{
	public:
		_ClassAdInit( ) { tzset( ); }
} __ClassAdInit;

END_NAMESPACE

#if (__GNUC__>=3)
template string std::operator+<char, std::char_traits<char>, std::allocator<char> >(const string&, const string&);
#endif
