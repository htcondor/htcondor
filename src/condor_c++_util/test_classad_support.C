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

