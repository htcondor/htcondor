#include "HashTable.h"
#include "extArray.h"
#include "list.h"
#include "exprTree.h"

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

class _ClassAdInit 
{
	public:
		_ClassAdInit( ) { tzset( ); }
} __ClassAdInit;
