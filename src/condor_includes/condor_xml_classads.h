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
	tag_ClassAds,
	tag_ClassAd,
	tag_Attribute,
	tag_Number,
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
	 * @see SetUseCompactNames().
	 * @see SetUseCompactSpacing(). */
	void Unparse(ClassAd *classad, MyString &buffer);

 private:
	void Unparse(ExprTree *expression, MyString &buffer);
	void add_tag(MyString &buffer, TagName which_tag, bool start_tag);
	void add_attribute_start_tag(MyString &buffer, const char *name);
	void add_bool_start_tag(MyString &buffer, BooleanBase *bool_expr);
	void add_empty_tag(MyString &buffer, TagName which_tag);
	void fix_characters(const char *source, MyString &dest);

 private:
	bool _use_compact_spacing;
	bool _output_type;
	bool _output_target_type;
};

#endif /* __CONDOR_XML_CLASSADS_H */
