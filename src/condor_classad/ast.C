/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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

extern void evalFromEnvironment (const char *, EvalResult *);
static bool name_in_list(const char *name, StringList &references);
static void printComparisonOpToStr (char *, ExprTree *, ExprTree *, char *);
static int calculate_math_op_length(ExprTree *lArg, ExprTree *rArg, int op_length);

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

int ExprTree::EvalTree(AttrList* l, EvalResult* r)
{
	return EvalTree(l, NULL, r);
}

int ExprTree::EvalTree(AttrList* l1, AttrList* l2, EvalResult* r)
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

int Variable::_EvalTree(AttrList* classad, EvalResult* val)
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

int Variable::_EvalTree( AttrList* my_classad, AttrList* target_classad, EvalResult* val)
{
	return _EvalTreeRecursive( name, my_classad, target_classad, val );
}

/*
Split a variable name into scope.target 
(For compatibility, also accept scope__target.)
If there is no scope, evaluate it simply.
Otherwise, identify the ClassAd corresponding to the scope, and re-evaluate.
*/

int Variable::_EvalTreeRecursive( char *name, AttrList* my_classad, AttrList* target_classad, EvalResult* val)
{
	ClassAd *other_classad;
	char prefix[ATTRLIST_MAX_EXPRESSION];
	char rest[ATTRLIST_MAX_EXPRESSION];
	int fields;
	int result;

	if( !val || !name ) return FALSE;

	fields = sscanf(name,"%[^.].%s",prefix,rest);
	if(fields!=2) {
		fields = sscanf(name,"%[^_]__%s",prefix,rest);
	}

	if(fields==2) {
		/* already split */
	} else {
		prefix[0] = 0;
		strcpy(rest,name);
	}

	if(prefix[0]) {	
		if(!strcmp(prefix,"MY") ) {
			return _EvalTreeRecursive(rest,my_classad,target_classad,val);
		} else if(!strcmp(prefix,"TARGET")) {
			return _EvalTreeRecursive(rest,target_classad,my_classad,val);
		} else {
			ExprTree *expr;
			char expr_string[ATTRLIST_MAX_EXPRESSION];
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
	} else {
		return this->_EvalTreeSimple(rest,my_classad,target_classad,val);
	}

	val->type = LX_UNDEFINED;	
	return TRUE;
}

/*
Once it has been reduced to a simple name, resolve the variable by
looking it up first in MY, then TARGET, and finally, the environment.
*/

int Variable::_EvalTreeSimple( char *name, AttrList *my_classad, AttrList *target_classad, EvalResult *val )
{
	ExprTree *tmp;

	if(my_classad)
	{
		tmp = my_classad->Lookup(name);
		if(tmp) return (tmp->EvalTree(my_classad, target_classad, val));
	}

	if(target_classad)
	{
		tmp = target_classad->Lookup(name);
		if(tmp) return (tmp->EvalTree(target_classad, my_classad, val));
	}

	evalFromEnvironment(name,val);
	return TRUE;
}

int Integer::_EvalTree(AttrList*, EvalResult* val)
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
int Integer::_EvalTree(AttrList*, AttrList*, EvalResult* val)
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
int Float::_EvalTree(AttrList*, EvalResult* val)
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
int Float::_EvalTree(AttrList*, AttrList*, EvalResult* val)
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
int String::_EvalTree(AttrList*, EvalResult* val)
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

int ISOTime::_EvalTree(AttrList*, EvalResult* val)
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

int String::_EvalTree(AttrList*, AttrList*, EvalResult* val)
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

int ISOTime::_EvalTree(AttrList*, AttrList*, EvalResult* val)
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
int Boolean::_EvalTree(AttrList*, EvalResult* val)
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

int Boolean::_EvalTree(AttrList*, AttrList*, EvalResult* val)
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


int Undefined::_EvalTree(AttrList*, EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_UNDEFINED;
    return TRUE;
}

//------------tw-------------------
int Undefined::_EvalTree(AttrList*, AttrList*,  EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_UNDEFINED;
    return TRUE;
}
//--------------------------------

int Error::_EvalTree(AttrList*, EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_ERROR;
    return TRUE;
}

//------------tw-------------------
int Error::_EvalTree(AttrList*, AttrList*,  EvalResult* val)
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
	bool is_in_list;
	const char *reference;

	is_in_list = false;
	references.rewind(); 
	while ((reference = references.next()) != NULL) {
		if (!strcmp(name, reference)) {
			is_in_list = true;
			break;
		}
	}

	return is_in_list;
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
	for (p = value; *p != 0; p++) {
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

int Boolean::CalcPrintToStr(void)
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
  sprintf(str, "%s%d", str, value);
  if(unit == 'k')
	strcat(str, " k");
}

void Float::PrintToStr(char* str)
{
  sprintf(str, "%s%f", str, value);
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
  while(*ptr1 != '\0')
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

void Boolean::PrintToStr(char* str)
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
Boolean::DeepCopy(void) const
{
	Boolean *copy;

	copy = new Boolean(value);
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
