#include "condor_common.h"
#include "MyString.h"
#include "HashTable.h"
#include "view_server.h"

template class HashTable<MyString,ClassAd*>;
template class HashTable<MyString, GeneralRecord*>;
template class HashBucket<MyString, GeneralRecord*>;
