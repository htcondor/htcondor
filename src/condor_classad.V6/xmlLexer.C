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

#include "common.h"
#include "xmlLexer.h"

BEGIN_NAMESPACE( classad )

// Note that these must be in the same order as the enum TagID
struct xml_tag_mapping tag_mappings[] = 
{
	{"classads",        XMLLexer::tagID_ClassAds},
	{"c",               XMLLexer::tagID_ClassAd},
	{"a",               XMLLexer::tagID_Attribute},
	{"n",               XMLLexer::tagID_Number},
	{"s",               XMLLexer::tagID_String},
	{"b",               XMLLexer::tagID_Bool},
	{"un",              XMLLexer::tagID_Undefined},
	{"er",              XMLLexer::tagID_Error},
	{"t",               XMLLexer::tagID_Time},
	{"l",               XMLLexer::tagID_List},
	{"e",               XMLLexer::tagID_Expr},
	{"?xml",            XMLLexer::tagID_XML},
	{"?xml-stylesheet", XMLLexer::tagID_XMLStylesheet},
	{"DOCTYPE",         XMLLexer::tagID_Doctype}
};

struct entity
{
	char *entity;
	char *replacement_text;
	int length;
};

#define NUMBER_OF_ENTITIES (sizeof(entities) / sizeof(struct entity))
struct entity entities[] = 
{
	{"&amp;",  "&",  5},
	{"&lt;",   "<",  4},
	{"&gt;",   ">",  4},
	{"&quot;", "\"", 6},
	{"&apos;", "'",  6}
};

XMLLexer::Token::
Token()
{
	return;
}

XMLLexer::Token::
~Token()
{
	return;
}

void XMLLexer::Token::
ClearToken(void)
{
	token_type = tokenType_Invalid;
	tag_type   = tagType_Invalid;
	tag_id     = tagID_NoTag;
	text       = "";
	attributes.clear();
	return;
}

void XMLLexer::Token::
DumpToken(void) 
{
	if (token_type == tokenType_Tag) {
		printf("TAG: \"%s\", ", text.c_str());
		printf("  Type: ");
		switch (tag_type) {
		case tagType_Start:   printf("start, ");   break;
		case tagType_End:     printf("end, ");     break;
		case tagType_Empty:   printf("empty, ");   break;
		case tagType_Invalid: printf("invalid, "); break;
		default: break;
		}
		printf("  ID: ");
		switch (tag_id) {
		case tagID_ClassAds:  printf("classads\n"); break;
		case tagID_ClassAd:   printf("classad\n"); break;
		case tagID_Attribute: printf("attribute\n"); break;
		case tagID_Number:    printf("number\n"); break;
		case tagID_String:    printf("string\n"); break;
		case tagID_Bool:      printf("bool\n"); break;
		case tagID_Undefined: printf("undefined\n"); break;
		case tagID_Error:     printf("error\n"); break;
		case tagID_Time:      printf("time\n"); break;
		case tagID_List:      printf("list\n"); break;
		case tagID_Expr:      printf("expr\n"); break;
		case tagID_XML:       printf("?xml\n"); break;
		case tagID_XMLStylesheet: printf("?xmlStyleSheet\n"); break;
		case tagID_Doctype:   printf("DOCTYPE\n"); break;
		case tagID_NoTag:     printf("notag\n"); break;
		}
		XMLAttributesIterator  iter;

		for (iter = attributes.begin(); iter != attributes.end(); iter++) {
			printf("  Attribute: %s = \"%s\"\n",
				   iter->first.c_str(), iter->second.c_str());
		}
	} else if (token_type == tokenType_Text) {
		printf("TEXT: \"%s\"\n", text.c_str());
	} else {
		printf("Invalid token.\n");
	}
	return;
}

XMLLexer::
XMLLexer ()
{
	Initialize();
	return;
}


XMLLexer::
~XMLLexer ()
{
	return;
}


void XMLLexer::
SetLexText( const string &new_buffer)
{
	Initialize();
	buffer        = new_buffer.c_str();
	buffer_length = strlen(buffer);
	buffer_offset = -1;

	return;
}

void XMLLexer::
Initialize(void)
{
	current_token.ClearToken();
	token_is_valid = false;
	buffer         = NULL;
	buffer_offset  = -1;
	buffer_length  = -1;

	return;
}

bool XMLLexer::
ConsumeToken(Token *token)
{
	bool have_token;

	if (!token_is_valid) {
		have_token = PeekToken(token);
	} else {
		have_token = true;
		if (token != NULL) {
			*token = current_token;
		}
	}
	token_is_valid = false;

	return have_token;
}

bool XMLLexer::
PeekToken(Token *token)
{
	bool have_token; 

	if (token_is_valid) {
		have_token = true;
	} else {
		have_token = GrabToken();
	}

	if (have_token) {
		if (token != NULL) {
			*token = current_token;
		}
		token_is_valid = true;
	}
	return have_token;
}

