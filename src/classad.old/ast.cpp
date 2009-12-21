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

//******************************************************************************
// ast.C
//
// Implementation of the AST module with an interface to the AttrList module.
//
//******************************************************************************

#include "condor_common.h"
#include "condor_exprtype.h"
#include "condor_ast.h"
#include "condor_classad.h"
#include "condor_buildtable.h"
#include "condor_string.h"

#include "Regex.h"

extern char * format_time(int);
extern void evalFromEnvironment (const char *, EvalResult *);
static bool name_in_list(const char *name, StringList &references);
static void printComparisonOpToStr (char *, ExprTree *, ExprTree *, char *);
static int calculate_math_op_length(ExprTree *lArg, ExprTree *rArg, int op_length);
static void dprintResult(ExprTree *tree, EvalResult *result);

bool classad_debug_function_run = 0;

#define EatSpace(ptr)  while(*ptr != '\0') ptr++;

int EvalExprTree( ExprTree *expr, const AttrList *source, const AttrList *target,
				  EvalResult *result )
{
	return expr->EvalTree( source, target, result );
}

const char *ExprTreeToString( ExprTree *expr ) {
	static MyString value;
	if ( expr == NULL ) {
		return NULL;
	}
	expr->PrintToStr( value );
	return value.Value();
}

// EvalResult ctor
EvalResult::EvalResult()
{
	type = LX_UNDEFINED;
	debug = false;
}

// EvalResult dtor
EvalResult::~EvalResult()
{
	if ((type == LX_STRING || type == LX_TIME) && (s)) {
		delete [] s;
	}
}

void
EvalResult::deepcopy(const EvalResult & rhs)
{
	type = rhs.type;
	debug = rhs.debug;
	switch ( type ) {
		case LX_INTEGER:
		case LX_BOOL:
			i = rhs.i;
			break;
		case LX_FLOAT:
			f = rhs.f;
			break;
		case LX_STRING:
				// need to make a deep copy of the string
			s = strnewp( rhs.s );
			break;
		default:
			break;
	}
}

// EvalResult copy ctor
EvalResult::EvalResult(const EvalResult & rhs)
{
	deepcopy(rhs);
}

// EvalResult assignment op
EvalResult & EvalResult::operator=(const EvalResult & rhs)
{
	if ( this == &rhs )	{	// object assigned to itself
		return *this;		// all done.
	}

		// deallocate any state in this object by invoking dtor
	this->~EvalResult();

		// call copy ctor to make a deep copy of data
	deepcopy(rhs);

		// return reference to invoking object
	return *this;
}


void EvalResult::fPrintResult(FILE *fi)
{
    switch(type)
    {
	case LX_INTEGER :

	     fprintf(fi, "%d", this->i);
	     break;

	case LX_FLOAT :

	     fprintf(fi, "%f", this->f);
	     break;

	case LX_STRING :

	     fprintf(fi, "%s", this->s);
	     break;

	case LX_NULL :

	     fprintf(fi, "NULL");
	     break;

	case LX_UNDEFINED :

	     fprintf(fi, "UNDEFINED");
	     break;

	case LX_ERROR :

	     fprintf(fi, "ERROR");
	     break;

	default :

	     fprintf(fi, "type unknown");
	     break;
    }
    fprintf(fi, "\n");
}

////////////////////////////////////////////////////////////////////////////////
// Expression tree node constructors.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ">" operator.
////////////////////////////////////////////////////////////////////////////////

int Integer::operator >(ExprTree& tree)
{
    EvalResult	tmpResult;

    tree.EvalTree((AttrList*)NULL, &tmpResult);
    if(tmpResult.type == LX_INTEGER)
    {
	return value > tmpResult.i;
    }
    else if(tmpResult.type == LX_FLOAT)
    {
	return value > tmpResult.f;
    }
    return FALSE;
}

