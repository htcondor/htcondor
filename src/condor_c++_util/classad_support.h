#ifndef CLASSAD_SUPPORT_H
#define CLASSAD_SUPPORT_H

#include "condor_common.h"
#include "condor_classad.h"
#include "string_list.h"

#define DIRTY_ATTR_SIZE (1024*50)

extern char ATTR_DIRTY_ATTR_LIST [];

void SetAttrDirty(ClassAd *ad, char *attr);
void SetAttrClean(ClassAd *ad, char *attr);
bool IsAttrDirty(ClassAd *ad, char *attr);
bool AnyAttrDirty(ClassAd *ad);
void EmitDirtyAttrList(int mode, ClassAd *ad);

#endif
