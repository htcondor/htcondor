#include "condor_common.h"
#include "HashTable.h"
#include "extArray.h"
#include "list.h"
#include "exprTree.h"
#include "classad_collection.h"

template class HashTable<MyString,classad::FunctionCall::ClassAdFunc>;
template class HashBucket<MyString,classad::FunctionCall::ClassAdFunc>;

template class ExtArray<classad::Attribute>;
template class ExtArray<classad::ExprTree*>;
template class ExtArray<char>;
template class ExtArray<const char*>;

template class List<classad::Value>; template class Item<classad::Value>;
template class List<classad::ExprTree>;	template class Item<classad::ExprTree>;

template class HashTable<classad::ExprTree*, classad::Value>;
template class HashBucket<classad::ExprTree*, classad::Value>;
template class HashTable<classad::ExprTree*, bool>;
template class HashBucket<classad::ExprTree*, bool>;

template class ListIterator<classad::ExprTree>;

template class HashTable<int, classad::BaseCollection*>;
template class HashBucket<int, classad::BaseCollection*>;
template class HashTable<MyString, classad::ClassAd *>;
template class HashBucket<MyString, classad::ClassAd *>;
template class Set<MyString>;
template class SetElem<MyString>;
template class SetIterator<MyString>;
template class Set<int>;
template class SetElem<int>;
template class SetIterator<int>;
template class Set<classad::RankedClassAd>;
template class SetElem<classad::RankedClassAd>;
template class SetIterator<classad::RankedClassAd>;
template class ExtArray<classad::CollChildIterator*>;
template class ExtArray<classad::CollContentIterator*>;

class _ClassAdInit 
{
	public:
		_ClassAdInit( ) { tzset( ); }
} __ClassAdInit;
