#include "condor_common.h"
#include "condor_classad.h"
#include "ad_printmask.h"
#include "ListCache.h"

template class List<Formatter>;
template class Item<Formatter>;
template class ListCache<ClassAd>;
template class ListCacheEntry<ClassAd>;
template class List<ListCacheEntry<ClassAd> >;
template class Item<ListCacheEntry<ClassAd> >;
template class ListIterator<ListCacheEntry<ClassAd> >;
