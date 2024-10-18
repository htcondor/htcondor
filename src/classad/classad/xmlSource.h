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


#ifndef __CLASSAD_XMLSOURCE_H__
#define __CLASSAD_XMLSOURCE_H__

#include "classad/xmlLexer.h"

namespace classad {

class ClassAd;

class ClassAdXMLParser
{
	public:
		ClassAdXMLParser();
		~ClassAdXMLParser();
		ClassAd *ParseClassAd(const std::string &buffer);
		ClassAd *ParseClassAd(const std::string &buffer, int &offset);
		ClassAd *ParseClassAd(FILE *file);
		//ClassAd *ParseClassAd(std::istream& stream);
		bool ParseClassAd(const std::string &buffer, ClassAd &ad, int &offset);
		bool ParseClassAd(const std::string &buffer, ClassAd &ad);
		bool ParseClassAd(FILE *file, ClassAd &ad);
		//bool ParseClassAd(std::istream& stream, ClassAd &ad);
		bool ParseClassAd(LexerSource * lexer_source, ClassAd & ad);
	private:
        // The copy constructor and assignment operator are defined
        // to be private so we don't have to write them, or worry about
        // them being inappropriately used. The day we want them, we can 
        // write them. 
        ClassAdXMLParser(const ClassAdXMLParser &)            { return;       }
        ClassAdXMLParser &operator=(const ClassAdXMLParser &) { return *this; }

		ClassAd  *ParseClassAd(ClassAd *classad_in = NULL);
		ExprTree *ParseAttribute(std::string &attribute_name);
		ExprTree *ParseThing(void);
		ExprTree *ParseList(void);
		ExprTree *ParseNumberOrString(XMLLexer::TagID tag_id);
		ExprTree *ParseBool(void);
		ExprTree *ParseUndefinedOrError(XMLLexer::TagID tag_id);
		ExprTree *ParseAbsTime(void);
		ExprTree *ParseRelTime(void);
		ExprTree *ParseExpr(void);
        void SwallowEndTag(XMLLexer::TagID tag_id);

		XMLLexer lexer;
};

}

#endif//__CLASSAD_XMLSOURCE_H__
