#include "condor_classad.h"
#include "ad_printmask.h"
#include "HashCache.h"
#include "MyString.h"

template class List<Formatter>;
template class Item<Formatter>;
template class HashCache<MyString,ClassAd*>;
template class HashCacheEntry<ClassAd*>;
template class HashTable<MyString,HashCacheEntry<ClassAd*> >;
