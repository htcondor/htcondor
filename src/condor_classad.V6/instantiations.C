#include "HashTable.h"
#include "extArray.h"
#include "list.h"
#include "condor_classad.h"

template class HashTable<MyString,FunctionCall::ClassAdFunc>;
template class HashBucket<MyString,FunctionCall::ClassAdFunc>;

template class ExtArray<Attribute>;
template class ExtArray<ExprTree*>;
template class ExtArray<char>;
template class ExtArray<const char*>;

template class List<Value>;		template class Item<Value>;
template class List<ExprTree>;	template class Item<ExprTree>;

template class HashTable<ExprTree*, Value>;
template class HashBucket<ExprTree*, Value>;
template class HashTable<ExprTree*, bool>;
template class HashBucket<ExprTree*, bool>;

template class ListIterator<ExprTree>;

template class HashTable<int, BaseCollection*>;
template class HashBucket<int, BaseCollection*>;
template class HashTable<MyString, ClassAd *>;
template class HashBucket<MyString, ClassAd *>;
template class Set<MyString>;
template class SetElem<MyString>;
template class SetIterator<MyString>;
template class Set<int>;
template class SetElem<int>;
template class SetIterator<int>;
template class Set<RankedClassAd>;
template class SetElem<RankedClassAd>;
template class SetIterator<RankedClassAd>;
template class ExtArray<CollChildIterator*>;
template class ExtArray<CollContentIterator*>;

class _ClassAdInit 
{
	public:
		_ClassAdInit( ) { tzset( ); }
} __ClassAdInit;
