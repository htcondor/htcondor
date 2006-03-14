/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
#include "condor_classad_lookup.h"
#include "condor_string.h"

extern void evalFromEnvironment (const char *, EvalResult *);
static bool name_in_list(const char *name, StringList &references);
static void printComparisonOpToStr (char *, ExprTree *, ExprTree *, char *);
static int calculate_math_op_length(ExprTree *lArg, ExprTree *rArg, int op_length);

bool classad_debug_function_run = 0;

#define EatSpace(ptr)  while(*ptr != '\0') ptr++;

void EvalResult::fPrintResult(FILE *f)
{
    switch(type)
    {
	case LX_INTEGER :

	     fprintf(f, "%d", this->i);
	     break;

	case LX_FLOAT :

	     fprintf(f, "%f", this->f);
	     break;

	case LX_STRING :

	     fprintf(f, "%s", this->s);
	     break;

	case LX_NULL :

	     fprintf(f, "NULL");
	     break;

	case LX_UNDEFINED :

	     fprintf(f, "UNDEFINED");
	     break;

	case LX_ERROR :

	     fprintf(f, "ERROR");
	     break;

	default :

	     fprintf(f, "type unknown");
	     break;
    }
    fprintf(f, "\n");
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
    
    if(!(tmp = classad->Lookup(name)))
    {
	val->type = LX_UNDEFINED;
        return TRUE;
    }

    return tmp->EvalTree(classad, val);
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

int Variable::_EvalTreeRecursive( char *name, const AttrList* my_classad, const AttrList* target_classad, EvalResult* val, bool restrict_search)
{
	char prefix[ATTRLIST_MAX_EXPRESSION];
	char rest[ATTRLIST_MAX_EXPRESSION];
	int fields;

	if( !val || !name ) return FALSE;

	memset(prefix,0,sizeof(prefix));
	memset(rest,0,sizeof(prefix));

	fields = sscanf(name,"%[^.].%s",prefix,rest);

	if(fields==2) {
		/* already split */
	} else {
		prefix[0] = 0;
		strcpy(rest,name);
	}

	if(prefix[0]) {	
        // Note that we use restrict_search=true instead of simply 
        // passing NULL for the other ClassAd. This is because we might
        // still need to refer to the other ClassAd. For example, evaluating 
        // A in ClassAd 1 should give 3
        // ClassAd 1: A = TARGET.B; C = 3
        // ClassAd 2: B = TARGET.C
		if(!strcasecmp(prefix,"MY") ) {
			return _EvalTreeRecursive(rest,my_classad,target_classad,val, true);
		} else if(!strcasecmp(prefix,"TARGET")) {
			return _EvalTreeRecursive(rest,target_classad,my_classad,val, true);
        }
        /*
         * This code has been deprecated. 
         * It was written by Doug Thain for research relating to his
         * paper titled "Gathering at the Well: Creating Communities for 
         * Grid I/O". It hasn't been used since, and the fact that this 
         * causes ClassAds to need to talk to Daemons is causing linking
         * problems for libcondorapi.a, so we're just ditching it. 
		} else {
			ExprTree *expr;
			char expr_string[ATTRLIST_MAX_EXPRESSION];
			if (target_classad) {
				expr = target_classad->Lookup(prefix);
				if(expr) {
					expr_string[0] = 0;
					expr->RArg()->PrintToStr(expr_string);
					other_classad = ClassAdLookupGlobal(expr_string);
					if(other_classad) {
						result = _EvalTreeRecursive(rest,other_classad,other_classad,val);
						delete other_classad;
						return result;
					}
				}
			}
		}
        */
	} else {
		return this->_EvalTreeSimple(rest,my_classad,target_classad,val, restrict_search);
	}

	val->type = LX_UNDEFINED;	
	return TRUE;
}

/*
Once it has been reduced to a simple name, resolve the variable by
looking it up first in MY, then TARGET, and finally, the environment.
*/

int Variable::_EvalTreeSimple( char *name, const AttrList *my_classad, const AttrList *target_classad, EvalResult *val, bool restrict_search )
{
	ExprTree *tmp;

	if(my_classad)
	{
		tmp = my_classad->Lookup(name);
		if(tmp) return (tmp->EvalTree(my_classad, target_classad, val));
	}

	if(!restrict_search && target_classad)
	{
		tmp = target_classad->Lookup(name);
		if(tmp) return (tmp->EvalTree(target_classad, my_classad, val));
	}

    evalFromEnvironment(name,val);
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

void ExprTree::GetReferences(const AttrList *base_attlrist,
							 StringList &internal_references,
							 StringList &external_references) const
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

#ifdef USE_STRING_SPACE_IN_CLASSADS
	copy = new Variable(name);
#else
	char     *name_copy;
	name_copy = new char[strlen(name)+1];
	strcpy(name_copy, name);
	copy = new Variable(name_copy);
#endif
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

#ifdef USE_STRING_SPACE_IN_CLASSADS
	copy = new String(value);
#else
	char   *value_copy;
	value_copy = new char[strlen(value)+1];
	strcpy(value_copy, value);
	copy = new String(value_copy);
#endif
	CopyBaseExprTree(copy);
	
	return copy;
}

ExprTree*  
ISOTime::DeepCopy(void) const
{
	ISOTime *copy;

#ifdef USE_STRING_SPACE_IN_CLASSADS
	copy = new ISOTime(time);
#else
	char   *time_copy;
	time_copy = new char[strlen(time)+1];
	strcpy(time_copy, time);
	copy = new ISOTime(time_copy);
#endif
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
	ExprTree  *copy_of_larg;
	ExprTree  *copy_of_rarg;

	copy_of_larg = lArg->DeepCopy();
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
#ifdef CLASSAD_FUNCTIONS
#include "dlfcn.h"
#include "classad_shared.h"
#endif

int Function::CalcPrintToStr(void)
{
	int      length;
	int      i;
	int      num_args;
	ExprTree *arg;

	length = 0;
	length += strlen(name);
	length += 1; // for left paren

	arguments.Rewind();
	i = 0;
	num_args = arguments.Length();
	while (arguments.Next(arg)) {
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

	arguments.Rewind();
	i = 0;
	num_args = arguments.Length();
	strcat(s, name);
	strcat(s, "(");
	while (arguments.Next(arg)) {
		arg->PrintToStr(s);
		i++;
		if (i < num_args) {
			strcat(s, "; ");
		}
	}
	strcat(s, ")");

	return;
}

ExprTree *Function::DeepCopy(void) const
{
	Function *copy;

#ifdef USE_STRING_SPACE_IN_CLASSADS
	copy = new Function(name);
#else
	char     *name_copy;
	name_copy = strnewp(name);
#endif
	CopyBaseExprTree(copy);

	ListIterator< ExprTree > iter(arguments);
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

	successful_eval = FALSE;
	result->type = LX_UNDEFINED;

	if (result != NULL) {
		number_of_args = arguments.Length();
		evaluated_args = new EvalResult[number_of_args];
		
		ListIterator<ExprTree> iter(arguments);
		ExprTree *arg;

		i = 0;
		while (iter.Next(arg)) {
			if (attrlist2 == NULL) {
				// This will let us refer to attributes in a ClassAd, like "MY"
				arg->EvalTree(attrlist1, &evaluated_args[i++]);
			} else {
				// This will let us refer to attributes in two ClassAds: like 
				// "My" and "Target"
				arg->EvalTree(attrlist1, attrlist2, &evaluated_args[i++]);
			}
		}
		
        if (!strcasecmp(name, "gettime")) {
			successful_eval = FunctionGetTime(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "random")) {
			successful_eval = FunctionRandom(number_of_args, evaluated_args, result);
		} else if (!strcasecmp(name, "_debug_function_")) {
            successful_eval = FunctionClassadDebugFunction(number_of_args, evaluated_args, result);
        }
#ifdef CLASSAD_FUNCTIONS
        else {
			successful_eval = FunctionSharedLibrary(number_of_args, 
													evaluated_args, result);
		}
#else
        else {
            successful_eval = false;
        }
#endif
		delete [] evaluated_args;
	}

	return successful_eval;
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

#ifdef CLASSAD_FUNCTIONS
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
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
	time_t current_time = time(NULL);

	result->i = (int) current_time;
	result->type = LX_INTEGER;
	return TRUE;
}

extern "C" int get_random_int();

int Function::FunctionRandom(
	int number_of_args,         // IN:  size of evaluated args array
	EvalResult *evaluated_args, // IN:  the arguments to the function
	EvalResult *result)         // OUT: the result of calling the function
{
    int   random_number;
    bool  success;

    if (number_of_args == 0) {
        // If we get no arguments, we return a random number between 0 and MAXINT
        random_number = get_random_int();
        success = true;
    } else if (number_of_args == 1) {
        // If we get one integer argument, we return a random number
        // between 0 and that number
        if (evaluated_args[0].type == LX_INTEGER) {
            random_number = get_random_int() % evaluated_args[0].i;
            success = true;
        } else if (evaluated_args[0].type == LX_FLOAT) {
            random_number = get_random_int() % (int) evaluated_args[0].f;
            success = true;
        } else {
            success = false;
        }
    } else {
        success = false;
    }

    if (success) {
        result->i    = random_number;
        result->type = LX_INTEGER;
    } else {
        result->type = LX_ERROR;
    }
	return success;
}

int Function::FunctionClassadDebugFunction(
	int number_of_args,         
	EvalResult *evaluated_args, 
	EvalResult *result)         
{
    classad_debug_function_run = true;
    result->i    = 1;
    result->type = LX_INTEGER;

	return TRUE;
}

