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

#ifndef __XMLSOURCE_H__
#define __XMLSOURCE_H__

#include "xmlLexer.h"

BEGIN_NAMESPACE( classad )

class ClassAd;

class ClassAdXMLParser
{
	public:
		ClassAdXMLParser();
		~ClassAdXMLParser();
		ClassAd *ParseClassAd(const std::string &buffer);
		ClassAd *ParseClassAd(const std::string &buffer, int &offset);
		ClassAd *ParseClassAd(FILE *file);
		ClassAd *ParseClassAd(std::istream& stream);
		bool ParseClassAd(const std::string &buffer, ClassAd &ad, int &offset);
		bool ParseClassAd(const std::string &buffer, ClassAd &ad);
		bool ParseClassAd(FILE *file, ClassAd &ad);
		bool ParseClassAd(std::istream& stream, ClassAd &ad);
	private:
        // The copy constructor and assignment operator are defined
        // to be private so we don't have to write them, or worry about
        // them being inappropriately used. The day we want them, we can 
        // write them. 
        ClassAdXMLParser(const ClassAdXMLParser &parser)            { return;       }
        ClassAdXMLParser &operator=(const ClassAdXMLParser &parser) { return *this; }

		ClassAd *ClassAdXMLParser::ParseClassAd(void);
		ExprTree *ParseAttribute(std::string &attribute_name);
		ExprTree *ParseThing(void);
		ExprTree *ParseList(void);
		ExprTree *ParseNumberOrString(XMLLexer::TagID tag_id);
		ExprTree *ParseBool(void);
		ExprTree *ParseUndefinedOrError(XMLLexer::TagID tag_id);
		ExprTree *ParseAbsTime(void);
		ExprTree *ParseRelTime(void);
		ExprTree *ParseExpr(void);

		XMLLexer lexer;
};

END_NAMESPACE

#endif//__SOURCE_H__
