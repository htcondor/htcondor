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
