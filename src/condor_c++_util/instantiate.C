#include "condor_common.h"
#include "list.h"
#include "filter.h"
#include "simplelist.h"
#include "extArray.h"
#include "stringSpace.h"

template class List<FilterObj>; template class Item<FilterObj>;
template class List<char>; 		template class Item<char>;
template class List<int>; 		template class Item<int>;
template class SimpleList<int>; 
template class SimpleList<float>;
template class ExtArray<char *>;
template class ExtArray<StringSpace::SSStringEnt>;
template class ExtArray<StringSpace*>;
