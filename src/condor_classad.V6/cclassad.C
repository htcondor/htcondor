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

#include "common.h"
#include "cclassad.h"
#include "classad.h"
#include "source.h"
#include "sink.h"
#include "xmlSink.h"

using namespace std;

BEGIN_NAMESPACE( classad )

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
		free(expr);
		if(!result) return 0;
	}

	if(!cclassad_evaluate_to_expr(b,"requirements",&expr)) {
		return 0;
	}

	if(!cclassad_evaluate_to_bool(a,expr,&result)) {
		free(expr);
		if(!result) return 0;
	}

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

END_NAMESPACE
