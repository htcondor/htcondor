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


#ifndef __CLASSAD_XMLLEXER_H__
#define __CLASSAD_XMLLEXER_H__

#include "classad/common.h"
#include "classad/lexerSource.h"
#include <map>

namespace classad {

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
    XMLLexer(const XMLLexer &)            { return;       }
    XMLLexer &operator=(const XMLLexer &) { return *this; }
};

struct xml_tag_mapping
{
	const char		*tag_name;
	XMLLexer::TagID  id;
};

#define NUMBER_OF_TAG_MAPPINGS (sizeof(tag_mappings)/sizeof(struct xml_tag_mapping))
extern struct xml_tag_mapping tag_mappings[];


} // classad

#endif //__CLASSAD_XMLLEXER_H__
