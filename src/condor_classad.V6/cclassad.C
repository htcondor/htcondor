
#include "cclassad.h"
#include "classad.h"
#include "source.h"
#include "sink.h"

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
