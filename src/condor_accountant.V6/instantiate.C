#include "condor_common.h"
#include "condor_accountant.h"

template class HashTable<MyString, Accountant::CustomerRecord*>;
template class HashBucket<MyString,Accountant::CustomerRecord*>;
template class HashTable<MyString, Accountant::ResourceRecord*>;
template class HashBucket<MyString,Accountant::ResourceRecord*>;
template class Set<MyString>;
template class SetElem<MyString>;

