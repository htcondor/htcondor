#include "HashTable.h"
#include "extArray.h"
#include "list.h"
#include "exprTree.h"

template class HashTable<MyString,FunctionCall::ClassAdFunc>;
template class HashBucket<MyString,FunctionCall::ClassAdFunc>;

template class ExtArray<Attribute>;
template class ExtArray<ExprTree*>;

template class List<Value>;		template class Item<Value>;
template class List<ExprTree>;	template class Item<ExprTree>;

