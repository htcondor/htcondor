/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef CLASSAD_SUPPORT_H
#define CLASSAD_SUPPORT_H

/* This code may or may not be what you want. Please compare it with
 * the dirty attribute stuff that has been added to Old ClassAds. 
 * Currently, no one uses it the code in this file. 
 */

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
