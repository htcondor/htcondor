/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#ifndef __XMLLEXER_H__
#define __XMLLEXER_H__

#include "common.h"
#include "lexerSource.h"
#include <map>

BEGIN_NAMESPACE( classad )

typedef std::map<std::string, std::string> XMLAttributes;
typedef std::map<std::string, std::string>::iterator XMLAttributesIterator;

// the lexical analyzer class
class XMLLexer
{
 public:
	enum TokenType
	{
		tokenType_Tag,
		tokenType_Text,
		tokenType_Invalid
	};
	enum TagType
	{
		tagType_Start, // This is for tags like <foo>
		tagType_End,   // This is for tags like </foo>
		tagType_Empty, // This is for tags like <foo/>
		tagType_Invalid
	};
	enum TagID
	{
		tagID_ClassAds,
		tagID_ClassAd,
		tagID_Attribute,
		tagID_Integer,
		tagID_Real,
		tagID_String,
		tagID_Bool,
		tagID_Undefined,
		tagID_Error,
		tagID_AbsoluteTime,
		tagID_RelativeTime,
		tagID_List,
		tagID_Expr,
		tagID_XML,
		tagID_XMLStylesheet,
		tagID_Doctype,
		tagID_NoTag
	};
	
	class Token
	{
	public:
		Token();
		~Token();
		void ClearToken(void);
		void DumpToken(void);

		TokenType      token_type;
		TagType        tag_type;
		TagID          tag_id;
		std::string    text;
		XMLAttributes  attributes;
	};

	XMLLexer ();
	~XMLLexer ();

	void SetLexerSource(LexerSource *source);

	// the 'extract token' functions
	bool PeekToken(Token* token);
	bool ConsumeToken(Token *token);

 private:
	Token       current_token;
	bool        token_is_valid;
	LexerSource *lexer_source;

	bool GrabToken(void);
	bool GrabTag(void);
	void BreakdownTag(const char *complete_tag);
	bool GrabText(void);
 private:
    
    // The copy constructor and assignment operator are defined
    // to be private so we don't have to write them, or worry about
    // them being inappropriately used. The day we want them, we can 
    // write them. 
    XMLLexer(const XMLLexer &lexer)            { return;       }
    XMLLexer &operator=(const XMLLexer &lexer) { return *this; }
};

struct xml_tag_mapping
{
	char             *tag_name;
	XMLLexer::TagID  id;
};

#define NUMBER_OF_TAG_MAPPINGS (sizeof(tag_mappings)/sizeof(struct xml_tag_mapping))
extern struct xml_tag_mapping tag_mappings[];


END_NAMESPACE // classad

#endif //__LEXER_H__
