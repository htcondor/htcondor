/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
