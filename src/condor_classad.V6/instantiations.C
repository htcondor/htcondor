/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
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

// expr evaluation cache
template hash_map<const ExprTree*, Value, ExprHash >;
template hash_map<const ExprTree*, Value, ExprHash >::iterator;

// component stuff
template vector< pair<string, ExprTree*> >;
template vector<ExprTree*>;

template map<string, ClassAd *>;
template map<string, ClassAdCollection *>;

#ifdef CLASSAD_DISTRIBUTION
template vector<string>;
#include "transaction.h"
#include "view.h"

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

#if (__GNUC__>=3)
template string std::operator+<char, std::char_traits<char>, std::allocator<char> >(const string&, const string&);
#endif

#endif

class _ClassAdInit 
{
	public:
		_ClassAdInit( ) { tzset( ); }
} __ClassAdInit;

END_NAMESPACE
