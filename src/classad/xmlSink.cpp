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
#include "classad/exprTree.h"
#include "classad/xmlSink.h"
#include "classad/sink.h"
#include "classad/xmlLexer.h"
#include "classad/util.h"
#include "classad/classadCache.h"

using std::string;
using std::vector;
using std::pair;


namespace classad {

static void add_tag(
    string &buffer, 
	XMLLexer::TagID  tag_id,
	XMLLexer::TagType tag_type,
	const char *attribute_name = NULL,
	const char *attribute_value = NULL);


ClassAdXMLUnParser::
ClassAdXMLUnParser()
{
	compact_spacing = true;
	return;
}


ClassAdXMLUnParser::
~ClassAdXMLUnParser()
{
	return;
}

void ClassAdXMLUnParser::
SetCompactSpacing(
	bool use_compact_spacing)
{
	compact_spacing = use_compact_spacing;
	return;
}

void ClassAdXMLUnParser::Unparse(
	string    &buffer, 
	const ExprTree  *expr)
{
	Unparse(buffer, expr, 0);
	return;
}

void ClassAdXMLUnParser::
Unparse( std::string &buffer,
	const ClassAd *ad,
	const References &whitelist )
{
	if( !ad ) {
		buffer = "<error:null expr>";
		return;
	}

	vector< pair<string, ExprTree*> > attrs;
	ad->GetComponents(attrs, whitelist);
	UnparseAux(buffer, attrs, 0);
}

void ClassAdXMLUnParser::
Unparse(
	string   &buffer, 
	const ExprTree *tree, 
	int      indent)
{
	if( !tree ) {
		buffer = "<error:null expr>";
		return;
	} else {
		
		switch( tree->GetKind( ) ) {
		case ExprTree::ERROR_LITERAL:
		case ExprTree::UNDEFINED_LITERAL:
		case ExprTree::BOOLEAN_LITERAL:
		case ExprTree::INTEGER_LITERAL:
		case ExprTree::REAL_LITERAL:
		case ExprTree::RELTIME_LITERAL:
		case ExprTree::ABSTIME_LITERAL:
		case ExprTree::STRING_LITERAL: {
			Value				val;
			((Literal*)tree)->GetValue(val);
			Unparse(buffer, val, indent);
			break;
		}
		case ExprTree::ATTRREF_NODE: 
		case ExprTree::OP_NODE: 
		case ExprTree::FN_CALL_NODE: {
			ClassAdUnParser unparser;
			add_tag(buffer, XMLLexer::tagID_Expr, XMLLexer::tagType_Start);
			unparser.setXMLUnparse(true);
			unparser.Unparse(buffer, tree);
			add_tag(buffer, XMLLexer::tagID_Expr, XMLLexer::tagType_End);
			break;
		}
			
		case ExprTree::CLASSAD_NODE: {
			vector< pair<string, ExprTree*> > attrs;
			((ClassAd*)tree)->GetComponents(attrs);
			UnparseAux(buffer, attrs, indent);
			break;
		}
		
		case ExprTree::EXPR_LIST_NODE: {
			vector<ExprTree*> exprs;
			((ExprList*)tree)->GetComponents(exprs);
			UnparseAux(buffer, exprs, indent);
			break;
		}
		
		case ExprTree::EXPR_ENVELOPE: {
			Unparse( buffer, ((CachedExprEnvelope*)tree)->get(),indent );
			break; 
		}
		
		default:
			buffer = "";
			CondorErrno = ERR_BAD_EXPRESSION;
			CondorErrMsg = "unknown expression type";
			break;
		}
	}
	return;
}

void ClassAdXMLUnParser::
Unparse(
	string &buffer, 
	const Value  &val, 
	int    indent)
{
	char tempBuf[512];

	switch( val.GetType( ) ) {
		case Value::NULL_VALUE: 
			// I don't think this ever occurs. Am I correct? 
			//buffer += "<null/>"; 
			break;

		case Value::STRING_VALUE: {
			string	               s;
			add_tag(buffer, XMLLexer::tagID_String, XMLLexer::tagType_Start);
			ClassAdUnParser unp;
			unp.setXMLUnparse(true); 
			unp.setDelimiter(0); // change the default delimiter from \" to no delimiter
			unp.Unparse(s,val);			
			s.erase(0,1);
			s.erase(s.length()-1,1);
			buffer += s;
			add_tag(buffer, XMLLexer::tagID_String, XMLLexer::tagType_End);
			break;
		}
		case Value::INTEGER_VALUE: {
			long long i;
			val.IsIntegerValue(i);
			add_tag(buffer, XMLLexer::tagID_Integer, XMLLexer::tagType_Start);
			buffer += std::to_string(i);
			add_tag(buffer, XMLLexer::tagID_Integer, XMLLexer::tagType_End);
			break;
		}
		case Value::REAL_VALUE: {   
			double real;

			add_tag(buffer, XMLLexer::tagID_Real, XMLLexer::tagType_Start);
			val.IsRealValue(real); 
            if (real == 0.0) {
                buffer += "0.0";
            } else if (real == -0.0) {
                buffer += "-0.0";
            } else if (classad_isnan(real)) {
                buffer += "NaN";
            } else if (classad_isinf(real) == -1){
                buffer += "-INF";
            } else if (classad_isinf(real) == 1) {
                buffer += "INF";
            } else {
                snprintf(tempBuf, sizeof(tempBuf), "%1.15E", real);
                buffer += tempBuf;
            }
			add_tag(buffer, XMLLexer::tagID_Real, XMLLexer::tagType_End);
			break;
		}
		case Value::BOOLEAN_VALUE: {
			bool b = false;
			val.IsBooleanValue( b );
			if (b) {
				add_tag(buffer, XMLLexer::tagID_Bool, 
						XMLLexer::tagType_Empty, "v", "t");
			} else {
				add_tag(buffer, XMLLexer::tagID_Bool, 
						XMLLexer::tagType_Empty, "v", "f");
			}				
			break;;
		}
		case Value::UNDEFINED_VALUE: {
			add_tag(buffer, XMLLexer::tagID_Undefined, 
					XMLLexer::tagType_Empty);
			break;
		}
		case Value::ERROR_VALUE: {
			add_tag(buffer, XMLLexer::tagID_Error,
					XMLLexer::tagType_Empty);
			break;
		}
		case Value::ABSOLUTE_TIME_VALUE: {
       			add_tag(buffer, XMLLexer::tagID_AbsoluteTime, XMLLexer::tagType_Start);
			ClassAdUnParser unp;
			string abstimestr;
			unp.Unparse(abstimestr,val);
			abstimestr.erase(0,9); 
			abstimestr.erase(abstimestr.length()-2,2);
			buffer += abstimestr;
			add_tag(buffer, XMLLexer::tagID_AbsoluteTime, XMLLexer::tagType_End);
			break;
		}
		case Value::RELATIVE_TIME_VALUE: {
			add_tag(buffer, XMLLexer::tagID_RelativeTime, XMLLexer::tagType_Start);
			ClassAdUnParser unp;
			string reltimestr;
			unp.Unparse(reltimestr,val);
			reltimestr.erase(0,9);
			reltimestr.erase(reltimestr.length()-2,2);
			buffer += reltimestr;
			add_tag(buffer, XMLLexer::tagID_RelativeTime, XMLLexer::tagType_End);
			break;
		}
		case Value::SCLASSAD_VALUE:
		case Value::CLASSAD_VALUE: {
			ClassAd *ad = NULL;
			vector< pair<string,ExprTree*> > attrs;
			val.IsClassAdValue(ad);
			ad->GetComponents(attrs);
			UnparseAux(buffer, attrs, indent);
			break;
		}
		case Value::SLIST_VALUE:
		case Value::LIST_VALUE: {
			const ExprList *el = NULL;
			vector<ExprTree*> exprs;
			val.IsListValue(el);
			el->GetComponents(exprs);
			UnparseAux(buffer, exprs, indent);
			break;
		}
		default:
			break;
	}
}

void ClassAdXMLUnParser::
UnparseAux(string                           &buffer, 
		   vector< pair<string,ExprTree*> > &attrs, 
		   int                              indent)
{
	vector< pair<string,ExprTree*> >::const_iterator itr;

	add_tag(buffer, XMLLexer::tagID_ClassAd, XMLLexer::tagType_Start);
	if (!compact_spacing) {
		buffer += '\n';
	}
	for (itr=attrs.begin(); itr!=attrs.end(); itr++) {
		if (!compact_spacing) {
			buffer.append(indent+4, ' ');
		}
		Value val; 
		val.SetStringValue(itr->first);
		string idstr;
		ClassAdUnParser unp;
		unp.setXMLUnparse(true); 
		unp.Unparse(idstr,val); 
		idstr.erase(0,1);
		idstr.erase(idstr.length()-1,1);
		add_tag(buffer, XMLLexer::tagID_Attribute, XMLLexer::tagType_Start,
				"n", idstr.c_str());
		Unparse(buffer, itr->second, indent+4);
		add_tag(buffer, XMLLexer::tagID_Attribute, XMLLexer::tagType_End);
		if (!compact_spacing) {
			buffer += '\n';
		}
	}
	if (!compact_spacing) {
		buffer.append(indent, ' ');
	}
	add_tag(buffer, XMLLexer::tagID_ClassAd, XMLLexer::tagType_End);
	if (!compact_spacing && indent == 0) {
		buffer += '\n';
	}
	return;
}


void ClassAdXMLUnParser::
UnparseAux(
	string             &buffer, 
	vector<ExprTree*>  &exprs, 
	int                indent)
{
	vector<ExprTree*>::const_iterator	itr;

	add_tag(buffer, XMLLexer::tagID_List, XMLLexer::tagType_Start);
	for(itr = exprs.begin(); itr != exprs.end(); itr++) {
		Unparse(buffer, *itr, indent);
	}
	add_tag(buffer, XMLLexer::tagID_List, XMLLexer::tagType_End);
}

static void add_tag(
    string             &buffer, 
	XMLLexer::TagID    tag_id,
	XMLLexer::TagType  tag_type,
	const char         *attribute_name,
	const char         *attribute_value)
{
	buffer += '<';
	if (tag_type == XMLLexer::tagType_End) {
		buffer += '/';
	}
	buffer += tag_mappings[tag_id].tag_name;
	if (attribute_name != NULL && attribute_value != NULL) {
		buffer += ' ';
		buffer += attribute_name;
		buffer += "=\"";
		buffer += attribute_value;
		buffer +=  '\"';
	}
	if (tag_type == XMLLexer::tagType_Empty) {
		buffer += '/';
	}
	buffer += '>';
}

/*
static void 
UnparseRelativeTime(string &buffer, time_t rsecs)
{
	int	  days, hrs, mins, secs;
	char  tempBuf[512];

	if( rsecs < 0 ) {
		buffer += "-";
		rsecs = -rsecs;
	}
	days = rsecs;
	hrs  = days % 86400;
	mins = hrs  % 3600;
	secs = mins % 60;
	days = days / 86400;
	hrs  = hrs  / 3600;
	mins = mins / 60;
	
	if (days) {
		sprintf( tempBuf, "%dd", days );
		buffer += tempBuf;
	}
	
	sprintf(tempBuf, "%02d:%02d", hrs, mins);
	buffer += tempBuf;
	
	if (secs) {
		sprintf(tempBuf, ":%02d", secs);
		buffer += tempBuf;
	}
	return;
}
*/

}
