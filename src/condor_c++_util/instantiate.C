#include <stdlib.h>

#pragma implementation "list.h"
#include "list.h"
#include "proc_obj.h"
#include "filter.h"

#pragma implementation "simplelist.h"
#include "simplelist.h"

typedef List<FilterObj> listfilterobj;
typedef List<ProcObj> listprocobj;
typedef List<char> listchar;
typedef List<int> listint;

typedef SimpleList<int> slistint;
typedef SimpleList<float> slistfloat;
