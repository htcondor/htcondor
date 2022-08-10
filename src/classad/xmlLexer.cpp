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


#include "classad/common.h"
#include "classad/xmlLexer.h"
#include "classad/lexer.h"
#include "classad/util.h"

using namespace std;

namespace classad {

// Note that these must be in the same order as the enum TagID
struct xml_tag_mapping tag_mappings[] = 
{
	{"classads",        XMLLexer::tagID_ClassAds},
	{"c",               XMLLexer::tagID_ClassAd},
	{"a",               XMLLexer::tagID_Attribute},
	{"i",               XMLLexer::tagID_Integer},
	{"r",              XMLLexer::tagID_Real},
	{"s",               XMLLexer::tagID_String},
	{"b",               XMLLexer::tagID_Bool},
	{"un",              XMLLexer::tagID_Undefined},
	{"er",              XMLLexer::tagID_Error},
	{"at",               XMLLexer::tagID_AbsoluteTime},
	{"rt",               XMLLexer::tagID_RelativeTime},
	{"l",               XMLLexer::tagID_List},
	{"e",               XMLLexer::tagID_Expr},
	{"?xml",            XMLLexer::tagID_XML},
	{"?xml-stylesheet", XMLLexer::tagID_XMLStylesheet},
	{"DOCTYPE",         XMLLexer::tagID_Doctype}
};

struct entity
{
	const char *name;
	const char *replacement_text;
	int length;
};

#define NUMBER_OF_ENTITIES (sizeof(entities) / sizeof(struct entity))
struct entity entities[] = 
{
	{"&amp;",  "&",  5},
	{"&lt;",   "<",  4},
	{"&gt;",   ">",  4}
};

XMLLexer::Token::
Token()
{
	ClearToken();
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
		case tagID_Integer:    printf("integer\n"); break;
		case tagID_Real:    printf("real\n"); break;
		case tagID_String:    printf("string\n"); break;
		case tagID_Bool:      printf("bool\n"); break;
		case tagID_Undefined: printf("undefined\n"); break;
		case tagID_Error:     printf("error\n"); break;
		case tagID_AbsoluteTime:      printf("absolutetime\n"); break;
		case tagID_RelativeTime:      printf("relativetime\n"); break;
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
	lexer_source = NULL;
    token_is_valid = false;
	return;
}


XMLLexer::
~XMLLexer ()
{
	return;
}


void XMLLexer::
SetLexerSource(LexerSource *source)
{
	lexer_source = source;

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

    /*
    if (have_token) {
        printf("Consuming token: ");
        current_token.DumpToken();
    }
    */

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
        /*
        printf("Peeking token: ");
        current_token.DumpToken();
        */
	}
	return have_token;
}

bool XMLLexer::
GrabToken(void)
{
	bool have_token;
	bool have_character;
	int  character;
	bool in_string_tag = false;

	// If the previous token was a starting string tag, then we don't
	// want to skip whitespace.
	if ( current_token.token_type == tokenType_Tag &&
		 current_token.tag_type == tagType_Start &&
		 current_token.tag_id == tagID_String ) {
		in_string_tag = true;
	}

	current_token.ClearToken();
	have_character = false;
    have_token     = false;
    character      = -1;
	// Find first non-whitespace character, to decide if we 
	// are getting a tag or some text. If we get text, we preserve
	// whitespace, because we keep whitespace in strings.
	while (!lexer_source->AtEnd()) {
		character = lexer_source->ReadCharacter();
		if (in_string_tag || !isspace(character)) {
			have_character = true;
			break;
		}
	}
	
	if (have_character) {
		if (character == '<') {
			have_token = GrabTag();
		} else {
			lexer_source->UnreadCharacter();
			have_token = GrabText();
		}
	} else {
		have_token = false;
	}

	//current_token.DumpToken();

	return have_token;
}