bool XMLLexer::
GrabToken(void)
{
	bool have_token;

	current_token.ClearToken();

	// Find first non-whitespace character, to decide if we 
	// are getting a tag or some text. If we get text, we preserve
	// whitespace, because we keep whitespace in strings.
	int i;
	buffer_offset++;
	i = buffer_offset;
	while (i < buffer_length && isspace(buffer[i])) {
		i++;
	}
	
	if (buffer[i] == '<') {
		buffer_offset = i;
		have_token = GrabTag();
	} else {
		have_token = GrabText();
	}

	return have_token;
}

bool XMLLexer::
GrabTag(void)
{
	bool    have_token;
	string  complete_tag; // the tag and it's attributes

	current_token.token_type = tokenType_Tag;
	buffer_offset++; // skip the '<'
	complete_tag = "";

	// Skip whitespace
	while (buffer_offset < buffer_length && isspace(buffer[buffer_offset])) {
		buffer_offset++;
	}

	while (buffer_offset < buffer_length && buffer[buffer_offset] != '>') {
		complete_tag += buffer[buffer_offset];
		buffer_offset++;
	}

	if (buffer[buffer_offset] != '>') { 
		// We have an unclosed tag. This will not do. 
		have_token = false;
	} else {
		BreakdownTag(complete_tag.c_str());
		have_token = true;
	}
	return true;
}

void XMLLexer::
BreakdownTag(const char *complete_tag)
{
	int length, i;
	
	length = strlen(complete_tag);
	
	// Skip whitespace
	for (i = 0; i < length && isspace(complete_tag[i]); i++) {
		;
	}
	
	// Is it a begin or end tag?
	if (complete_tag[i] == '/') {
		current_token.tag_type = tagType_End;
		i++;
	} else if (complete_tag[length-1] == '/') {
		current_token.tag_type = tagType_Empty;
		length--; // skip the / in the processing that follows.
	} else {
		current_token.tag_type = tagType_Start;
	}
	
	// Now pull out the tag name
	current_token.text = "";
	while (i < length && i != '>' && !isspace(complete_tag[i])) {
		current_token.text += complete_tag[i];
		i++;
	}

	// Figure out which tag it is
	current_token.tag_id = tagID_NoTag;
	for (unsigned int x = 0; x < NUMBER_OF_TAG_MAPPINGS; x++) {
		if (!strcmp(current_token.text.c_str(), tag_mappings[x].tag_name)) {
			current_token.tag_id = tag_mappings[x].id;
			break;
		}
	}
	
	// If we're not at the end, we probably have attributes, so let's
	// pull them out. (We might just have whitespace though.)
	while (i < length) {
		string name, value;

		name  = "";
		value = "";

		// Skip whitespace
		while (i < length && isspace(complete_tag[i])) {
			i++;
		}

		// Now take text up to a whitespace or equal sign. This is the name
		while (i < length 
			   && !isspace(complete_tag[i]) 
			   && complete_tag[i] != '=') {
			name += complete_tag[i];
			i++;
		}

		// Now skip whitespace and equal signs
		// Note that this allows some technically illegal things
		// like " == = = =", but who really cares?
		while (i < length
			   && (isspace(complete_tag[i]) || complete_tag[i] == '=')) {
			i++;
		}

		// Now pick out the value
		while (i < length && !isspace(complete_tag[i])) {
			if (complete_tag[i] != '"') {
				value += complete_tag[i];
			}
			i++;
		}
		if (name.size() > 0 && value.size() > 0) {
			current_token.attributes[name] = value;
		}
	}
	return;
}

bool XMLLexer::
GrabText(void)
{
	bool have_token;
	bool have_nonspace;

	current_token.token_type = tokenType_Text;
	have_nonspace = false;
	current_token.text = "";

	while (buffer_offset < buffer_length && buffer[buffer_offset] != '<') {
		int  inc = 1;

		if (buffer[buffer_offset] == '&') {
			const char *entity = buffer + buffer_offset;
			bool replaced_entity = false;

			for (unsigned int i = 0; i < NUMBER_OF_ENTITIES; i++){
				if (!strncmp(entity, entities[i].entity, entities[i].length)) {
					current_token.text += entities[i].replacement_text;
					inc = entities[i].length;
					replaced_entity = true;
					break;
				}
			}
			if (!replaced_entity) {
				current_token.text += '&';
			}
		} else {
			current_token.text += buffer[buffer_offset];
		}
		if (!isspace(buffer[buffer_offset])) {
			have_nonspace = true;
		}
		buffer_offset += inc;
	}

	if (!have_nonspace) {
		// We know that we're at the end of the buffer, because
		// otherwise GrabTag would have been called.
		have_token = false;
	} else {
		have_token = true;
		if (buffer[buffer_offset] == '<') {
			buffer_offset--;
		}
	}
	return have_token;
}

END_NAMESPACE // classad