int Float::operator >(ExprTree& tree)
{
    EvalResult	tmpResult;

    tree.EvalTree((AttrList*)NULL, &tmpResult);
    if(tmpResult.type == LX_INTEGER)
    {
	return value > tmpResult.i;
    }
    else if(tmpResult.type == LX_FLOAT)
    {
	return value > tmpResult.f;
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// ">=" operator.
////////////////////////////////////////////////////////////////////////////////

int Integer::operator >=(ExprTree& tree)
{
    EvalResult	tmpResult;

    tree.EvalTree((AttrList*)NULL, &tmpResult);
    if(tmpResult.type == LX_INTEGER)
    {
	return value >= tmpResult.i;
    }
    else if(tmpResult.type == LX_FLOAT)
    {
	return value >= tmpResult.f;
    }
    return FALSE;
}

int Float::operator >=(ExprTree& tree)
{
    EvalResult	tmpResult;

    tree.EvalTree((AttrList*)NULL, &tmpResult);
    if(tmpResult.type == LX_INTEGER)
    {
	return value >= tmpResult.i;
    }
    else if(tmpResult.type == LX_FLOAT)
    {
	return value >= tmpResult.f;
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// "<" operator.
////////////////////////////////////////////////////////////////////////////////

int Integer::operator <(ExprTree& tree)
{
    EvalResult	tmpResult;

    tree.EvalTree((AttrList*)NULL, &tmpResult);
    if(tmpResult.type == LX_INTEGER)
    {
	return value < tmpResult.i;
    }
    else if(tmpResult.type == LX_FLOAT)
    {
	return value < tmpResult.f;
    }
    return FALSE;
}

int Float::operator <(ExprTree& tree)
{
    EvalResult	tmpResult;

    tree.EvalTree((AttrList*)NULL, &tmpResult);
    if(tmpResult.type == LX_INTEGER)
    {
	return value < tmpResult.i;
    }
    else if(tmpResult.type == LX_FLOAT)
    {
	return value < tmpResult.f;
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// "<=" operator.
////////////////////////////////////////////////////////////////////////////////

int Integer::operator <=(ExprTree& tree)
{
    EvalResult	tmpResult;

    tree.EvalTree((AttrList*)NULL, &tmpResult);
    if(tmpResult.type == LX_INTEGER)
    {
	return value <= tmpResult.i;
    }
    else if(tmpResult.type == LX_FLOAT)
    {
	return value <= tmpResult.f;
    }
    return FALSE;
}

int Float::operator <=(ExprTree& tree)
{
    EvalResult	tmpResult;

    tree.EvalTree((AttrList*)NULL, &tmpResult);
    if(tmpResult.type == LX_INTEGER)
    {
	return value <= tmpResult.i;
    }
    else if(tmpResult.type == LX_FLOAT)
    {
	return value <= tmpResult.f;
    }
    return FALSE;
  }


////////////////////////////////////////////////////////////////////////////////
// Two overloaded evaluation functions. One take a AttrList, one takes a
// AttrList list.
////////////////////////////////////////////////////////////////////////////////

//------tw 11/16/95
// add one more overloaded evaluation function, it takes two AttrLists,
// one AttrList for "MY." variable valuation and the other AttrList for "TARGET." variable
// evaluation. 
//----------

int ExprTree::EvalTree(const AttrList* l, EvalResult* r)
{
	return EvalTree(l, NULL, r);
}

int ExprTree::EvalTree(const AttrList* l1, const AttrList* l2, EvalResult* r)
{
	int rval;

	if (evalFlag) {
		// circular evaluation
		evalFlag = false;
		r->type = LX_ERROR;
		return FALSE;
	}

	// set evalFlag, evaluate, clear evalFlag
	evalFlag = true;
	rval = _EvalTree(l1, l2, r);
	evalFlag = false;
	
	return rval;
}

int Variable::_EvalTree(const AttrList* classad, EvalResult* val)
{
    ExprTree* tmp = NULL;
    
    if(!val || !classad) 
    {
	return FALSE;
    }
    
    if(!(tmp = classad->LookupExpr(name)))
    {
		val->type = LX_UNDEFINED;
		dprintResult(this, val);
        return TRUE;
    }

    int result = tmp->EvalTree(classad, val);
	dprintResult(this, val);

	return result;
}

int Variable::_EvalTree( const AttrList* my_classad, const AttrList* target_classad, EvalResult* val)
{
	return _EvalTreeRecursive( name, my_classad, target_classad, val, false );
}

/*
Split a variable name into scope.target 
If there is no scope, evaluate it simply.
Otherwise, identify the ClassAd corresponding to the scope, and re-evaluate.
*/

int Variable::_EvalTreeRecursive( const char *adName, const AttrList* my_classad, const AttrList* target_classad, EvalResult* val, bool restrict_search)
{
	if( !val || !adName ) return FALSE;

	MyString n(adName);
	MyString prefix;
	MyString rest;
	
	int dotPos = n.FindChar('.');

	if (dotPos == -1) {
		// no dots in name
		rest = n;
	} else {
		prefix = n.Substr(0, dotPos - 1);
		rest   = n.Substr(dotPos + 1, n.Length());
	}

	if(prefix.Length() > 0) {	
        // Note that we use restrict_search=true instead of simply 
        // passing NULL for the other ClassAd. This is because we might
        // still need to refer to the other ClassAd. For example, evaluating 
        // A in ClassAd 1 should give 3
        // ClassAd 1: A = TARGET.B; C = 3
        // ClassAd 2: B = TARGET.C
		if(!strcasecmp(prefix.Value(),"MY") ) {
			return _EvalTreeRecursive(rest.Value(),my_classad,target_classad,val, true);
		} else if(!strcasecmp(prefix.Value(),"TARGET")) {
			return _EvalTreeRecursive(rest.Value(),target_classad,my_classad,val, true);
        }
	} else {
		return this->_EvalTreeSimple(rest.Value(),my_classad,target_classad,val, restrict_search);
	}

	val->type = LX_UNDEFINED;	
	return TRUE;
}

/*
Once it has been reduced to a simple name, resolve the variable by
looking it up first in MY, then TARGET, and finally, the environment.
*/

int Variable::_EvalTreeSimple( const char *adName, const AttrList *my_classad, const AttrList *target_classad, EvalResult *val, bool restrict_search )
{
	ExprTree *tmp;

	if(my_classad)
	{
		tmp = my_classad->LookupExpr(adName);
		if(tmp) {
			int result = tmp->EvalTree(my_classad, target_classad, val);
			dprintResult(this, val);
			return result;
		}
	}

	if(!restrict_search && target_classad)
	{
		tmp = target_classad->LookupExpr(adName);
		if(tmp) {
			int result = tmp->EvalTree(target_classad, my_classad, val);
			dprintResult(this, val);
			return result;
		}
	}

    evalFromEnvironment(adName,val);
	dprintResult(this, val);
	return TRUE;
}

int Integer::_EvalTree(const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_INTEGER;
    if(unit == 'k')
    {
	val->i = value / 1024;
    }
    else
    {
	val->i = value;
    }

    return TRUE; 
}

//-------tw-------------
int Integer::_EvalTree(const AttrList*, const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_INTEGER;
    if(unit == 'k')
    {
	val->i = value / 1024;
    }
    else
    {
	val->i = value;
    }

    return TRUE; 
}

//--------------------
int Float::_EvalTree(const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_FLOAT;
    if(unit == 'k')
    {
	val->f = value / 1024;
    }
    else
    {
	val->f = value;
    }

    return TRUE;
}


//-------tw-------------
int Float::_EvalTree(const AttrList*, const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_FLOAT;
    if(unit == 'k')
    {
	val->f = value / 1024;
    }
    else
    {
	val->f = value;
    }

    return TRUE;
}

//--------------------------------
int String::_EvalTree(const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_STRING;
    val->s = new char[strlen(value) + 1];
    strcpy(val->s, value);
    return TRUE;
}

int ISOTime::_EvalTree(const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_TIME;
    val->s = new char[strlen(time) + 1];
    strcpy(val->s, time);
    return TRUE;
}

//-------tw-----------------------------

int String::_EvalTree(const AttrList*, const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_STRING;
    val->s = new char[strlen(value) + 1];
    strcpy(val->s, value);
    return TRUE;
}

int ISOTime::_EvalTree(const AttrList*, const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_TIME;
    val->s = new char[strlen(time) + 1];
    strcpy(val->s, time);
    return TRUE;
}

//-----------------------------------
int ClassadBoolean::_EvalTree(const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_INTEGER;
    val->i = value;
    return TRUE;
}


//-----------tw------------------------

int ClassadBoolean::_EvalTree(const AttrList*, const AttrList*, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_INTEGER;
    val->i = value;
    return TRUE;
}

//-----------------------------------


int Undefined::_EvalTree(const AttrList*, EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_UNDEFINED;
    return TRUE;
}

//------------tw-------------------
int Undefined::_EvalTree(const AttrList*, const AttrList*,  EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_UNDEFINED;
    return TRUE;
}
//--------------------------------

int Error::_EvalTree(const AttrList*, EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_ERROR;
    return TRUE;
}

//------------tw-------------------
int Error::_EvalTree(const AttrList*, const AttrList*,  EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_ERROR;
    return TRUE;
}
//--------------------------------

void ExprTree::GetReferences(const AttrList * /* base_attlrist */,
							 StringList & /* internal_references */,
							 StringList & /* external_references */) const
{
	return;
}

void BinaryOpBase::GetReferences(const AttrList *base_attrlist,
								 StringList &internal_references,
								 StringList &external_references) const 
{
	if (lArg != NULL) {
		lArg->GetReferences(base_attrlist, internal_references, external_references);
	}
	if (rArg != NULL) {
		rArg->GetReferences(base_attrlist, internal_references, external_references);
	}
	return;
}

void AssignOpBase::GetReferences(const AttrList *base_attrlist,
								 StringList &internal_references,
								 StringList &external_references) const
{
	// We don't look at the left argument, because we know that
	// we won't want to add it to the internal or external references.
	if (rArg != NULL) {
		rArg->GetReferences(base_attrlist, internal_references, external_references);
	}
	return;
}

void VariableBase::GetReferences(const AttrList *base_attrlist,
								 StringList &internal_references,
								 StringList &external_references) const
{
	bool is_external_reference; // otherwise, internal
	char *simplified_name;

	is_external_reference = base_attrlist->IsExternalReference(name, 
													  &simplified_name);
	if (is_external_reference) {
		if (!name_in_list(simplified_name, external_references)) {
			external_references.append(simplified_name);
		}
	}
	else {
		if (!name_in_list(simplified_name, internal_references)) {
			internal_references.append(simplified_name);
		}
	}
	// We added simplified_name to the list, but it was copied
	// when we did the append, so we need to free it now. 
	free(simplified_name);

	return;
}

static bool name_in_list(const char *name, StringList &references)
{
	return ( references.contains_anycase(name) ? true : false );
}

// Calculate how many bytes an expression will print to
int Variable::CalcPrintToStr(void)
{
	return strlen(name);
}

int Integer::CalcPrintToStr(void)
{
	int  length;
	char printed_representation[256];
	printed_representation[0] = 0;
	PrintToStr(printed_representation);
	length = strlen(printed_representation);
	return length;
}

int Float::CalcPrintToStr(void)
{
	int  length;
	char printed_representation[256];
	printed_representation[0] = 0;
	PrintToStr(printed_representation);
	length = strlen(printed_representation);
	return length;
}

int String::CalcPrintToStr(void)
{
	int   length;
	char  *p;
	length = 0;
	
	// Unfortunately, we have to walk the string to find the length.
	// This is because it contain quote marks
	for (p = value; p && *p != 0; p++) {
		if (*p == '"') {
			length += 2;
		} else {
			length++;
		}
	}
	// Then we have to add 2, for the opening and closing quote marks.
	return length + 2;
}

int ISOTime::CalcPrintToStr(void)
{
	// Add 2, for the opening and closing quote (') marks.
	return strlen(time) + 2;
}

int ClassadBoolean::CalcPrintToStr(void)
{
	int  length;
	char printed_representation[256];
	printed_representation[0] = 0;
	PrintToStr(printed_representation);
	length = strlen(printed_representation);
	return length;
}

int Error::CalcPrintToStr(void)
{
	int  length;
	char printed_representation[256];
	printed_representation[0] = 0;
	PrintToStr(printed_representation);
	length = strlen(printed_representation);
	return length;
}

int Undefined::CalcPrintToStr(void)
{
	int  length;
	char printed_representation[256];
	printed_representation[0] = 0;
	PrintToStr(printed_representation);
	length = strlen(printed_representation);
	return length;
}

int AddOp::CalcPrintToStr(void)
{
	int  length;

	if (lArg == NULL) {
		length = 1                   // Left parenthesis
			+ rArg->CalcPrintToStr() // Parenthesized expressions
			+ 1;                     // Right parenthesis
	}
	else {
		length = lArg->CalcPrintToStr() // Left subexpression
			+ 3                         // for " + "
			+ rArg->CalcPrintToStr();   // Right subexpression
		if (unit == 'k') {
			length += 2; // for " k"
		}
	}

	return length;
}

int SubOp::CalcPrintToStr(void)
{
	int length;

	length = calculate_math_op_length(lArg, rArg, 3);

	if (unit == 'k') {
		length += 2; // for " k"
	}

	return length;
}

int MultOp::CalcPrintToStr(void)
{
	int length;

	length = calculate_math_op_length(lArg, rArg, 3);

	if (unit == 'k') {
		length += 2; // for " k"
	}

	return length;
}


int DivOp::CalcPrintToStr(void)
{
	int length;

	length = calculate_math_op_length(lArg, rArg, 3);

	if (unit == 'k') {
		length += 2; // for " k"
	}

	return length;
}

/// ----------------
int MetaEqOp::CalcPrintToStr(void)
{
	// 5 for " =?= "
	return calculate_math_op_length(lArg, rArg, 5);
}

int MetaNeqOp::CalcPrintToStr(void)
{
	// 5 for " =!= "
	return calculate_math_op_length(lArg, rArg, 5);
}

int EqOp::CalcPrintToStr(void)
{
	// 4 for " == "
	return calculate_math_op_length(lArg, rArg, 4);
}

int NeqOp::CalcPrintToStr(void)
{
	// 4 for " != "
	return calculate_math_op_length(lArg, rArg, 4);
}

int GtOp::CalcPrintToStr(void)
{
	// 3 for " > "
	return calculate_math_op_length(lArg, rArg, 3);	
}

int GeOp::CalcPrintToStr(void)
{
	// 4 for " >= "
	return calculate_math_op_length(lArg, rArg, 4);	
}

int LtOp::CalcPrintToStr(void)
{
	// 3 for " < "
	return calculate_math_op_length(lArg, rArg, 3);	
}

int LeOp::CalcPrintToStr(void)
{
	// 4 for " <= "
	return calculate_math_op_length(lArg, rArg, 4);	
}

int AndOp::CalcPrintToStr(void)
{
	// 4 for " && "
	return calculate_math_op_length(lArg, rArg, 4);
}

int OrOp::CalcPrintToStr(void)
{
	// 4 for " || "
	return calculate_math_op_length(lArg, rArg, 4);
}

int AssignOp::CalcPrintToStr(void)
{
	// 3 for " = "
	return calculate_math_op_length(lArg, rArg, 3);
}

/// -----------------

////////////////////////////////////////////////////////////////////////////////
// Print an Expression to a string.                                           //
////////////////////////////////////////////////////////////////////////////////

void Variable::PrintToStr(char* str)
{
  strcat(str, name);
}

void Integer::PrintToStr(char* str)
{
  sprintf(str+strlen(str), "%d", value);
  if(unit == 'k')
	strcat(str, " k");
}

void Float::PrintToStr(char* str)
{
  sprintf(str+strlen(str), "%f", value);
  if(unit == 'k')
	strcat(str, " k");
}

void String::PrintToStr(char* str)
{
  char*		ptr1 = value;
  char*		ptr2 = str;

  while(*ptr2 != '\0') ptr2++;
  *ptr2 = '"';
  ptr2++;
  while( ptr1 && *ptr1 != '\0')
  {
	if(*ptr1 == '"')
	{
		*ptr2 = '\\';
		ptr2++;
	}
	*ptr2 = *ptr1;
	ptr1++;
	ptr2++;
  }
  *ptr2 = '"';
  *(ptr2 + 1) = '\0';
}

void ISOTime::PrintToStr(char* str)
{
  char*		ptr1 = time;
  char*		ptr2 = str;

  while(*ptr2 != '\0') ptr2++;
  *ptr2 = '\'';
  ptr2++;
  while(*ptr1 != '\0')
  {
	*ptr2 = *ptr1;
	ptr1++;
	ptr2++;
  }
  *ptr2 = '\'';
  *(ptr2 + 1) = '\0';
}

void ClassadBoolean::PrintToStr(char* str)
{
	if( value )
		strcat( str, "TRUE" );
	else
		strcat( str, "FALSE" );
}

void Undefined::PrintToStr(char* str)
{
	strcat( str, "UNDEFINED" );
}

void Error::PrintToStr(char* str)
{
	strcat( str, "ERROR" );
}

void AddOp::PrintToStr(char* str)
{
	if( !lArg ) {
		// HACK!!  No lArg implies user-directed parenthesization
		strcat( str, "(" );
		((ExprTree*)rArg)->PrintToStr( str );
		strcat( str, ")" );
		return;
	}

	// lArg available --- regular addition operation
	((ExprTree*)lArg)->PrintToStr(str);
    strcat(str, " + ");
	((ExprTree*)rArg)->PrintToStr(str);
    if(unit == 'k') strcat(str, " k");
}

void SubOp::PrintToStr(char* str)
{
    if(lArg) {
		((ExprTree*)lArg)->PrintToStr(str);
    }
    strcat(str, " - ");
    if(rArg) {
		((ExprTree*)rArg)->PrintToStr(str);
	}
    if(unit == 'k') strcat(str, " k"); 
}


void MultOp::PrintToStr(char* str)
{
    if(lArg) {
		((ExprTree*)lArg)->PrintToStr(str);
    }
    strcat(str, " * ");
    if(rArg) {
		((ExprTree*)rArg)->PrintToStr(str);
    }
    if(unit == 'k') strcat(str, " k");
}

void DivOp::PrintToStr(char* str)
{
    if(lArg) {
		((ExprTree*)lArg)->PrintToStr(str);
    }
    strcat(str, " / ");
    if(rArg) {
		((ExprTree*)rArg)->PrintToStr(str);
    }
    if(unit == 'k') strcat(str, " k");
}

void MetaEqOp::PrintToStr(char* str)
{
    printComparisonOpToStr (str, lArg, rArg, " =?= ");
}

void MetaNeqOp::PrintToStr(char* str)
{
    printComparisonOpToStr (str, lArg, rArg, " =!= ");
}

void EqOp::PrintToStr(char* str)
{
    printComparisonOpToStr (str, lArg, rArg, " == ");
}

void NeqOp::PrintToStr(char* str)
{
    printComparisonOpToStr (str, lArg, rArg, " != ");
}

void GtOp::PrintToStr(char* str)
{
	printComparisonOpToStr (str, lArg, rArg, " > ");
}

void GeOp::PrintToStr(char* str)
{
    printComparisonOpToStr (str, lArg, rArg, " >= ");
}

void LtOp::PrintToStr(char* str)
{
	printComparisonOpToStr (str, lArg, rArg, " < ");
}

void LeOp::PrintToStr(char* str)
{
	printComparisonOpToStr (str, lArg, rArg, " <= ");
}

void AndOp::PrintToStr(char* str)
{
    if(lArg) {
		((ExprTree*)lArg)->PrintToStr(str);
	}
    strcat(str, " && ");
    if(rArg) {
		((ExprTree*)rArg)->PrintToStr(str);
	}
}

void OrOp::PrintToStr(char* str)
{
	if( lArg )((ExprTree*)lArg)->PrintToStr(str);
    strcat(str, " || ");
    if(rArg) ((ExprTree*)rArg)->PrintToStr(str);
}

void AssignOp::PrintToStr(char* str)
{
    if(lArg) ((ExprTree*)lArg)->PrintToStr(str);
    strcat(str, " = ");
    if(rArg) ((ExprTree*)rArg)->PrintToStr(str);
}



static void 
printComparisonOpToStr (char *str, ExprTree *lArg, ExprTree *rArg, char *op)
{
    if(lArg) {
		((ExprTree*)lArg)->PrintToStr(str);
    }
    strcat(str, op);
    if(rArg) {
		((ExprTree*)rArg)->PrintToStr(str);
    }
}

static int
calculate_math_op_length(ExprTree *lArg, ExprTree *rArg, int op_length)
{
	int length;

	length = 0;
	if (lArg) {
		length += lArg->CalcPrintToStr();
	}
	length += op_length; // Like " - "
	if (rArg) {
		length += rArg->CalcPrintToStr();
	}
	return length;
}

ExprTree*  
Variable::DeepCopy(void) const
{
	Variable *copy;

	copy = new Variable(name);
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*  
Integer::DeepCopy(void) const
{
	Integer *copy;

	copy = new Integer(value);
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*  
Float::DeepCopy(void) const
{
	Float *copy;

	copy = new Float(value);
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*  
ClassadBoolean::DeepCopy(void) const
{
	ClassadBoolean *copy;

	copy = new ClassadBoolean(value);
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*  
String::DeepCopy(void) const
{
	String *copy;

	copy = new String(value);
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*  
ISOTime::DeepCopy(void) const
{
	ISOTime *copy;

	copy = new ISOTime(time);
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*  
Undefined::DeepCopy(void) const
{
	Undefined *copy;

	copy = new Undefined();
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*  
Error::DeepCopy(void) const
{
	Error *copy;

	copy = new Error();
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*
AddOp::DeepCopy(void) const
{
	AddOp     *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	/* We have to be careful with the AddOp, because it is overloaded to be used
	 * as the parenthesis grouping.
	 */
	copy_of_larg = copy_of_rarg = NULL;
	if (lArg != NULL) {
		copy_of_larg = lArg->DeepCopy();
	}
	if (rArg != NULL) {
		copy_of_rarg = rArg->DeepCopy();
	}

	copy = new AddOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
SubOp::DeepCopy(void) const
{
	SubOp  *copy;
	ExprTree  *copy_of_larg = NULL;
	ExprTree  *copy_of_rarg;

	if(lArg) {
		copy_of_larg = lArg->DeepCopy();
	}
	ASSERT(rArg);
	copy_of_rarg = rArg->DeepCopy();

	copy = new SubOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
MultOp::DeepCopy(void) const
{
	MultOp    *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new MultOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
DivOp::DeepCopy(void) const
{
	DivOp     *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new DivOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
MetaEqOp::DeepCopy(void) const
{
	MetaEqOp  *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new MetaEqOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
MetaNeqOp::DeepCopy(void) const
{
	MetaNeqOp *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new MetaNeqOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
EqOp::DeepCopy(void) const
{
	EqOp      *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new EqOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
NeqOp::DeepCopy(void) const
{
	NeqOp     *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new NeqOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
GtOp::DeepCopy(void) const
{
	GtOp      *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new GtOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
GeOp::DeepCopy(void) const
{
	GeOp      *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new GeOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
LtOp::DeepCopy(void) const
{
	LtOp      *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new LtOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
LeOp::DeepCopy(void) const
{
	LeOp      *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new LeOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
AndOp::DeepCopy(void) const
{
	AndOp     *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new AndOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
OrOp::DeepCopy(void) const
{
	OrOp      *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new OrOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}

ExprTree*
AssignOp::DeepCopy(void) const
{
	AssignOp  *copy;
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
	copy_of_rarg = rArg->DeepCopy();

	copy = new AssignOp(copy_of_larg, copy_of_rarg);
	CopyBaseExprTree(copy);
	return copy;
}
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include "classad_shared.h"

int Function::CalcPrintToStr(void)
{
	int      length;
	int      i;
	int      num_args;
	ExprTree *arg;

	length = 0;
	length += strlen(name);
	length += 1; // for left paren

	arguments->Rewind();
	i = 0;
	num_args = arguments->Length();
	while (arguments->Next(arg)) {
		length += arg->CalcPrintToStr();
		i++;
		if (i < num_args) {
			length += 2; // for "; "
		}
	}
	length += 1; // for right paren
	
	return length;
}

void Function::PrintToStr(char *s)
{
	ExprTree *arg;
	int i, num_args;

	arguments->Rewind();
	i = 0;
	num_args = arguments->Length();
	strcat(s, name);
	strcat(s, "(");
	while (arguments->Next(arg)) {
		arg->PrintToStr(s);
		i++;
		if (i < num_args) {
			strcat(s, ", ");
		}
	}
	strcat(s, ")");

	return;
}

ExprTree *Function::DeepCopy(void) const
{
	Function *copy;

	copy = new Function(name);
	CopyBaseExprTree(copy);

	ListIterator< ExprTree > iter(*arguments);
	ExprTree *arg;

	iter.ToBeforeFirst();
	while (iter.Next(arg)) {
		copy->AppendArgument(arg->DeepCopy());
	}

	return copy;
}

int Function::_EvalTree(const AttrList *attrlist, EvalResult *result)
{
	_EvalTree(attrlist, NULL, result);
	return 0;
}

int Function::_EvalTree(const AttrList *attrlist1, const AttrList *attrlist2, EvalResult *result)
{
	int        number_of_args, i;
	int        successful_eval;
	EvalResult *evaluated_args;
	bool must_eval_to_strings = false;
	bool done = false;

	if ( result == NULL ) {
		return FALSE;
	}

	successful_eval = FALSE;
	result->type = LX_UNDEFINED;

	if ( !strcasecmp(name, "debug") ) {
		result->debug = true;
	}

		// treat calls to function IfThenElse() special, because we cannot
		// evaluate the arguments ahead of time (argument evaluation must
		// be lazy for IfThenElse).
		// also, many of the string functions need all their arguments
		// converted to strings - set a flag if we need to do this.
	if ( !strcasecmp(name,"ifthenelse") ) {
		successful_eval = FunctionIfThenElse(attrlist1,attrlist2,result);
		done = true;
	} else
	if ( 
		 !strcasecmp(name,"strcat") ||
		 !strcasecmp(name,"strcmp") ||
		 !strcasecmp(name,"stricmp") ||
		 !strcasecmp(name,"toUpper") ||
		 !strcasecmp(name,"toLower") ||
		 !strcasecmp(name,"size") ||
		 !strcasecmp(name,"eval") ) 
	{
		must_eval_to_strings = true;
	}

	if (!done) {
		number_of_args = arguments->Length();
		evaluated_args = new EvalResult[number_of_args];
		
		ListIterator<ExprTree> iter(*arguments);
		ExprTree *arg;

		i = 0;
		while (iter.Next(arg)) {
			evaluated_args[i].debug = result->debug;
			if ( must_eval_to_strings ) {
				if (!EvaluateArgumentToString(arg, attrlist1,attrlist2,
											  &evaluated_args[i++]))
				{
					// if all args must be converted to strings, and we 
					// fail to convert an arg to a string, then fail.
					result->type = LX_ERROR;
					done = true;
					break;		// no need to look at the other args
				}
			} else {
				EvaluateArgument( arg, attrlist1, attrlist2, &evaluated_args[i++] );
			}
		}
	
		if ( !done ) {	
        if (!strcasecmp(name, "gettime")) {
			successful_eval = FunctionGetTime(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "time")) {
			successful_eval = FunctionTime(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "interval")) {
			successful_eval = FunctionInterval(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "random")) {
			successful_eval = FunctionRandom(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "_debug_function_")) {
            successful_eval = FunctionClassadDebugFunction(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "isundefined")) {
			successful_eval = FunctionIsUndefined(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "iserror")) {
			successful_eval = FunctionIsError(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "isstring")) {
			successful_eval = FunctionIsString(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "isinteger")) {
			successful_eval = FunctionIsInteger(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "isreal")) {
			successful_eval = FunctionIsReal(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "isboolean")) {
			successful_eval = FunctionIsBoolean(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "string")) {
			successful_eval = FunctionString(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "real")) {
			successful_eval = FunctionReal(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "int")) {
			successful_eval = FunctionInt(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "floor")) {
			successful_eval = FunctionFloor(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "round")) {
			successful_eval = FunctionRound(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "ceiling")) {
			successful_eval = FunctionCeiling(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "strcat")) {
			successful_eval = FunctionStrcat(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "substr")) {
			successful_eval = FunctionSubstr(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "strcmp")) {
			successful_eval = FunctionStrcmp(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stricmp")) {
			successful_eval = FunctionStricmp(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "toupper")) {
			successful_eval = FunctionToUpper(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "tolower")) {
			successful_eval = FunctionToLower(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "size")) {
			successful_eval = FunctionSize(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stringlistsize")) {
			successful_eval = FunctionStringlistSize(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stringlistsum")) {
			successful_eval = FunctionStringlistSum(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stringlistavg")) {
			successful_eval = FunctionStringlistAvg(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stringlistmin")) {
			successful_eval = FunctionStringlistMin(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stringlistmax")) {
			successful_eval = FunctionStringlistMax(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stringlistmember")) {
			successful_eval = FunctionStringlistMember(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stringlistimember")) {
			successful_eval = FunctionStringlistIMember(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "stringlist_regexpMember")) {
			successful_eval = FunctionStringlistRegexpMember(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "regexp")) {
			successful_eval = FunctionRegexp(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "regexps")) {
			successful_eval = FunctionRegexps(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "formattime")) {
			successful_eval = FunctionFormatTime(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "debug")) {
			*result = evaluated_args[0];
			successful_eval = true;
		} else if (!strcasecmp(name, "eval")) {
			successful_eval = FunctionEval(attrlist1, attrlist2, number_of_args, evaluated_args, result);
		}
#ifdef HAVE_DLOPEN
        else {
			successful_eval = FunctionSharedLibrary(number_of_args, 
													evaluated_args, result);
		}
#else
        else {
            successful_eval = false;
        }
#endif

		dprintResult(this, result);

		if (result->debug) {
			if (strcasecmp(name, "debug") == 0) {
				result->debug = false;
			}
		  }
		}	// of if (!done)

		delete [] evaluated_args;
	}

	return successful_eval;
}

void EvalResult::toString(bool force)
{
	switch(type) {
		case LX_STRING:
			break;
		case LX_FLOAT: {
			MyString buf;
			buf.sprintf("%lf",f);
			s = strnewp(buf.Value());
			type = LX_STRING;
			break;
		}
		case LX_BOOL:	
			type = LX_STRING;
			if (i) {
				s = strnewp("TRUE");
			} else {
				s = strnewp("FALSE");
			}	
			break;
		case LX_INTEGER: {
			MyString buf;
			buf.sprintf("%d",i);
			s = strnewp(buf.Value());
			type = LX_STRING;
			break;
		}
		case LX_UNDEFINED:
			if( force ) {
				s = strnewp("UNDEFINED");
				type = LX_STRING;
			}
			break;
		case LX_ERROR:
			if( force ) {
				s = strnewp("ERROR");
				type = LX_STRING;
			}
			break;
		default:
			ASSERT("Unknown classad result type");
	}
}

int Function::EvaluateArgumentToString(
	ExprTree *arg,
	const AttrList *attrlist1,
	const AttrList *attrlist2,
	EvalResult *result) const        // OUT: the result of calling the function
{
	result->type = LX_ERROR;

	EvaluateArgument( arg, attrlist1, attrlist2, result );

	result->toString();

	if ( result->type == LX_STRING ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

bool string_is_all_whitespace(char *s)
{
	bool is_all_whitespace = true;

	while (*s != 0) {
		if (!isspace(*s)) {
			is_all_whitespace = false;
			break;
		} else {
			s++;
		}
	}
	return is_all_whitespace;
}

#ifdef HAVE_DLOPEN
int Function::FunctionSharedLibrary(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	char *shared_library_location;
	int  eval_succeeded;

	eval_succeeded = false;
	if ((shared_library_location = param("CLASSAD_LIB_PATH")) != NULL){
		void *dl_handle;
		ClassAdSharedFunction function;
		
		dl_handle = dlopen(shared_library_location, RTLD_LAZY);
		if (dl_handle) {
			function = (ClassAdSharedFunction) dlsym(dl_handle, name);
			if (function != NULL) {
				ClassAdSharedValue  function_result;
				ClassAdSharedValue  *function_args;

				// Prepare the arguments for passing to the external library
				// Note that we don't just use EvalResult, because we 
				// want to give the DZero folks a header file that is completely
				// independent of anything else in Condor.
				if (number_of_args > 0) {
					function_args = new ClassAdSharedValue[number_of_args];
					for (int arg_index = 0; arg_index < number_of_args; arg_index++) {
						switch (evaluated_args[arg_index].type) {
						case LX_INTEGER:
							function_args[arg_index].type = ClassAdSharedType_Integer;
							function_args[arg_index].integer = evaluated_args[arg_index].i;
							break;
						case LX_FLOAT:
							function_args[arg_index].type = ClassAdSharedType_Float;
							function_args[arg_index].real = evaluated_args[arg_index].f;
							break;
						case LX_STRING:
							function_args[arg_index].type = ClassAdSharedType_String;
							function_args[arg_index].text = evaluated_args[arg_index].s;
							break;
						case LX_UNDEFINED:
							function_args[arg_index].type = ClassAdSharedType_Undefined;
							break;
						default:
							function_args[arg_index].type = ClassAdSharedType_Error;
							break;
						}
					}
				} else {
					function_args = NULL;
				}

				function(number_of_args, function_args, &function_result);
                delete [] function_args;

				switch (function_result.type) {
				case ClassAdSharedType_Integer:
					result->type = LX_INTEGER;
					result->i = function_result.integer;
					break;
				case ClassAdSharedType_Float:
					result->type = LX_FLOAT;
					result->f = function_result.real;
					break;
				case ClassAdSharedType_String:
					result->type = LX_STRING;
					result->s = function_result.text;
					break;
				case ClassAdSharedType_Undefined:
					result->type = LX_UNDEFINED;
					break;
				default: 
					result->type = LX_ERROR;
					break;
				}
				eval_succeeded = true;
			}
		}
		free(shared_library_location);
	}
	return eval_succeeded;
}
#endif

int Function::FunctionGetTime(
	int /* number_of_args */,          // IN:  size of evaluated args array
	EvalResult * /* evaluated_args */, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	time_t current_time = time(NULL);

	result->i = (int) current_time;
	result->type = LX_INTEGER;
	return TRUE;
}

int Function::FunctionTime(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	return FunctionGetTime(number_of_args, evaluated_args, result);
}

int Function::FunctionInterval(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

    if (evaluated_args[0].type != LX_INTEGER) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_STRING;
	result->s = strnewp(format_time(evaluated_args[0].i));
	return TRUE;
}

extern "C" int get_random_int();
extern "C" float get_random_float();

int Function::FunctionRandom(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
    bool  success = false;
	result->type = LX_ERROR;

    if (number_of_args == 0) {
        // If we get no arguments, we return a random number between 0 and 1.0
        result->f = get_random_float();
		result->type = LX_FLOAT;
        success = true;
    } else if (number_of_args == 1) {
        // If we get one integer argument, we return a random number
        // between 0 and that number
        if (evaluated_args[0].type == LX_INTEGER) {
			result->type = LX_INTEGER;
            result->i = get_random_int() % evaluated_args[0].i;
            success = true;
        } 
		if (evaluated_args[0].type == LX_FLOAT) {
			result->type = LX_FLOAT;
            result->f = get_random_float() * evaluated_args[0].f;
            success = true;
        }
    }

	return success;
}

int Function::FunctionIsUndefined(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_INTEGER;
	if ( evaluated_args[0].type == LX_UNDEFINED ) {
		result->i = 1;
	} else {
		result->i = 0;
	}

	return true;
}

int Function::FunctionIsError(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_INTEGER;
	if ( evaluated_args[0].type == LX_ERROR ) {
		result->i = 1;
	} else {
		result->i = 0;
	}

	return true;
}

int Function::FunctionIsString(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_INTEGER;
	if ( evaluated_args[0].type == LX_STRING ) {
		result->i = 1;
	} else {
		result->i = 0;
	}

	return true;
}


int Function::FunctionIsInteger(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_INTEGER;
	if ( evaluated_args[0].type == LX_INTEGER ) {
		result->i = 1;
	} else {
		result->i = 0;
	}

	return true;
}


int Function::FunctionIsReal(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_INTEGER;
	if ( evaluated_args[0].type == LX_FLOAT ) {
		result->i = 1;
	} else {
		result->i = 0;
	}

	return true;
}

int Function::FunctionIsBoolean(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_INTEGER;
	if ( (evaluated_args[0].type == LX_BOOL) ||
		 (evaluated_args[0].type == LX_INTEGER && // int val of 0 or 1 is bool
		     (evaluated_args[0].i == 0  || evaluated_args[0].i == 1)) )
	{
		result->i = 1;
	} else {
		result->i = 0;
	}

	return true;
}


int Function::FunctionString(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	*result = evaluated_args[0];
	result->toString();
	if( result->type == LX_ERROR ) {
		return FALSE;
	}

	return TRUE;
}

int Function::FunctionReal(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_FLOAT;
	result->f = 0;

	switch ( evaluated_args[0].type ) {
		case LX_FLOAT:
			// if input x is a Real, the result is x.
			result->f = evaluated_args[0].f;
			break;
		case LX_INTEGER:
			// if integer, upgrade to a Real.
			// to implement, we just cast, since this is what C++ will do.
			result->f = (float)(evaluated_args[0].i);
			break;
		case LX_BOOL:
			if ( evaluated_args[0].i ) 
				result->f = 1.0;
			else
				result->f = 0.0;
			break;
		case LX_STRING: 
			// convert string to Real, or return error if string
			// does not represent a Real.
			if (!evaluated_args[0].s) {
				result->type = LX_ERROR;
				return false;
			}
			if (sscanf(evaluated_args[0].s,"%f",&result->f) != 1) {
				result->type = LX_ERROR;
				return false;
			}
			break;
		default:
			// likely error or undefined
			result->type = LX_ERROR;
			return false;
	}

	return true;
}

int Function::FunctionFloor(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_INTEGER;
	result->i = 0;

	// If input arg x is integer, return x.  Otherwise, x is converted
	// to a real w/ FunctionReal(), and floor() is applied.
	if ( evaluated_args[0].type == LX_INTEGER ) {
		result->i = evaluated_args[0].i;
	} else {
		EvalResult real_result;
		if ( FunctionReal(number_of_args,evaluated_args,&real_result) ) {
			result->i = (int)floor(real_result.f);
		} else {
			result->type = LX_ERROR;
			return FALSE;
		}
	}

	return TRUE;
}


int Function::FunctionRound(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_INTEGER;
	result->i = 0;

	// If input arg x is integer, return x.  Otherwise, x is converted
	// to a real w/ FunctionReal(), and ceil() is applied.
	if ( evaluated_args[0].type == LX_INTEGER ) {
		result->i = evaluated_args[0].i;
	} else {
		EvalResult real_result;
		if ( FunctionReal(number_of_args,evaluated_args,&real_result) ) {
			result->i = (int)rint(real_result.f);
		} else {
			result->type = LX_ERROR;
			return FALSE;
		}
	}

	return TRUE;
}

int Function::FunctionCeiling(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_INTEGER;
	result->i = 0;

	// If input arg x is integer, return x.  Otherwise, x is converted
	// to a real w/ FunctionReal(), and ceil() is applied.
	if ( evaluated_args[0].type == LX_INTEGER ) {
		result->i = evaluated_args[0].i;
	} else {
		EvalResult real_result;
		if ( FunctionReal(number_of_args,evaluated_args,&real_result) ) {
			result->i = (int)ceil(real_result.f);
		} else {
			result->type = LX_ERROR;
			return FALSE;
		}
	}

	return TRUE;
}


int Function::FunctionStrcat(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	// NOTE: ALL ARGUMENTS HAVE BEEN CONVERTED INTO STRING TYPES
	// BEFORE THIS FUNCTION WAS INVOKED.
	
	int i;
	MyString tempStr;

	for (i=0; i< number_of_args; i++) {
		ASSERT(evaluated_args[i].type == LX_STRING);
		tempStr += evaluated_args[i].s;
	}

	result->type = LX_STRING;
	result->s = strnewp( tempStr.Value() );

	return TRUE;
}



int Function::FunctionInt(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return false;
	}

	result->type = LX_INTEGER;
	result->i = 0;

	switch ( evaluated_args[0].type ) {
		case LX_INTEGER:
			// if input x is an Integer, the result is x.
			result->i = evaluated_args[0].i;
			break;
		case LX_FLOAT:
			// if input x is Float, it is truncated towards zero.
			// to implement, we just cast, since this is what C++ will do.
			result->i = (int)(evaluated_args[0].f);
			break;
		case LX_BOOL:
			if ( evaluated_args[0].i ) 
				result->i = 1;
			else
				result->i = 0;
			break;
		case LX_STRING: 
			// convert string to int, or return error if string
			// does not represent an int.
			if (!evaluated_args[0].s) {
				result->type = LX_ERROR;
				return false;
			}
			result->i = atoi(evaluated_args[0].s);
			if ( result->i == 0 ) {
				// this sucks.  atoi returns 0 on error, so
				// here we try to figure out if we have a 0
				// because that is what the string has, or 
				// we have a 0 due to an error.
				int c;
				int i=0;
				while ( (c=evaluated_args[0].s[i++]) ) {
					if ( (!isspace(c)) && c!='0' &&
						 c!='+' && c!='-' && c!='.' ) 
					{
						result->type = LX_ERROR;
						return false;
					}
				}
			}
			break;
		default:
			// likely error or undefined
			result->type = LX_ERROR;
			return false;
	}

	return true;
}



/*
	IfThenElse( condition ; then ; else )
	If condition evaluates to TRUE, return evaluated 'then'.
	If condition evaluates to FALSE, return evaluated 'else'.
	If condition evaluates to UNDEFIEND, return UNDEFINED.
	If condition evaluates to ERROR, return ERROR.
	If condition is of type string or null, return ERROR.
	If three arguments are not passed in, return ERROR.
*/
int Function::FunctionIfThenElse(
	const AttrList *attrlist1,
	const AttrList *attrlist2,
	EvalResult *result)         // OUT: the result of calling the function
{
	bool condition = false;
	EvalResult conditionclause;
	ExprTree *arg = NULL;

	int number_of_args = arguments->Length();

	conditionclause.debug = result->debug;

	if ( number_of_args != 3 ) {
		// we must have three arguments
		result->type = LX_ERROR;	
		return false;
	}

	ListIterator<ExprTree> iter(*arguments);

		// pop off and evaluate the condition clause (1st argument)
	iter.Next(arg);		// arg now has the first argument (condition clause)
	EvaluateArgument( arg, attrlist1, attrlist2, &conditionclause );

	switch ( conditionclause.type ) {
		case LX_BOOL:
		case LX_INTEGER:
			if ( conditionclause.i ) {
				condition = true;
			}
			break;
		case LX_FLOAT:
			if ( conditionclause.f ) {
				condition = true;
			}
			break;
		case LX_UNDEFINED:
			result->type = LX_UNDEFINED;
			return true;
		default:  // will catch types null, error, string
			result->type = LX_ERROR;	
			return false;
	}	// end of switch

	if ( condition ) {
			// Condition is true - we want to return the second argument
		iter.Next(arg);		// arg now has the second argument (then clause)
			// evaluate the second argument (then clause), store in result
		EvaluateArgument( arg, attrlist1, attrlist2, result );	
	} else {
			// Condition is false - we want to return the third argument
		iter.Next(arg);		// arg now has the second argument (then clause)
		iter.Next(arg);		// arg now has the third argument (else clause)
			// evaluate the third argument (else clause), store in result
		EvaluateArgument( arg, attrlist1, attrlist2, result );	
	}

	return true;
}

int Function::FunctionClassadDebugFunction(
	int /* number_of_args */,         
	EvalResult * /* evaluated_args */, 
	EvalResult *result)         
{
    classad_debug_function_run = true;
    result->i    = 1;
    result->type = LX_INTEGER;

	return TRUE;
}

int Function::FunctionSubstr(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{

	/* 
	substr(string s, int offset [, int length ]) returns string.

	The result is the substring of s starting at the position indicated by
	offset with the length indicated by length. The first character of s is at
	offset 0. If offset is negative, it is replaced by length(s) - offset. If
	length is omitted, the substring extends to the end of s. If length is
	negative, an intermediate result is computed as if length were omitted, and
	then -length characters are deleted from the right end of the result. If
	the resulting substring lies partially outside the limits of s, the part
	that lies within s is returned. If the substring lies entirely outside s or
	has negative length (because of a negative length argument), the result is
	the null string. 
	*/
	int length;
	int offset;
	char *s;

	if ( (number_of_args < 2) || ( number_of_args >  3 )) {
		result->type = LX_ERROR;
		return FALSE;
	}

	if( (evaluated_args[0].type != LX_STRING) || 
		(evaluated_args[1].type != LX_INTEGER)) {
		result->type = LX_ERROR;
		return FALSE;
	}

	s = evaluated_args[0].s;
	offset = evaluated_args[1].i;

	if (offset < 0) {
		offset = strlen(s) + offset;
	}

	if ( number_of_args == 3 ) {
        if (evaluated_args[2].type != LX_INTEGER) {
				result->type = LX_ERROR;
				return FALSE;
		}
		length = evaluated_args[2].i;
	} else {
		length = strlen(s) - offset;
	}

	if( ( offset < 0) || ( ((unsigned) offset) > strlen(s) )) {
		result->type = LX_STRING;
		result->s = strnewp("");
		return TRUE;
	}

	if (length > (signed) strlen(s + offset)) {
		length = strlen(s) - offset;
	}

	if (length < 0) {
		length = strlen(s) - offset + length;
	}

	if (length <= 0) {
		result->type = LX_STRING;
		result->s = strnewp("");
		return TRUE;
	}

	result->type = LX_STRING;
	result->s = strnewp(s + offset);
	result->s[length] = '\0';
	return TRUE;
}
	
int Function::FunctionStrcmp(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	strcmp(any a, any b) returns int.

	The operands are converted to Strings by the ``string'' function above. The
	result is an Integer less than, equal to, or greater than zero according to
	whether a is lexicographically less than, equal to, or greater than b. Note
	that case is significant in the comparison.
	*/

	// NOTE: ALL ARGUMENTS HAVE BEEN CONVERTED INTO STRING TYPES
	// BEFORE THIS FUNCTION WAS INVOKED.
	
	if ( number_of_args != 2 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_INTEGER;
	result->i = strcmp( evaluated_args[0].s, evaluated_args[1].s);

	return TRUE;
}
	
int Function::FunctionStricmp(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stricmp(any a, any b) returns int.

	The same as strcmp except that upper and lower case letters are considered
	equivalent.
	*/

	// NOTE: ALL ARGUMENTS HAVE BEEN CONVERTED INTO STRING TYPES
	// BEFORE THIS FUNCTION WAS INVOKED.
	
	if ( number_of_args != 2 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_INTEGER;
	result->i = ::strcasecmp( evaluated_args[0].s, evaluated_args[1].s);

	return TRUE;
}
	
int Function::FunctionToUpper(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	toUpper(any s) returns string.

	The operand is converted to a String by the ``string'' function above. The
	result is a String that is identical to s except that all lowercase letters
	in s are converted to uppercase.
	*/

	// NOTE: ALL ARGUMENTS HAVE BEEN CONVERTED INTO STRING TYPES
	// BEFORE THIS FUNCTION WAS INVOKED.
	
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_STRING;
	result->s = strnewp( evaluated_args[0].s);
	char *p = result->s;
	while (*p) {
		*p = toupper(*p);
		p++;
	}

	return TRUE;
}
	
int Function::FunctionToLower(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	toLower(any s) returns string.

	The operand is converted to a String by the ``string'' function above. The
	result is a String that is identical to s except that all uppercase letters
	in s are converted to lowercase.
	*/
		
	// NOTE: ALL ARGUMENTS HAVE BEEN CONVERTED INTO STRING TYPES
	// BEFORE THIS FUNCTION WAS INVOKED.
	
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_STRING;
	result->s = strnewp( evaluated_args[0].s);
	char *p = result->s;
	while (*p) {
		*p = tolower(*p);
		p++;
	}

	return TRUE;
}
	
int Function::FunctionSize(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	size(string s) returns int.

    Returns the number of characters of the string s.
	*/
	 
	// NOTE: ALL ARGUMENTS HAVE BEEN CONVERTED INTO STRING TYPES
	// BEFORE THIS FUNCTION WAS INVOKED.
	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_INTEGER;
	result->i = strlen( evaluated_args[0].s);
	
	return TRUE;
}
	
int Function::FunctionStringlistSize(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stringlistSize(string l [, string d]) returns int.
    
	If any of the arguments is not of type String, the result is an error.
	Returns the number of elements in the string list l. The characters
	specified in optional argument d are treated as the delimiters for the
	string list.  If not specified, d defaults to " ," (space and comma
	characters). 
	*/

	char *d;

	if (( number_of_args == 0) || ( number_of_args > 2 )) {
		result->type = LX_ERROR;
		return FALSE;
	}

	if(evaluated_args[0].type != LX_STRING) {
		result->type = LX_ERROR;
		return FALSE;
	}

	if ( number_of_args == 2) {
		if(evaluated_args[1].type != LX_STRING) {
			result->type = LX_ERROR;
			return FALSE;
		}
		d = evaluated_args[1].s;
	} else {
		d = " ,";
	}

	StringList sl(evaluated_args[0].s, d);
	result->type = LX_INTEGER;
	result->i = sl.number();
	return TRUE;
}
	
static int StringListNumberIterator(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result,         // OUT: the result of calling the function
	void (*func)(double, double *),
	double *accumulator)
{
	char *d;

	if (( number_of_args == 0) || ( number_of_args > 2 )) {
		result->type = LX_ERROR;
		return FALSE;
	}

	if ( number_of_args == 2) {
		if (evaluated_args[1].type != LX_STRING) {
			result->type = LX_ERROR;
			return FALSE;
		}
		d = evaluated_args[1].s;
	} else {
		d = " ,";
	}

	if (evaluated_args[0].type != LX_STRING) {
		result->type = LX_ERROR;
		return FALSE;
	}
	char *s = evaluated_args[0].s;

	StringList sl(s, d);

	if (sl.number() == 0) {
		result->type = LX_UNDEFINED;
		return TRUE;
	}

	result->type = LX_INTEGER;

	sl.rewind();
	char *entry;
	while( (entry = sl.next())) {
		float temp;
		int r = sscanf(entry, "%f", &temp);
		if (r != 1) {
			result->type = LX_ERROR;
			return FALSE;
		}
		if (strspn(entry, "+-0123456789") != strlen(entry)) {
			result->type = LX_FLOAT;
		}
		func(temp, accumulator);
	}
	
	if (result->type == LX_INTEGER) {
			result->i = (int)*accumulator;
	} else {
			result->f = *accumulator;
	}

	return TRUE;
}

static void sum(double entry, double *accumulator) {
	*accumulator += entry;
}

int Function::FunctionStringlistSum(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stringlistSum(string l [, string d]) returns number.

	If any of the arguments is not of type String, the result is an error. If l
	is composed only of numbers, the result is the sum of the values, as a Real
	if any value is Real, and as an Integer otherwise. If the list is empty,
	the result is 0. In other cases, the result is ERROR.  The characters
	specified in optional argument d are treated as the delimiters for the
	string list.  If not specified, d defaults to " ," (space and comma
	characters). 
	*/

	double accumulator = 0.0;;
	int r = StringListNumberIterator(
		number_of_args, evaluated_args, result, sum, &accumulator);

	// zero length array sums to zero
	if (result->type == LX_UNDEFINED) {
		result->type = LX_INTEGER;
		result->i = 0;
	}
	return r;
}
	
static void avg(double entry, double *pair) {
	pair[0] += entry;
	pair[1]++;
}

int Function::FunctionStringlistAvg(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stringlistAvg(string l [, string d]) returns number.

	If any of the arguments is not of type String, the result is an error. If l
	is composed only of numbers, the result is the average of the values, as a
	Real. If the list is empty, the result is 0. In other cases, the result is
	ERROR.  The characters specified in optional argument d are treated as the
	delimiters for the string list.  If not specified, d defaults to " ,"
	(space and comma characters). 
	*/

	// array[0] is the sum of all the entries, array[1] is the number of entries
	double array[2];
	array[0] = 0.0;
	array[1] = 0.0;

	int r = StringListNumberIterator(
		number_of_args, evaluated_args, result, avg, array);

	if (!r) {
		return FALSE;
	}

	if (result->type == LX_UNDEFINED) {
		result->f = 0.0;
	} else {
		result->f    = array[0] / array[1];
	}
	result->type = LX_FLOAT;
	return TRUE;
}
	
static void minner(double entry, double *best) {
	if (entry < *best) {
		*best = entry;
	}
}

int Function::FunctionStringlistMin(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stringlistMin(string l [, string d]) returns number.

	If any of the arguments is not of type String, the result is an error. If l
	is composed only of numbers, the result is the minimum of the values, as a
	Real if any value is Real, and as an Integer otherwise. If the list is
	empty, the result is UNDEFINED. In other cases, the result is ERROR. The
	characters specified in optional argument d are treated as the delimiters
	for the string list.  If not specified, d defaults to " ," (space and comma
	characters). 
	*/

	double lowest = FLT_MAX;
	return StringListNumberIterator(
		number_of_args, evaluated_args, result, minner, &lowest);
}
	
static void maxer(double entry, double *best) {
	if (entry > *best) {
		*best = entry;
	}
}

int Function::FunctionStringlistMax(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stringlistMax(string l [, string d]) returns number.
 
	If any of the arguments is not of type String, the result is an error. If l
	is composed only of numbers, the result is the maximum of the values, as a
	Real if any value is Real, and as an Integer otherwise. If the list is
	empty, the result is UNDEFINED. In other cases, the result is ERROR.  The
	characters specified in optional argument d are treated as the delimiters
	for the string list.  If not specified, d defaults to " ," (space and comma
	characters). 
	*/

	double maxest = FLT_MIN;
	return StringListNumberIterator(
		number_of_args, evaluated_args, result, maxer, &maxest);
}
	
static int stringlistmember(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result,         // OUT: the result of calling the function
	bool ignore_case)
{
	if ((number_of_args < 2) || (number_of_args > 3)) {
		result->type = LX_ERROR;
		return FALSE;
	}

	if ((evaluated_args[0].type != LX_STRING) ||
		(evaluated_args[1].type != LX_STRING)) {
		result->type = LX_ERROR;
		return FALSE;
	}

	char *d = " ,";
	if (number_of_args == 3) {
		if (evaluated_args[2].type != LX_STRING) {
			result->type = LX_ERROR;
			return FALSE;
		}

		d = evaluated_args[2].s;
	}

	result->type = LX_INTEGER;
	StringList sl(evaluated_args[1].s, d);
	sl.rewind();
	char *entry;
	while( (entry = sl.next())) {
		if (ignore_case) {
			if (strcasecmp(entry, evaluated_args[0].s) == 0) {
				result->i = 1;
				return TRUE;
			}
		} else {
			if (strcmp(entry, evaluated_args[0].s) == 0) {
				result->i = 1;
				return TRUE;
			}
		}
	}

	result->i = 0;
	return TRUE;
}

int Function::FunctionStringlistMember(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stringlistMember(string x, string l [, string d]) returns boolean.

	If any of the arguments is not of type String, the result is an error. If
	any of the values in string list l are equal to x in the sense of the
	strcmp() function, then the result is true, otherwise it is false.  The
	characters specified in optional argument d are treated as the delimiters
	for the string list.  If not specified, d defaults to " ," (space and comma
	characters). 
	*/

	return stringlistmember(number_of_args, evaluated_args, result, false);
}
	
int Function::FunctionStringlistIMember(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stringlistIMember(string x, string l [, string d]) returns boolean.

	If any of the arguments is not of type String, the result is an error. If
	any of the values in string list l are equal to x in the sense of the
	stricmp() function, then the result is true, otherwise it is false.  The
	characters specified in optional argument d are treated as the delimiters
	for the string list.  If not specified, d defaults to " ," (space and comma
	characters).
	*/

	return stringlistmember(number_of_args, evaluated_args, result, true);
}
	
static int regexp_str_to_options(char *option_str) {
	int options = 0;
	while (*option_str) {
		switch (*option_str) {
			case 'i':
			case 'I':
				options |= Regex::caseless;
				break;
			case 'm':
			case 'M':
				options |= Regex::multiline;
				break;
			case 's':
			case 'S':
				options |= Regex::dotall;
				break;
			case 'x':
			case 'X':
				options |= Regex::extended;
				break;
			default:
				// Ignore for forward compatibility 
				break;
		}
		option_str++;
	}
	return options;
}

int Function::FunctionStringlistRegexpMember(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	stringlistRegexpMember(string pattern, string l 
			[, string d] [, string options]) returns boolean.

	If l is not a stringlist or any of the other arguments is not of type 
	String, the result is an error. If any of the values in string list do
	not eveluate to a string it returns an error. Otherwise, if any of the 
	values in the list matches the pattern accoring to the regexp function,
	the result is true. If there is no match the result is false.

	The characters in the optional argument d are treated as the delimiters
	for the stringlist. If this is not enumerated the delimiter defaults to
	" ," (space and comma characters). The options string behaves as in
	regexp function below.
	*/

	if (( number_of_args < 2) || ( number_of_args > 4 )) {
		result->type = LX_ERROR;
		return FALSE;
	}

	char *d = " ,";

	if (number_of_args == 3) {
		if (evaluated_args[2].type != LX_STRING) {
			result->type = LX_ERROR;
			return FALSE;
		}

		d = evaluated_args[2].s;
	}

	char *option_str = "";

	if (number_of_args == 4) {
		if (evaluated_args[3].type != LX_STRING) {
				result->type = LX_ERROR;
				return FALSE;
		}
		option_str = evaluated_args[3].s;
	}

	if ((evaluated_args[0].type != LX_STRING) || 
		(evaluated_args[1].type != LX_STRING)) {
		result->type = LX_ERROR;
		return FALSE;
	}

	Regex r;
	const char *errstr = 0;
	int errpos = 0;
	bool valid;
	int options = regexp_str_to_options(option_str);

	/* can the pattern be compiled */
	valid = r.compile(evaluated_args[0].s, &errstr, &errpos, options);
	if (!valid) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->type = LX_INTEGER;
	StringList sl(evaluated_args[1].s, d);
	sl.rewind();
	char *entry;
	int match = 0;
	while( (entry = sl.next())) {
			if (r.match(entry)) {
				match = 1;
			}
	}

	if(match) {
		result->i = 1;
		return TRUE;
	}

	result->i = 0;
	return TRUE;
}
	
int Function::FunctionRegexp(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	regexp(string pattern, string target , string output[, string options ]) returns boolean.

	If any of the arguments is not of type String or if pattern is not a valid
	regular expression, the result is an error. Otherwise, if pattern matches
	target, the result is string, otherwise error.  The resultant string is the
	string "output", with backticks references (e.g. \1 \2) replaced by the captured
	regexps inside the pattern.

	The details of the syntax and semantics of the regular expressions follows
	the perl-compatible regular expression format as supported by the PCRE
	library (see http://www.pcre.org/).   The options argument, if present, may
	contain the following characters to alter the exact details. Unrecognized
	options are silently ignored. 

    i or I

        Ignore case.

    m or M

        Multi-line: A carat (^) matches not only the start of the subject string, but also after each newline. Similarly, dollar ($) matches before a newline. 

    s or S

        Single-line: Dot (.) matches any character, including newline.

    x or X

        Extended: Whitespace and comments (from # to the next newline) in the pattern are ignored.
	*/

	if (( number_of_args < 2) || ( number_of_args > 3 )) {
		result->type = LX_ERROR;
		return FALSE;
	}

	char *option_str = "";

	if (number_of_args == 3) {
		if (evaluated_args[2].type != LX_STRING) {
				result->type = LX_ERROR;
				return FALSE;
		}
		option_str = evaluated_args[2].s;
	}

	if ((evaluated_args[0].type != LX_STRING) || (evaluated_args[1].type != LX_STRING)) {
		result->type = LX_ERROR;
		return FALSE;
	}

	Regex r;
	const char *errstr = 0;
	int errpos = 0;
	bool valid;
	int options = regexp_str_to_options(option_str);

	valid = r.compile(evaluated_args[0].s, &errstr, &errpos, options);
	if (!valid) {
		result->type = LX_ERROR;
		return FALSE;
	}

	result->i = r.match(evaluated_args[1].s);
	result->type = LX_INTEGER;

	return TRUE;
}

int Function::FunctionRegexps(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	regexps(string pattern, string target, string substitute [, string options ]) returns string.

	Like regexp except that it returns the string substitute as modified by
	matches from regexp.

	If any of the arguments is not of type String or if pattern is not a valid
	regular expression, the result is an error. Otherwise, if pattern matches
	target, the result is true, otherwise it is false.

	The details of the syntax and semantics of the regular expressions follows
	the perl-compatible regular expression format as supported by the PCRE
	library (see http://www.pcre.org/).   The options argument, if present, may
	contain the following characters to alter the exact details. Unrecognized
	options are silently ignored. 

    i or I

        Ignore case.

    m or M

        Multi-line: A carat (^) matches not only the start of the subject string, but also after each newline. Similarly, dollar ($) matches before a newline. 

    s or S

        Single-line: Dot (.) matches any character, including newline.

    x or X

        Extended: Whitespace and comments (from # to the next newline) in the pattern are ignored.
	*/

	if (( number_of_args < 3) || ( number_of_args > 4 )) {
		result->type = LX_ERROR;
		return FALSE;
	}

	char *option_str = "";

	if (number_of_args == 4) {
		if (evaluated_args[3].type != LX_STRING) {
				result->type = LX_ERROR;
				return FALSE;
		}
		option_str = evaluated_args[3].s;
	}

	if ((evaluated_args[0].type != LX_STRING) || 
		(evaluated_args[1].type != LX_STRING) ||
		(evaluated_args[2].type != LX_STRING)) {
		result->type = LX_ERROR;
		return FALSE;
	}

	Regex r;
	const char *errstr = 0;
	int errpos = 0;
	bool valid;
	int options = regexp_str_to_options(option_str);


	valid = r.compile(evaluated_args[0].s, &errstr, &errpos, options);
	if (!valid) {
		result->type = LX_ERROR;
		return FALSE;
	}

	ExtArray<MyString> a;
	MyString output;
	char *input = evaluated_args[2].s;

	if (!r.match(evaluated_args[1].s, &a)) {
		result->type = LX_STRING;
		result->s = strnewp("");
		return TRUE;
	}

	result->type = LX_STRING;
	while (*input) {
		if (*input == '\\') {
			if (isdigit(input[1])) {
				int offset = input[1] - '0';
				input++;
				output += a[offset];
			} else {
				output += '\\';
			}
		} else {
			output += *input;
		}
		input++;
	}
	result->s    = strnewp(output.Value());
	return TRUE;
}

int Function::FunctionFormatTime(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{

	/* 
	formatTime([int t], [string s]) returns string.

    This function creates a formatted string that is a representation of time t.

	The argument t is interpreted as an epoch time (seconds since 1/1/1970 GMT). 
    The argument s is interpreted similarly to the format argument of 
	the ANSI C strftime function. It consists of arbitary text plus placeholders 
	for elements of the time. These placeholders are percent signs (%) followed 
	by a single letter. To have a percent sign in your output, you must use a 
	double percent sign (%%).  
	If t is not specified, it defaults to the current time as reported by the OS.
	If s is not specified, it defaults to "%c".
	*/

	time_t epoch_time;		// parameter one
	char *format_string;	// parameter two		

	if ( number_of_args >  2 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

		// grab optional paramter 1, the time
	if ( number_of_args > 0 ) {
			// caller provided a time
		if( (evaluated_args[0].type != LX_INTEGER) ||
			(evaluated_args[0].i < 0) ) 
		{
			result->type = LX_ERROR;
			return FALSE;
		}
		epoch_time = evaluated_args[0].i;
	} else {
			// no time specified, default to current time		
		time(&epoch_time);	// yuck.
	}

		// grab optional paramter 2, the format string
	if( number_of_args == 2 ) {
			// caller provided a format string 
		if ( evaluated_args[1].type != LX_STRING ) {
			result->type = LX_ERROR;
			return FALSE;
		}
		format_string = evaluated_args[1].s;
	} else {
			// no format string specified, default to %c
		format_string = "%c";
	}

	struct tm * time_components = localtime(&epoch_time); // not thread safe

		// the crux of this function is just a  call to strftime().
	int ret = 0;
	char output[1024]; // yuck!!
	if ( time_components ) {
		ret = strftime(output,sizeof(output),format_string,time_components);
	}

	result->type = LX_STRING;

		// if strftime wrote something, use that as the result.
	if ( ret > 0 ) {
		result->s = strnewp(output);
	} else {
		result->s = strnewp("");
	}

	return TRUE;
}


int Function::FunctionEval(
	AttrList const *attrlist1,
	AttrList const *attrlist2,
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	/*
	  eval(string s) returns the result of the string s evaluated
	  as a ClassAd expression.
	*/

	if ( number_of_args != 1 ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	if( (evaluated_args[0].type != LX_STRING) ||
		(evaluated_args[0].i < 0) ) 
	{
		result->type = LX_ERROR;
		return FALSE;
	}

	char const *expr_str = evaluated_args[0].s;
	ExprTree *expr_tree;
	ParseClassAdRvalExpr(expr_str,expr_tree);
	if( !expr_tree ) {
		result->type = LX_ERROR;
		return FALSE;
	}

	int rc = expr_tree->EvalTree(attrlist1,attrlist2,result);

	delete expr_tree;
	return rc;
}


void dprintResult(ExprTree *tree, EvalResult *result) {
	if (result->debug) {
		char *s = NULL;
		tree->PrintToNewStr(&s);

		switch ( result->type ) {
		case LX_INTEGER :
			dprintf(D_ALWAYS, "Classad debug: %s --> %d\n", s, result->i);
			break;

		case LX_FLOAT :

			dprintf(D_ALWAYS, "Classad debug: %s --> %f\n", s, result->f);
			break;

		case LX_STRING :

			dprintf(D_ALWAYS, "Classad debug: %s --> %s\n", s, result->s);
			break;

		case LX_NULL :

			dprintf(D_ALWAYS, "Classad debug: %s --> NULL\n", s);
			break;

		case LX_UNDEFINED :

			dprintf(D_ALWAYS, "Classad debug: %s --> UNDEFINED\n", s);
			break;

		case LX_ERROR :

			dprintf(D_ALWAYS, "Classad debug: %s --> ERROR\n", s);
			break;

		default :

			dprintf(D_ALWAYS, "Classad debug: %s --> ???\n", s);
			break;
		}
		free(s);
	}
}
