#include "condor_common.h"

#pragma implementation "list.h"
#include "list.h"
#include "filter.h"
#include "ad_printmask.h"

#pragma implementation "simplelist.h"
#include "simplelist.h"

#pragma implementation "extArray.h"
#include "extArray.h"

typedef List<FilterObj> listfilterobj;
typedef List<char> listchar;
typedef List<int> listint;
typedef List<Formatter> listFormatter;

typedef SimpleList<int> slistint;
typedef SimpleList<float> slistfloat;

typedef ExtArray<char *> extStringArray;

class StringSpace;

#include "stringSpace.h"

typedef ExtArray<StringSpace::SSStringEnt> extSSStringEntArray;
typedef ExtArray<StringSpace *> extStrSpaceArray;
