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


#ifndef __CONDOR_XML_CLASSADS_H
#define __CONDOR_XML_CLASSADS_H

#include "condor_classad.h"

enum TagName
{
	tag_ClassAds,
	tag_ClassAd,
	tag_Attribute,
	tag_Number, // no longer used, just for backwards compatibility
	tag_Integer,
	tag_Real,
	tag_String,
	tag_Bool,
	tag_Undefined,
	tag_Error,
	tag_Time,
	tag_List, // invalid for old ClassAds, but we should recognize it.
	tag_Expr,
	tag_NoTag,
	NUMBER_OF_TAG_NAME_ENUMS
};

class XMLSource;
class XMLToken;

class ClassAdXMLParser
{
 public:
    /** Constructor. Creates an object that can be used to parse ClassAds */
	ClassAdXMLParser();

    /** Destructor. */
	~ClassAdXMLParser();

	/** Parses an XML ClassAd. Only the first ClassAd in the buffer is 
	 *  parsed.
	 * @param buffer The string containing the XML ClassAd.
	 * @return A pointer to a new ClassAd, which must be deleted by the caller.
	 */
	ClassAd *ParseClassAd(const char *buffer);

	/** Parses an XML ClassAd from a string. Parsing begins at place-th
	 * character of the string, and after a single ClassAd is parsed, place
	 * is updated to indicate where parsing should resume for the next ClassAd.
	 * @param buffer The string containing the XML ClassAd.
	 * @param place  A pointer to an integer indicating where to start. This is
	 *        updated upon return. 
	 * @return A pointer to a new ClassAd, which must be deleted by the caller
	 */
	ClassAd *ParseClassAds(const char *buffer, int *place);

	/** Parses an XML ClassAd from a file. Only the first ClassAd in the 
	 * buffer is parsed.
	 * @param buffer The string containing the XML ClassAd.
	 * @return A pointer to a new ClassAd, which must be deleted by the caller.
	 */
	ClassAd *ParseClassAd(FILE *file);

 private:
	ClassAd *_ParseClassAd(XMLSource &source);
};

class ClassAdXMLUnparser
{
 public:
    /** Constructor. Creates an object that can be used to create XML
	 * representations of ClassAds */
	ClassAdXMLUnparser();

    /** Destructor.  */
	~ClassAdXMLUnparser();

	/** Query to find if the XML uses compact spacing. Compact spacing 
	 * places an entire ClassAd on one line, while non-compact places
	 * each attribute on a separate line. 
	 * @return true if using compact spacing. */
	bool GetUseCompactSpacing(void);

	/** Set compact spacing. Compact spacing 
	 * places an entire ClassAd on one line, while non-compact places
	 * each attribute on a separate line. 
	 * @param use_compact_spacing A boolean indicating if compact spacing is desired. */
	void SetUseCompactSpacing(bool use_compact_spacing);

	/** Query to find out if we will output the Type attribute.
	 *  @return true if we do, false if not. */
	bool GetOutputType(void);

	/** Set whether or not we output the Type attribute */
	void SetOutputType(bool output_type);

	/** Query to find out if we will output the TargetType attribute.
	 *  @return true if we do, false if not. */
	bool GetOuputTargetType(void);

	/** Set whether or not we output the Target attribute */
	void SetOutputTargetType(bool output_target_type);

	/** Adds XML header to buffer. This is the information that should appear 
	 * at the beginning of all ClassAd XML files. 
	 * @param buffer The string to append the header to. */
	void AddXMLFileHeader(MyString &buffer);

	/** Adds XML footer to buffer. This is the information that should appear 
	 * at the end of all ClassAd XML files. 
	 * @param buffer The string to append the footer to. */
	void AddXMLFileFooter(MyString &buffer);

	/** Format a ClassAd in XML. Note that this obeys the previously set
	 * parameters for compact spacing and compact names. 
	 * @param classsad The ClassAd to represent in XML.
	 * @param buffer The string to append the ClassAd to.
	 * @param attr_white_list List of attributes to show (all if NULL)
	 * @see SetUseCompactNames().
	 * @see SetUseCompactSpacing(). */
	void Unparse(ClassAd *classad, MyString &buffer, StringList *attr_white_list=NULL);

 private:
	void Unparse(const char *name, ExprTree *expression, MyString &buffer);
	void add_tag(MyString &buffer, TagName which_tag, bool start_tag);
	void add_attribute_start_tag(MyString &buffer, const char *name);
	void add_bool_start_tag(MyString &buffer, bool value);
	void add_empty_tag(MyString &buffer, TagName which_tag);
	void fix_characters(const char *source, MyString &dest);

 private:
	bool _use_compact_spacing;
	bool _output_type;
	bool _output_target_type;
};

#endif /* __CONDOR_XML_CLASSADS_H */
