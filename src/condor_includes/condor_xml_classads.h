/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef __CONDOR_XML_CLASSADS_H
#define __CONDOR_XML_CLASSADS_H

#include "condor_classad.h"

enum TagName
{
	tag_ClassAd,
	tag_Attribute,
	tag_Number,
	tag_String,
	tag_Bool,
	tag_Undefined,
	tag_Error,
	tag_Time, // invalid for old ClassAds, but we should recognize it.
	tag_List, // invalid for old ClassAds, but we should recognize it.
	tag_Expr,
	NUMBER_OF_TAG_NAME_ENUMS
};

class ClassAdXMLParser
{
 public:
	ClassAdXMLParser();
	~ClassAdXMLParser();

	ClassAd *ParseClassAd(const char *buffer);
	ClassAd *ParseClassAds(const char *buffer, int *place=0);
};

class ClassAdXMLUnparser
{
 public:
	ClassAdXMLUnparser();
	~ClassAdXMLUnparser();

	bool GetUseCompactNames(void);
	void SetUseCompactNames(bool use_compact_names);

	bool GetUseCompactSpacing(void);
	void SetUseCompactSpacing(bool use_compact_spacing);

	void AddXMLFileHeader(MyString &buffer);
	void AddXMLFileFooter(MyString &buffer);

	void Unparse(ClassAd *classad, MyString &buffer);

 private:
	void add_tag(MyString &buffer, TagName which_tag, bool start_tag);
	void add_attribute_start_tag(MyString &buffer, const char *name);
	void add_bool_start_tag(MyString &buffer, BooleanBase *bool_expr);
	void add_empty_tag(MyString &buffer, TagName which_tag);
	void fix_characters(char *source, MyString &dest);

 private:
	bool _use_compact_names;
	bool _use_compact_spacing;
};

#endif /* __CONDOR_XML_CLASSADS_H */
