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

#ifndef COMPAT_CLASSAD_UTIL_H
#define COMPAT_CLASSAD_UTIL_H

#include "compat_classad.h"

int Parse(const char*str, MyString &name, classad::ExprTree*& tree, int*pos = NULL);

int ParseClassAdRvalExpr(const char*s, classad::ExprTree*&tree, int*pos = NULL);

const char * ExprTreeToString( const classad::ExprTree *expr, std::string & buffer );
const char * ExprTreeToString( const classad::ExprTree *expr );
const char * ClassAdValueToString ( const classad::Value & value, std::string & buffer );
const char * ClassAdValueToString ( const classad::Value & value );

bool ExprTreeIsLiteral(classad::ExprTree * expr, classad::Value & value);
bool ExprTreeIsLiteralNumber(classad::ExprTree * expr, long long & ival);
bool ExprTreeIsLiteralNumber(classad::ExprTree * expr, double & rval);
bool ExprTreeIsLiteralString(classad::ExprTree * expr, std::string & sval);
bool ExprTreeIsLiteralBool(classad::ExprTree * expr, bool & bval);
classad::ExprTree * SkipExprEnvelope(classad::ExprTree * tree);
classad::ExprTree * SkipExprParens(classad::ExprTree * tree);
// create an op node, using copies of the input expr trees. this function will not copy envelope nodes (it skips over them)
// and it is clever enough to insert parentheses around the input expressions when needed to insure that the expression
// will unparse correctly.
classad::ExprTree * JoinExprTreeCopiesWithOp(classad::Operation::OpKind, classad::ExprTree * exp1, classad::ExprTree * exp2);

bool EvalBool(compat_classad::ClassAd *ad, const char *constraint);

bool EvalBool(compat_classad::ClassAd *ad, classad::ExprTree *tree);

bool ClassAdsAreSame( compat_classad::ClassAd *ad1, compat_classad::ClassAd * ad2, StringList * ignored_attrs=NULL, bool verbose=false );

int EvalExprTree( classad::ExprTree *expr, compat_classad::ClassAd *source,
				  compat_classad::ClassAd *target, classad::Value &result );

bool IsAMatch( compat_classad::ClassAd *ad1, compat_classad::ClassAd *ad2 );

bool IsAHalfMatch( compat_classad::ClassAd *my, compat_classad::ClassAd *target );

void AttrList_setPublishServerTime( bool publish );

void AddClassAdXMLFileHeader(std::string &buffer);
void AddClassAdXMLFileFooter(std::string &buffer);

#endif