bool XMLLexer::
GrabTag(void)
{
	bool    have_token;
	int     character;
	string  complete_tag; // the tag and its attributes

	current_token.token_type = tokenType_Tag;
	complete_tag = "";
	character = -1;

	// Skip whitespace
	while (!lexer_source->AtEnd()) {
		character = lexer_source->ReadCharacter();
		if (!isspace(character)) {
			complete_tag += character;
			break;
		}
	}

	// Read the rest of the tag, a character at a time
	while (!lexer_source->AtEnd()) {
		character = lexer_source->ReadCharacter();
		if (character != '>') {
			complete_tag += character;
		} else {
			break;
		}
	}

	if (character != '>') { 
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
	int start, count;

	length = (int)strlen(complete_tag);
	
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
	start = i;
	count = 0;
	while (i < length && i != '>' && !isspace(complete_tag[i])) {
		//current_token.text += complete_tag[i];
		i++;
		count++;
	}
	// With gcc-2.x's STL, this is faster than using a bunch of += statements.
	current_token.text.assign(complete_tag+start, count);

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
		start = i;
		count = 0;
		while (i < length 
			   && !isspace(complete_tag[i]) 
			   && complete_tag[i] != '=') {
			//name += complete_tag[i];
			i++;
			count++;
		}
		// With gcc-2.x's STL, this is faster than using a bunch of += statements.
		name.assign(complete_tag+start, count);

		// Now skip whitespace and equal signs
		// Note that this allows some technically illegal things
		// like " == = = =", but who really cares?
		while (i < length
			   && (isspace(complete_tag[i]) || complete_tag[i] == '=')) {
			i++;
		}

		i++; // go past 1st \"

		// Now pick out the value
		char oldCh = 0;
		// consume the string literal; read upto " ignoring \"
		while (    (i<length)  
				&& (    complete_tag[i] != '\"' 
					 || ( complete_tag[i] == '\"' && oldCh == '\\' ) ) ) {
			oldCh = complete_tag[i];
			value += complete_tag[i];
			i++;
		}
		// scan string for &...; & replace them with their corresponding entities
		for (unsigned int k=0; k< value.length(); k++) {
			if (value[k] == '&') { // create substring
				int index = k-1;
				string str;
				do {
					index++;
					str += value[index];
				} while(value[index] != ';');
				for (unsigned int j = 0; j < NUMBER_OF_ENTITIES; j++){
					if (!strcmp(str.c_str(), entities[j].name)) {
						value.replace(k, str.length(), entities[j].replacement_text);
					}
				}
			}
		}
		bool validStr = true;
		convert_escapes(value, validStr);
		if(!validStr) {  // contains a \0 escape char
			current_token.tag_type = tagType_Invalid;
		}
		else if (name.size() > 0 && value.size() > 0) {
			current_token.attributes[name] = value;
		}		
	}
	return;
}

bool XMLLexer::
GrabText(void)
{
	bool have_token = false;

	current_token.token_type = tokenType_Text;
	current_token.text = "";

	while (!lexer_source->AtEnd()) {
		int character;

		character = lexer_source->ReadCharacter();
		if (character == '<') {
			lexer_source->UnreadCharacter();
			break;
		} else {
			have_token = true;

			if (character == '&') {

				// Figure out if this is an entity. If it is, figure
				// out what character should actually be put into the text;
				// First, find the text of the entity: it's text up and including
				// a semicolon. But finding a space or another & will stop us.

				string entity_text;
				entity_text = character;
				while (!lexer_source->AtEnd()) {
					int ch = lexer_source->ReadCharacter();
					if (ch == ' ') {
						// drop the entire text, including the space into the token
						entity_text += ch;
						current_token.text += entity_text;
						break;
					} else if (ch == '&') {
						// drop the text before the & into the token. 
						// put back the & to be found the next time around, so
						// we can look for another entity
						lexer_source->UnreadCharacter();
						current_token.text += entity_text;
						break;
					} else if (ch == ';') {
						bool replaced_entity = false;
						// Non-numeric entity, do a comparison
						entity_text += ch;
						for (unsigned int i = 0; i < NUMBER_OF_ENTITIES; i++){
						  if (!strcmp(entity_text.c_str(), entities[i].name)) {
						    current_token.text += entities[i].replacement_text;
						    replaced_entity = true;
						    break;
						  }
						}
					       
						if (!replaced_entity) {
							current_token.text += entity_text;
						}
						break;
					} else {
						entity_text += ch;
					}
				}
			} else {
				current_token.text += character;
			}
		}
	}
	
	return have_token;
}

} // classad
