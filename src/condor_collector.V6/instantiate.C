#include "condor_common.h"
#include "MyString.h"
#include "Set.h"
#include "HashTable.h"
#include "view_server.h"

template class Set<MyString>;
template class SetElem<MyString>;
template class HashTable<MyString, GeneralRecord*>;
template class HashBucket<MyString, GeneralRecord*>;
