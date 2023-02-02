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
#include "classad/cclassad.h"
#include "classad/classad.h"
#include "classad/source.h"
#include "classad/sink.h"
#include "classad/xmlSink.h"

using std::string;
using std::vector;
using std::pair;


namespace classad {

struct cclassad {
	ClassAd *ad;
};

struct cclassad * cclassad_create( const char *str )
{
	ClassAdParser parser;
	struct cclassad *c;

	c = (struct cclassad *) malloc(sizeof(*c));
	if(!c) return 0;

	if(str) {
		c->ad = parser.ParseClassAd(str);
	} else {
		c->ad = new ClassAd;
	}

	if(!c->ad) {
		free(c);
		return 0;
	}

	return c;
}

void cclassad_delete( struct cclassad *c )
{
	if(c) {
		if(c->ad) delete c->ad;
		free(c);
	}
}

char * cclassad_unparse( struct cclassad *c )
{
	ClassAdUnParser unparser;
	string s;

	unparser.Unparse(s,c->ad);

	return strdup(s.c_str());
}

char * cclassad_unparse_xml( struct cclassad *c )
{
	ClassAdXMLUnParser unparser;
	string s;

	unparser.Unparse(s,c->ad);

	return strdup(s.c_str());
}

int cclassad_match( struct cclassad *a, struct cclassad *b )
{
	char *expr;
	int result;
	
	if(!cclassad_evaluate_to_expr(a,"requirements",&expr)) {
		return 0;
	}

	if(!cclassad_evaluate_to_bool(b,expr,&result)) {
		if(!result) {
			free(expr);
			return 0;
		}
	}
	free(expr);

	if(!cclassad_evaluate_to_expr(b,"requirements",&expr)) {
		return 0;
	}

	if(!cclassad_evaluate_to_bool(a,expr,&result)) {
		free(expr);
		if(!result) return 0;
	}
	free(expr);

	return 1;
}

int cclassad_insert_expr( struct cclassad *c, const char *attr, const char *value )
{
	ClassAdParser parser;
	string strattr(attr);
	string strexpr(value);
	ExprTree *e=0;

	e = parser.ParseExpression(strexpr);
	if(e) {
		if(c->ad->Insert(strattr,e)) {
			return 1;
		} else {
			delete e;
			return 0;
		}
	} else {
		return 0;
	}
}

int cclassad_insert_string( struct cclassad *c, const char *attr, const char *value )
{
	string strattr(attr);
	string strvalue(value);
	return c->ad->InsertAttr(strattr,strvalue);
}

int cclassad_insert_int( struct cclassad *c, const char *attr, int value )
{
	string strattr(attr);
	return c->ad->InsertAttr(strattr,value);
}

int cclassad_insert_long( struct cclassad *c, const char *attr, long value )
{
	string strattr(attr);
	return c->ad->InsertAttr(strattr,value);
}

int cclassad_insert_long_long( struct cclassad *c, const char *attr, long long value )
{
	string strattr(attr);
	return c->ad->InsertAttr(strattr,value);
}

int cclassad_insert_double( struct cclassad *c, const char *attr, double value )
{
	string strattr(attr);
	return c->ad->InsertAttr(strattr,value);
}

int cclassad_insert_bool( struct cclassad *c, const char *attr, int value )
{
	string strattr(attr);
	return c->ad->InsertAttr(strattr,value);
}


int cclassad_remove( struct cclassad *c, const char *attr )
{
	string strattr(attr);
	return c->ad->Delete(strattr);
}

int cclassad_evaluate_to_expr( struct cclassad *c, const char *expr, char **result )
{
	string exprstring(expr);
	Value value;

	if(c->ad->EvaluateExpr(exprstring,value)) {
		ClassAdUnParser unparser;
		string s;
		unparser.Unparse(s,value);
		*result = strdup(s.c_str());
		return *result!=0;
	}

	return 0;
}

int cclassad_evaluate_to_string( struct cclassad *c, const char *expr, char **result )
{
	string exprstring(expr);
	Value value;

	if(c->ad->EvaluateExpr(exprstring,value)) {
		string s;
		if(value.IsStringValue(s)) {
			*result = strdup(s.c_str());
			return *result!=0;
		}
	}
	return 0;
}

int cclassad_evaluate_to_int( struct cclassad *c, const char *expr, int *result )
{
	string exprstring(expr);
	Value value;

	if(c->ad->EvaluateExpr(exprstring,value)) {
		if(value.IsIntegerValue(*result)) {
			return 1;
		}
	}
	return 0;
}

int cclassad_evaluate_to_long( struct cclassad *c, const char *expr, long *result )
{
	string exprstring(expr);
	Value value;

	if(c->ad->EvaluateExpr(exprstring,value)) {
		if(value.IsIntegerValue(*result)) {
			return 1;
		}
	}
	return 0;
}

int cclassad_evaluate_to_long_long( struct cclassad *c, const char *expr, long long *result )
{
	string exprstring(expr);
	Value value;

	if(c->ad->EvaluateExpr(exprstring,value)) {
		if(value.IsIntegerValue(*result)) {
			return 1;
		}
	}
	return 0;
}

int cclassad_evaluate_to_double( struct cclassad *c, const char *expr, double *result )
{
	string exprstring(expr);
	Value value;

	if(c->ad->EvaluateExpr(exprstring,value)) {
		if(value.IsRealValue(*result)) {
			return 1;
		}
	}
	return 0;
}

int cclassad_evaluate_to_bool( struct cclassad *c, const char *expr, int *result )
{
	string exprstring(expr);
	Value value;

	if(c->ad->EvaluateExpr(exprstring,value)) {
		bool b;
		if(value.IsBooleanValue(b)) {
			*result = b;
			return 1;
		}
	}
	return 0;
}

}
