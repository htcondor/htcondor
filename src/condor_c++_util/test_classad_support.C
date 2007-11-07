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

