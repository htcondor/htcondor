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
#include "string_list.h"
#include "classad_support.h"
#include "condor_debug.h"

char ATTR_DIRTY_ATTR_LIST [] = "DirtyAttrList";

static char *assign = " = ";


/* if the attr isn't already in the dirty list, place it in there */
void SetAttrDirty(ClassAd *ad, char *attr)
{
	char dirty[DIRTY_ATTR_SIZE];
	StringList sl;
	char *tmp, *tmp2;

	if (!ad->LookupString(ATTR_DIRTY_ATTR_LIST, dirty))
	{
		/* doesn't exist, so I'll add it */
		sl.initializeFromString(attr);
	}
	else
	{
		/* it does exist, so see if it is in the list and add it if not */
		sl.initializeFromString(dirty);
		if (sl.contains(attr) == TRUE)
		{
			/* already marked dirty, do nothing */
			return;
		}
		sl.append(attr);
	}

	/* convert the StringList back into a form the attribute can be set with */
	tmp = sl.print_to_string();

	/* length of the list, plus the attribute, plus the assignment, plus 2
		for the double quotes, plus 1 for nul */
	tmp2 = (char*)calloc(strlen(tmp) + strlen(ATTR_DIRTY_ATTR_LIST) 
						+ strlen(assign) + 2 + 1, 1);
	if (tmp2 == NULL)
	{
		EXCEPT("Out of memory in SetAttrDirty()");
	}

	strcpy(tmp2, ATTR_DIRTY_ATTR_LIST);
	strcat(tmp2, assign);
	strcat(tmp2, "\"");
	strcat(tmp2, tmp);
	strcat(tmp2, "\"");

	ad->Delete(ATTR_DIRTY_ATTR_LIST);
	ad->Insert(tmp2);

	free(tmp);
	free(tmp2);
}

/* if the attribute exists in the classad as dirty, then remove it from the
	dirty list. */
void SetAttrClean(ClassAd *ad, char *attr)
{
	char dirty[DIRTY_ATTR_SIZE];
	StringList sl;
	char *tmp, *tmp2;

	/* no dirty list means this is automatically clean */
	if (!ad->LookupString(ATTR_DIRTY_ATTR_LIST, dirty))
	{
		return;
	}

	sl.initializeFromString(dirty);

	/* the attr isn't dirty, so that means it is already clean */
	if (sl.contains(attr) == FALSE)
	{
		return; 
	}

	/* there is only one thing in the list and I should clear it */
	if (sl.contains(attr) == TRUE && sl.number() == 1)
	{
		ad->Delete(ATTR_DIRTY_ATTR_LIST);
		return;
	}
	
	sl.remove(attr);

	/* convert the StringList back into a form the attribute can be set with */
	tmp = sl.print_to_string();

	/* length of the list, plus the attribute, plus the assignment, plus 2
		for the double quotes, plus 1 for nul */
	tmp2 = (char*)calloc(strlen(tmp) + strlen(ATTR_DIRTY_ATTR_LIST) 
						+ strlen(assign) + 2 + 1, 1);
	if (tmp2 == NULL)
	{
		EXCEPT("Out of memory in SetAttrClean()");
	}

	strcpy(tmp2, ATTR_DIRTY_ATTR_LIST);
	strcat(tmp2, assign);
	strcat(tmp2, "\"");
	strcat(tmp2, tmp);
	strcat(tmp2, "\"");

	ad->Delete(ATTR_DIRTY_ATTR_LIST);
	ad->Insert(tmp2);

	free(tmp);
	free(tmp2);
}

/* if the chosen attribute is dirty, return true */
bool IsAttrDirty(ClassAd *ad, char *attr)
{
	char dirty[DIRTY_ATTR_SIZE];
	StringList sl;
	
	/* no dirty list means this is automatically clean */
	if (!ad->LookupString(ATTR_DIRTY_ATTR_LIST, dirty))
	{
		return false;
	}

	sl.initializeFromString(dirty);

	/* the attr isn't dirty, so that means it is already clean */
	if (sl.contains(attr) == TRUE)
	{
		return true;
	}

	return false;
}

/* returns true if there are any dirty attributes at all */
bool AnyAttrDirty(ClassAd *ad)
{
	char dirty[DIRTY_ATTR_SIZE];
	
	/* no dirty list means this is automatically clean */
	if (ad->LookupString(ATTR_DIRTY_ATTR_LIST, dirty))
	{
		return true;
	}

	return false;
}

/* dprintf out with the mode in question the dirty attr list of the classad
	if it has one */
void EmitDirtyAttrList(int mode, ClassAd *ad)
{
	StringList sl;
	char dirty[DIRTY_ATTR_SIZE];

	if (AnyAttrDirty(ad) == true)
	{
		ad->LookupString(ATTR_DIRTY_ATTR_LIST, dirty);

		dprintf(mode, "%s = %s\n", ATTR_DIRTY_ATTR_LIST, dirty);
	}
	else
	{
		dprintf(mode, "%s = UNDEFINED\n", ATTR_DIRTY_ATTR_LIST);
		return;
	}
}




