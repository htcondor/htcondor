/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __XMLLEXER_H__
#define __XMLLEXER_H__

#include "common.h"
#include <map>

BEGIN_NAMESPACE( classad );

typedef map<string, string> XMLAttributes;
typedef map<string, string>::iterator XMLAttributesIterator;

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
		tagID_Number,
		tagID_String,
		tagID_Bool,
		tagID_Undefined,
		tagID_Error,
		tagID_Time,
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
		string         text;
		XMLAttributes  attributes;
	};

	XMLLexer ();
	~XMLLexer ();

	void SetLexText( const string &new_lex_buffer);

	// the 'extract token' functions
	bool PeekToken(Token* token);
	bool ConsumeToken(Token *token);
	
 private:
	Token       current_token;
	bool        token_is_valid;
	const char  *buffer;
	int         buffer_length;
	int         buffer_offset;

	void Initialize(void);
	bool GrabToken(void);
	bool GrabTag(void);
	void BreakdownTag(const char *complete_tag);
	bool GrabText(void);
};

END_NAMESPACE // classad

#endif //__LEXER_H__
