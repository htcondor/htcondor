#include "condor_common.h"
#include "HashTable.h"
#include "extArray.h"
#include "list.h"
#include "exprTree.h"
#include "collection.h"

BEGIN_NAMESPACE( classad )

template class HashTable<MyString,FunctionCall::ClassAdFunc>;
template class HashBucket<MyString,FunctionCall::ClassAdFunc>;

template class ExtArray<Attribute>;
template class ExtArray<ExprTree*>;
template class ExtArray<char>;
template class ExtArray<const char*>;

template class List<Value>; template class Item<Value>;
template class List<ExprTree>;	template class Item<ExprTree>;

template class HashTable<ExprTree*, Value>;
template class HashBucket<ExprTree*, Value>;
template class HashTable<ExprTree*, bool>;
template class HashBucket<ExprTree*, bool>;

template class ListIterator<ExprTree>;


template class multiset<ViewMember, ViewMemberLT>;
template class multiset<ViewMember, ViewMemberLT>::iterator;
template class slist<View*>;
template class hash_map<string,View*,hash<const string &> >;
template class hash_map<string,View*,hash<const string &> >::iterator;

template class hash_map<string,multiset<ViewMember,ViewMemberLT>::iterator,
	hash<const string &> >::iterator;
template class hash_map<string,multiset<ViewMember,ViewMemberLT>::iterator,
	hash<const string &> >;

template class hash_map<string, ClassAdProxy, hash<const string &> >;
template class hash_map<string, ClassAdProxy, hash<const string &> >::
	iterator;
template class hash_map<string, Transaction*, hash<const string & > >;
template class hash_map<string, Transaction*, hash<const string & > >
	::iterator;
template class vector<string>;


class _ClassAdInit 
{
	public:
		_ClassAdInit( ) { tzset( ); }
} __ClassAdInit;

END_NAMESPACE
