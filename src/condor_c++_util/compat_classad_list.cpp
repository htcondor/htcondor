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

#include "compat_classad_list.h"
#include "condor_xml_classads.h"

namespace compat_classad {

ClassAdList::~ClassAdList()
{
		// This is to avoid the size lookup each time
	int imax = list.size();
	for (int i = 0; i < imax; i++)
	{
		if (list[i]) delete list[i];
			// this transient state in the
			// destructor is the only time
			// NULL is put on the list.
		list[i] = NULL;
	}
}

ClassAd* ClassAdList::Next()
{  // Return this element _and_then_ push pointer
	if (index < 0 || index >= list.size())
	{ // This also handles the empty list case.
		return NULL;
	} else {
		return list[index++]; // Note that it's postfix ++
	}
}

int ClassAdList::Delete(ClassAd* cad)
{
	int ret = Remove( cad );
	if ( ret == TRUE ) {
		delete cad;
	}
	return ret;
}

int ClassAdList::Remove(ClassAd* cad)
{
	int retval = FALSE;
	std::vector<ClassAd*>::iterator it = list.begin();
		// This is to avoid the size lookup each time
	int imax = list.size();
	for (int i = 0; i < imax; i++)
	{
		if (*it == cad)
			// or should I do *(*it) == *cad to
			// do a deep comparison?
		{
			ClassAd* tmp = *it;
			it = list.erase(it);
			retval = TRUE;
				// Now if this element that we deleted occurs
				// at or after our index value, we're fine. But
				// if that's not the case, we need to decrement
				// the index to emulate old ClassAdList behaviour
				// correctly.
			if ( i < index ){ 
				index--;  // This will not cause index to go below zero.
			}
		} else {
			++it;
		}
	}
	return retval;
}

void ClassAdList::Insert(ClassAd* cad)
{
	bool is_in_list = false;
	std::vector<ClassAd*>::iterator it = list.begin();
	int imax = list.size(); // Performance optimization
	for(int i = 0; i < imax; i++)
	{
		if (*it == cad)
			// or should I do *(*it) == *cad to
			// do a deep comaprison?
		{
			is_in_list = true;
			break;
		}
	}
	if (!is_in_list)
	{
		list.push_back(cad);
	}
}

void ClassAdList::Sort(SortFunctionType smallerThan, void* userInfo)
{
	ClassAdComparator isSmallerThan(userInfo, smallerThan);
	std::sort(list.begin(), list.end(), isSmallerThan);
}

void ClassAdList::fPrintAttrListList(FILE* f, bool use_xml, StringList *attr_white_list)
{
	ClassAd            *tmpAttrList;
	ClassAdXMLUnparser  unparser;
	MyString            xml;

	if (use_xml) {
		unparser.SetUseCompactSpacing(false);
		unparser.AddXMLFileHeader(xml);
		printf("%s\n", xml.Value());
		xml = "";
	}

    Open();
    for(tmpAttrList = Next(); tmpAttrList; tmpAttrList = Next()) {
		if (use_xml) {
			unparser.Unparse((ClassAd *) tmpAttrList, xml, attr_white_list);
			printf("%s\n", xml.Value());
			xml = "";
		} else {
			tmpAttrList->fPrint(f,attr_white_list);
		}
        fprintf(f, "\n");
    }
	if (use_xml) {
		unparser.AddXMLFileFooter(xml);
		printf("%s\n", xml.Value());
		xml = "";
	}
    Close();
}

int ClassAdList::Count( classad::ExprTree *constraint )
{
	ClassAd *ad = NULL;
	int matchCount  = 0;

	// Check for null constraint.
	if ( constraint == NULL ) {
		return 0;
	}

	Rewind();
	while( (ad = Next()) ) {
		if ( EvalBool(ad, constraint) ) {
			matchCount++;
		}
	}
	return matchCount;
}

} // namespace compat_classad
