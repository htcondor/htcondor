#ifndef CLASSAD_SUPPORT_H
#define CLASSAD_SUPPORT_H

#include "condor_common.h"
#include "condor_classad.h"
#include "string_list.h"

#define DIRTY_ATTR_SIZE (1024*50)

extern char ATTR_DIRTY_ATTR_LIST [];

/* marks the given attr in the classad as "dirty" */
void SetAttrDirty(ClassAd *ad, char *attr);

/* marks the given attr in the classad as "clean" */
void SetAttrClean(ClassAd *ad, char *attr);

/* returns true if given attr is dirty in the classad */
bool IsAttrDirty(ClassAd *ad, char *attr);

/* returns true if any attr is dirty in the classad */
bool AnyAttrDirty(ClassAd *ad);

/* Print out with specified dprintf mode the dirty list in the classad */
void EmitDirtyAttrList(int mode, ClassAd *ad);

#endif
