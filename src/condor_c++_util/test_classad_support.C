/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "classad_support.h"

int main(void)
{
	ClassAd *ad = new ClassAd;
	
	SetAttrDirty(ad, "foo");
	SetAttrDirty(ad, "bar");
	SetAttrDirty(ad, "foobar");
	/* these better not get repeated in the dirty list */
	SetAttrDirty(ad, "bar");
	SetAttrDirty(ad, "foobar");
	SetAttrDirty(ad, "foo");
	EmitDirtyAttrList(D_ALWAYS, ad);

	SetAttrClean(ad, "bar");
	EmitDirtyAttrList(D_ALWAYS, ad);
	SetAttrClean(ad, "foo");
	EmitDirtyAttrList(D_ALWAYS, ad);
	SetAttrClean(ad, "foobar");
	EmitDirtyAttrList(D_ALWAYS, ad);

	SetAttrDirty(ad, "foo");
	SetAttrDirty(ad, "bar");
	SetAttrDirty(ad, "foobar");
	if (AnyAttrDirty(ad) == TRUE)
	{
		printf("AnyAttrDirty supposed to return true and did so.\n");
	}
	else
	{
		printf("AnyAttrDirty supposed to return true and did NOT!\n");
	}

	SetAttrClean(ad, "foobar");
	SetAttrClean(ad, "foo");
	SetAttrClean(ad, "bar");
	SetAttrClean(ad, "foobar");
	SetAttrClean(ad, "foo");
	SetAttrClean(ad, "bar");
	EmitDirtyAttrList(D_ALWAYS, ad);
	if (AnyAttrDirty(ad) == FALSE)
	{
		printf("AnyAttrDirty supposed to return false and did so.\n");
	}
	else
	{
		printf("AnyAttrDirty supposed to return false and did NOT!\n");
	}

	SetAttrDirty(ad, "foobar");
	if (IsAttrDirty(ad, "foobar") == TRUE)
	{
		printf("IsAttrDirty supposed to return true and did so.\n");
	}
	else
	{
		printf("IsAttrDirty supposed to return true and did NOT!\n");
	}

	if (IsAttrDirty(ad, "foo") == FALSE)
	{
		printf("IsAttrDirty supposed to return false and did so.\n");
	}
	else
	{
		printf("IsAttrDirty supposed to return false and did NOT!\n");
	}


	delete ad;

	return(0);
}

