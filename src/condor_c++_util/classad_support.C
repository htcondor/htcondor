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
#include "condor_common.h"
#include "condor_classad.h"
#include "string_list.h"
#include "classad_support.h"

char ATTR_DIRTY_ATTR_LIST [] = "DirtyAttrList";

static char *assign = " = ";


/* if the attr isn't already in the dirty list, place it in there */
void SetAttrDirty(ClassAd *ad, char *attr)
{
	//char dirty[DIRTY_ATTR_SIZE];
	StringList sl;
	char *tmp, *tmp2;
    std::string s;
    Value v;

	if (!ad->EvaluateAttrString(ATTR_DIRTY_ATTR_LIST, s))
	{
		/* doesn't exist, so I'll add it */
		sl.initializeFromString(attr);
	}
	else
	{
		/* it does exist, so see if it is in the list and add it if not */
		sl.initializeFromString(s.data());
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

	//strcpy(tmp2, ATTR_DIRTY_ATTR_LIST);
	//strcat(tmp2, assign);
	strcat(tmp2, "\"");
	strcat(tmp2, tmp);
	strcat(tmp2, "\"");

    v.SetStringValue(tmp2);
	ad->Delete(ATTR_DIRTY_ATTR_LIST);
	ad->Insert( std::string(ATTR_DIRTY_ATTR_LIST), Literal::MakeLiteral(v));

	free(tmp);
	free(tmp2);
}

/* if the attribute exists in the classad as dirty, then remove it from the
	dirty list. */
void SetAttrClean(ClassAd *ad, char *attr)
{
	//char dirty[DIRTY_ATTR_SIZE];
	StringList sl;
	char *tmp, *tmp2;
    std::string s;
    Value v;

	/* no dirty list means this is automatically clean */
	if ( !ad->EvaluateAttrString(ATTR_DIRTY_ATTR_LIST, s))
	{
		return;
	}

	sl.initializeFromString(s.data());

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

	//strcpy(tmp2, ATTR_DIRTY_ATTR_LIST);
	//strcat(tmp2, assign);
	strcat(tmp2, "\"");
	strcat(tmp2, tmp);
	strcat(tmp2, "\"");

    v.Clear();
    v.SetStringValue(tmp2);
	ad->Delete(ATTR_DIRTY_ATTR_LIST);
	ad->Insert( std::string(ATTR_DIRTY_ATTR_LIST), Literal::MakeLiteral(v));

	free(tmp);
	free(tmp2);
}

/* if the chosen attribute is dirty, return true */
bool IsAttrDirty(ClassAd *ad, char *attr)
{
	//char dirty[DIRTY_ATTR_SIZE];
	StringList sl;
    std::string v;

	/* no dirty list means this is automatically clean */
	if (!ad->EvaluateAttrString(ATTR_DIRTY_ATTR_LIST, v))
	{
		return false;
	}

	sl.initializeFromString(v.data());

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
	//char dirty[DIRTY_ATTR_SIZE];
	
	/* no dirty list means this is automatically clean */
	if (ad->Lookup(ATTR_DIRTY_ATTR_LIST))
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
	//char dirty[DIRTY_ATTR_SIZE];
    std::string v;

	if (AnyAttrDirty(ad) == true)
	{
		if (ad->EvaluateAttrString(ATTR_DIRTY_ATTR_LIST, v)) {
            dprintf(mode, "%s = %s\n", ATTR_DIRTY_ATTR_LIST, v.data());
        }
	}
	else
	{
		dprintf(mode, "%s = UNDEFINED\n", ATTR_DIRTY_ATTR_LIST);
		return;
	}
}




