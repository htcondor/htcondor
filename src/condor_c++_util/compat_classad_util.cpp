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

#include "compat_classad_util.h"


/* TODO This function needs to be tested.
 */
int Parse(const char*str, MyString &name, classad::ExprTree*& tree, int*pos)
{
	classad::ClassAdParser parser;
	classad::ClassAd *newAd;

		// We don't support the pos argument at the moment.
	if ( pos ) {
		*pos = 0;
	}

		// String escaping is different between new and old ClassAds.
		// We need to convert the escaping from old to new style before
		// handing the expression to the new ClassAds parser.
	std::string newAdStr = "[";
	for ( int i = 0; str[i] != '\0'; i++ ) {
		if ( str[i] == '\\' && 
			 ( str[i + 1] != '"' ||
			   str[i + 1] == '"' &&
			   ( str[i + 2] == '\0' || str[i + 2] == '\n' ||
				 str[i + 2] == '\r') ) ) {
			newAdStr.append( 1, '\\' );
		}
		newAdStr.append( 1, str[i] );
	}
	newAdStr += "]";
	newAd = parser.ParseClassAd( newAdStr );
	if ( newAd == NULL ) {
		tree = NULL;
		return 1;
	}
	if ( newAd->size() != 1 ) {
		delete newAd;
		tree = NULL;
		return 1;
	}
	
	classad::ClassAd::iterator itr = newAd->begin();
	name = itr->first.c_str();
	tree = itr->second->Copy();
	delete newAd;
	return 0;
}

/* TODO This function needs to be tested.
 */
int ParseClassAdRvalExpr(const char*s, classad::ExprTree*&tree, int*pos)
{
	classad::ClassAdParser parser;
	std::string str = s;
	if ( parser.ParseExpression( str, tree, true ) ) {
		return 0;
	} else {
		tree = NULL;
		if ( pos ) {
			*pos = 0;
		}
		return 1;
	}
}

/* TODO This function needs to be tested.
 */
const char *ExprTreeToString( classad::ExprTree *expr )
{
	static std::string buffer;
	classad::ClassAdUnParser unparser;

	unparser.SetOldClassAd( true );
	unparser.Unparse( buffer, expr );

	return buffer.c_str();
}

/* TODO This function needs to be written.
 */
bool EvalBool(CompatClassAd *ad, const char *constraint)
{
	return false;
}

/* TODO This function needs to be written.
 */
bool EvalBool(CompatClassAd *ad, classad::ExprTree *tree)
{
	return false;
}

/* TODO This function needs to be written.
 */
bool ClassAdsAreSame( CompatClassAd *ad1, CompatClassAd * ad2, StringList *ignored_attrs, bool verbose )
{
	return false;
}
