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
#include "compat_classad_list.h"
#include "condor_xml_classads.h"

#include <vector>
#include <algorithm>

namespace compat_classad {

static unsigned int ptr_hash_fn(ClassAd * const &index)
{
	intptr_t i = (intptr_t)index;
	return (unsigned int)( i ^ (i>>32) );
}

ClassAdList::ClassAdList():
	htable(ptr_hash_fn)
{
}

ClassAdList::~ClassAdList()
{
	ClassAd *ad=NULL;
	bool junk;

	htable.startIterations();
	while( htable.iterate(ad,junk) ) {
		delete ad;
		ad = NULL;
	}
}

ClassAd* ClassAdList::Next()
{
	ClassAd *ad=NULL;
	bool junk;

	if( htable.iterate(ad,junk) ) {
		return ad;
	}
	return NULL;
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
	if( htable.remove(cad)==0 ) {
		return TRUE;
	}
	return FALSE;
}

void ClassAdList::Insert(ClassAd* cad)
{
	htable.insert(cad,true);
}

void ClassAdList::Open()
{
	htable.startIterations();
}

void ClassAdList::Close()
{
}

int ClassAdList::Length()
{
	return htable.getNumElements();
}

void ClassAdList::Sort(SortFunctionType smallerThan, void* userInfo)
{
	ClassAdComparator isSmallerThan(userInfo, smallerThan);

		// copy from htable into a vector; sort vector; copy back

	std::vector<ClassAd *> vect;
	ClassAd *ad=NULL;
	bool junk;

	htable.startIterations();
	while( htable.iterate(ad,junk) ) {
		vect.push_back(ad);
	}

	std::sort(vect.begin(), vect.end(), isSmallerThan);

	htable.clear();
	std::vector<ClassAd *>::iterator it;
	for(it = vect.begin();
		it != vect.end();
		it++)
	{
		htable.insert(*it,true);
	}
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
