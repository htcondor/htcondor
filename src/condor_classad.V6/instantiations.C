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

#if (__GNUC__<3)
#define CLASS
#else
#define CLASS class
#endif


#if (__GNUC__>=3) && (__GNUC_MINOR__ >= 4)
#define DEREF_TYPEDEFS
#endif

BEGIN_NAMESPACE( classad )
//-------------classad templates --------------

template CLASS map<string, bool>;

#ifdef DEREF_TYPEDEFS
template CLASS _Rb_tree_iterator<pair<string, bool> >;
#else
template CLASS map<string, bool, CaseIgnLTStr>::iterator;
#endif

// function table
template CLASS map<string, void*, CaseIgnLTStr>;
#ifdef DEREF_TYPEDEFS
template CLASS _Rb_tree_iterator<pair<string, void *> >;
#else
template CLASS map<string, void*, CaseIgnLTStr>::iterator;
#endif 

// XML attributes
template CLASS map<string, string>;
#ifdef DEREF_TYPEDEFS
template CLASS _Rb_tree_iterator<pair<string, string> >;
#else
template CLASS map<string, string>::iterator;
#endif

// attribute list
template CLASS hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>;
#ifdef DEREF_TYPEDEFS
template CLASS __gnu_cxx::_Hashtable_iterator<pair< string, ExprTree*>, string, StringCaseIgnHash, 
       _Select1st<pair<string, ExprTree*> >, CaseIgnEqStr, 
       allocator<ExprTree*> >;
template CLASS __gnu_cxx::_Hashtable_iterator<pair< string const, ExprTree*>, string const, StringCaseIgnHash, 
       _Select1st<pair<string const, ExprTree*> >, CaseIgnEqStr, 
       allocator<ExprTree*> >;
#else
template CLASS hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>::iterator;
template CLASS hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>::const_iterator;
#endif

template CLASS set<string, CaseIgnLTStr>;
#ifdef DEREF_TYPEDEFS
template CLASS _Rb_tree_iterator<string>;
#else
template CLASS set<string, CaseIgnLTStr>::iterator;
#endif

// expr evaluation cache
template CLASS hash_map<const ExprTree*, Value, ExprHash >;
#ifdef DEREF_TYPEDEFS
template CLASS __gnu_cxx::_Hashtable_iterator<pair<const ExprTree *, Value>, const ExprTree *, ExprHash,
       _Select1st<pair<const ExprTree *, Value> >, ExprHash,
       allocator<Value> >;
#else
template CLASS hash_map<const ExprTree*, Value, ExprHash >::iterator;
#endif

// component stuff
template CLASS vector< pair<string, ExprTree*> >;
template CLASS vector<ExprTree*>;

template CLASS map<const ClassAd*, References>;

#ifdef CLASSAD_DISTRIBUTION
END_NAMESPACE
template CLASS vector<string>;
#include "transaction.h"
#include "view.h"
BEGIN_NAMESPACE(classad)

// view content
template CLASS multiset<ViewMember, ViewMemberLT>;
#ifdef DEREF_TYPEDEFS
template CLASS _Rb_tree_iterator<ViewMember>;
#else
template CLASS multiset<ViewMember, ViewMemberLT>::iterator;
#endif

// list of sub-views
template CLASS slist<View*>;

// view registry
template CLASS hash_map<string,View*,StringHash>;
#ifdef DEREF_TYPEDEFS
template CLASS __gnu_cxx::_Hashtable_iterator<pair<string, View*>, string, StringHash,
       _Select1st<pair<string, View*> >, equal_to<string>,
       allocator<View *> >;
#else
template CLASS hash_map<string,View*,StringHash>::iterator;
#endif

// index
template CLASS hash_map<string,multiset<ViewMember,ViewMemberLT>::iterator,
    StringHash>;
#ifdef DEREF_TYPEDEFS
template CLASS __gnu_cxx::_Hashtable_iterator<pair< string, _Rb_tree_iterator<ViewMember> >, string, StringHash, 
       _Select1st<pair<string, _Rb_tree_iterator<ViewMember> > >, equal_to<string>,
       allocator<_Rb_tree_iterator<ViewMember> > >;
#else
template CLASS hash_map<string,multiset<ViewMember,ViewMemberLT>::iterator,
    StringHash>::iterator;
#endif

// main classad table
template CLASS hash_map<string, ClassAdProxy, StringHash>;
#ifdef DEREF_TYPEDEFS
template CLASS __gnu_cxx::_Hashtable_iterator<pair< string, ClassAdProxy>, string, StringHash, 
       _Select1st<pair<string, ClassAdProxy> >, equal_to<string>,
       allocator<ClassAdProxy> >;
#else
template CLASS hash_map<string, ClassAdProxy, StringHash>::iterator;
#endif

// index file
template CLASS map<string, int>;
template CLASS hash_map<string,int,StringHash>;
#ifdef DEREF_TYPEDEFS
template CLASS __gnu_cxx::_Hashtable_iterator<pair< string, int>, string, StringHash, 
       _Select1st<pair<string, int> >, equal_to<string>,
       allocator<int> >;
#else
template CLASS hash_map<string,int,StringHash>::iterator;
#endif

// transaction registry
template CLASS hash_map<string, ServerTransaction*, StringHash>;
#ifdef DEREF_TYPEDEFS
template CLASS __gnu_cxx::_Hashtable_iterator<pair< string, ServerTransaction*>, string, StringHash, 
       _Select1st<pair<string, ServerTransaction*> >, equal_to<string>,
       allocator<ServerTransaction*> >;
#else
template CLASS hash_map<string, ServerTransaction*, StringHash>::iterator;
#endif

// operations in transaction
template CLASS list<XactionRecord>;

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

