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
//****************************************************************************//
// astbase.C 				                                      //
//  	 				                                      //
// Implementation of the base Abstract Syntax Tree (AST) module.              //
//                                                                            //
//****************************************************************************//

#include "condor_common.h"
#include "condor_exprtype.h"
#include "condor_astbase.h"
#include "condor_debug.h"

#define AdvancePtr(ptr)  while(*ptr != '\0') ptr++;

#ifdef USE_STRING_SPACE_IN_CLASSADS
StringSpace *ExprTree::string_space = NULL;
int ExprTree::string_space_references = 0;
#endif

////////////////////////////////////////////////////////////////////////////////
// Tree node constructors.                                                    //
////////////////////////////////////////////////////////////////////////////////

VariableBase::VariableBase(char* name)
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	this->stringSpaceIndex = string_space->getCanonical(name, SS_DUP);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	this->name = (char *) (*string_space)[stringSpaceIndex];
#else
    this->name = name;
#endif
    this->type = LX_VARIABLE;
}

IntegerBase::IntegerBase(int v)
{
    this->value = v;
    this->type  = LX_INTEGER;
}

FloatBase::FloatBase(float v)
{
    this->value = v;
    this->type  = LX_FLOAT;
}

BooleanBase::BooleanBase(int v)
{
    this->value = v;
    this->type  = LX_BOOL;
}

StringBase::StringBase(char* str)
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	stringSpaceIndex = string_space->getCanonical(str, SS_DUP);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	value = (char *) (*string_space)[stringSpaceIndex];
#else
    value = str;
#endif
    this->type  = LX_STRING;
}

ISOTimeBase::ISOTimeBase(char* str)
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	stringSpaceIndex = string_space->getCanonical(str, SS_DUP);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	time = (char *) (*string_space)[stringSpaceIndex];
#else
    time = str;
#endif
    this->type  = LX_TIME;
}

UndefinedBase::UndefinedBase()
{
    this->type  = LX_UNDEFINED;
}

ErrorBase::ErrorBase()
{
    this->type  = LX_ERROR;
}

AddOpBase::AddOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_ADD;
}

SubOpBase::SubOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_SUB;
}

MultOpBase::MultOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_MULT;
}

DivOpBase::DivOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_DIV;
}

MetaEqOpBase::MetaEqOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_META_EQ;
}

MetaNeqOpBase::MetaNeqOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_META_NEQ;
}

EqOpBase::EqOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_EQ;
}

NeqOpBase::NeqOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_NEQ;
}

GtOpBase::GtOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_GT;
}

GeOpBase::GeOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_GE;
}

LtOpBase::LtOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_LT;
}

LeOpBase::LeOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_LE;
}

AndOpBase::AndOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_AND;
}

OrOpBase::OrOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_OR;
}

AssignOpBase::AssignOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->type  = LX_ASSIGN;
}

////////////////////////////////////////////////////////////////////////////////
// Destructors
////////////////////////////////////////////////////////////////////////////////
VariableBase::~VariableBase()
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	string_space->disposeByIndex(stringSpaceIndex);
#else
	delete [] name;
#endif
}

StringBase::~StringBase()
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	string_space->disposeByIndex(stringSpaceIndex);
#else
	delete [] value;
#endif
}

ISOTimeBase::~ISOTimeBase()
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	string_space->disposeByIndex(stringSpaceIndex);
#else
	delete [] time;
#endif
}

BinaryOpBase::~BinaryOpBase()
{
	if (lArg != NULL) {
		delete lArg;
	}
	if (rArg != NULL) {
		delete rArg;
	}
	return;
}


////////////////////////////////////////////////////////////////////////////////
// Comparison operator                                                        //
////////////////////////////////////////////////////////////////////////////////

int ExprTree::operator ==(ExprTree& tree)
{
    if(tree.type != this->type)
    {
	return FALSE;
    }
    return TRUE;
}

int VariableBase::operator==(ExprTree& tree)
{
    if(tree.MyType() == LX_VARIABLE)
    {
        return !strcmp(this->name, ((VariableBase&)tree).name);
    }
    return FALSE;
}

int IntegerBase::operator ==(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
        return this->value == ((IntegerBase&)tree).value;
    }
    return FALSE;
}

int FloatBase::operator ==(ExprTree& tree)
{
    if(tree.MyType() == LX_FLOAT)
    {
        return this->value == ((FloatBase&)tree).value;
    }
    return FALSE;
}

int StringBase::operator ==(ExprTree& tree)
{
    if(tree.MyType() == LX_STRING)
    {
        return !strcmp(this->value, ((StringBase&)tree).value);
    }
    return FALSE;
}

int ISOTimeBase::operator ==(ExprTree& tree)
{
    if(tree.MyType() == LX_TIME)
    {
        return !strcmp(this->time, ((ISOTimeBase&)tree).time);
    }
    return FALSE;
}

int BooleanBase::operator ==(ExprTree& tree)
{
    if(tree.MyType() == LX_BOOL)
    {
        return this->value == ((BooleanBase&)tree).value;
    }
    return FALSE;
}

int BinaryOpBase::operator ==(ExprTree& tree)
{
	// HACK!!  No lArg implies user-directed parenthesization
	if ( this->lArg == NULL || ((BinaryOpBase&)tree).LArg() == NULL ) {
		return (this->lArg == ((BinaryOpBase&)tree).LArg()) &&
			   (*this->rArg == *((BinaryOpBase&)tree).RArg());
	} else if (tree.MyType() == this->MyType()) {
		return (*this->lArg == *((BinaryOpBase&)tree).LArg()) &&
			   (*this->rArg == *((BinaryOpBase&)tree).RArg());
	}
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// ">" operator.
////////////////////////////////////////////////////////////////////////////////

int ExprTree::operator >(ExprTree&)
{
    return FALSE;
}

int IntegerBase::operator >(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
	return value > ((IntegerBase*)&tree)->Value();
    }
    else if(tree.MyType() == LX_FLOAT)
    {
	return value > ((FloatBase*)&tree)->Value();
    }
    return FALSE;
}

int FloatBase::operator >(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
	return value > ((IntegerBase*)&tree)->Value();
    }
    else if(tree.MyType() == LX_FLOAT)
    {
	return value > ((FloatBase*)&tree)->Value();
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// ">=" operator.
////////////////////////////////////////////////////////////////////////////////

int ExprTree::operator >=(ExprTree&)
{
    return FALSE;
}

int IntegerBase::operator >=(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
	return value >= ((IntegerBase*)&tree)->Value();
    }
    else if(tree.MyType() == LX_FLOAT)
    {
	return value >= ((FloatBase*)&tree)->Value();
    }
    return FALSE;
}

int FloatBase::operator >=(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
	return value >= ((IntegerBase*)&tree)->Value();
    }
    else if(tree.MyType() == LX_FLOAT)
    {
	return value >= ((FloatBase*)&tree)->Value();
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// "<" operator.
////////////////////////////////////////////////////////////////////////////////

int ExprTree::operator <(ExprTree&)
{
    return FALSE;
}

int IntegerBase::operator <(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
	return value < ((IntegerBase*)&tree)->Value();
    }
    else if(tree.MyType() == LX_FLOAT)
    {
	return value < ((FloatBase*)&tree)->Value();
    }
    return FALSE;
}

int FloatBase::operator <(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
	return value < ((IntegerBase*)&tree)->Value();
    }
    else if(tree.MyType() == LX_FLOAT)
    {
	return value < ((FloatBase*)&tree)->Value();
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// "<=" operator.
////////////////////////////////////////////////////////////////////////////////

int ExprTree::operator <=(ExprTree&)
{
    return FALSE;
}

int IntegerBase::operator <=(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
	return value <= ((IntegerBase*)&tree)->Value();
    }
    else if(tree.MyType() == LX_FLOAT)
    {
	return value <= ((FloatBase*)&tree)->Value();
    }
    return FALSE;
}

int FloatBase::operator <=(ExprTree& tree)
{
    if(tree.MyType() == LX_INTEGER)
    {
	return value <= ((IntegerBase*)&tree)->Value();
    }
    else if(tree.MyType() == LX_FLOAT)
    {
	return value <= ((FloatBase*)&tree)->Value();
    }
    return FALSE;
}

// This is used by the various CopyDeep()s to get the variables from the base
// ExprTree class.
void ExprTree::CopyBaseExprTree(ExprTree * const recipient) const
{
	recipient->unit         = unit;
	recipient->type         = type;
	recipient->evalFlag     = evalFlag;
	return;
}

void ExprTree::PrintToNewStr(char **str)
{
	int   length;
	char  *new_str;

	// Before we allocate new space for this string, we determine how
	// much space is needed.
	length = CalcPrintToStr();
	new_str = (char *) malloc(length + 1); // +1 for termination
	*new_str = 0; // necessary, because PrintToStr begins at end of string

	// Now that we have a correctly sized string, fill it up.
	PrintToStr(new_str);

	// Sanity check--I've tested this quite thoroughly, so it shouldn't
	// except. It's a good idea to run test_classads though, should you 
	// ever change the code. Catch errors before shipping. 
	if (strlen(new_str) != (size_t) length) {
		EXCEPT("Bad length calculation in class ads. Expected %d, "
			   "got %d (\"%s\"",
			   length, strlen(new_str), new_str);
	}
	*str = new_str;

	return;
}

////////////////////////////////////////////////////////////////////////////////
// Display an expression tree.
////////////////////////////////////////////////////////////////////////////////

void ExprTree::Display()
{
    dprintf (D_NOHEADER | D_ALWAYS, "Unknown type");
}

void VariableBase::Display()
{
    dprintf (D_NOHEADER | D_ALWAYS, "%s", name);
}

void IntegerBase::Display()
{
    dprintf(D_NOHEADER | D_ALWAYS, "%d", value);
    if(unit == 'k')
    {
	dprintf(D_NOHEADER | D_ALWAYS, " k");
    }
}

void FloatBase::Display()
{
    dprintf(D_NOHEADER | D_ALWAYS, "%f", value);
    if(unit == 'k')
    {
	dprintf(D_NOHEADER | D_ALWAYS, " k");
    }
}

void StringBase::Display()
{
    dprintf (D_NOHEADER | D_ALWAYS, "\"%s\"", value);
}

void ISOTimeBase::Display()
{
    dprintf (D_NOHEADER | D_ALWAYS, "\"%s\"", time);
}

void BooleanBase::Display()
{
    dprintf(D_NOHEADER | D_ALWAYS, (char *)(value ? "TRUE" : "FALSE") );
}


void UndefinedBase::Display()
{
	dprintf(D_NOHEADER | D_ALWAYS, "UNDEFINED");
}

void ErrorBase::Display()
{
	dprintf(D_NOHEADER | D_ALWAYS, "ERROR");
}

void AddOpBase::Display()
{
	if( !lArg ) {
		dprintf( D_NOHEADER | D_ALWAYS, "(" );
		rArg->Display( );
		dprintf( D_NOHEADER | D_ALWAYS, ")" );
	}
		
	lArg->Display();

    dprintf(D_NOHEADER | D_ALWAYS, " + ");
    if(rArg)
    {
	rArg->Display();
    }
    if(unit == 'k')
    {
	dprintf(D_NOHEADER | D_ALWAYS, " k");
    }
}

void SubOpBase::Display()
{
    if(lArg)
    {
	lArg->Display();
    }
    dprintf(D_NOHEADER | D_ALWAYS, " - ");
    if(rArg && (rArg->MyType() == LX_ADD || rArg->MyType() == LX_SUB))
    {
	dprintf(D_NOHEADER | D_ALWAYS, "(");
	rArg->Display();
	dprintf(D_NOHEADER | D_ALWAYS, ")");
    } else
    {
	rArg->Display();
    }
    if(unit == 'k')
    {
	dprintf(D_NOHEADER | D_ALWAYS, " k");
    }
}

void MultOpBase::Display()
{
    if(lArg && (lArg->MyType() == LX_ADD || lArg->MyType() == LX_SUB))
    {
	dprintf(D_NOHEADER | D_ALWAYS, "(");
	lArg->Display();
	dprintf(D_NOHEADER | D_ALWAYS, ")");
    } else
    {
	lArg->Display();
    }
    dprintf(D_NOHEADER | D_ALWAYS, " * ");
    if(rArg && (rArg->MyType() == LX_ADD || rArg->MyType() == LX_SUB))
    {
	dprintf(D_NOHEADER | D_ALWAYS, "(");
	rArg->Display();
	dprintf(D_NOHEADER | D_ALWAYS, ")");
    } else
    {
	rArg->Display();
    }
    if(unit == 'k')
    {
	dprintf(D_NOHEADER | D_ALWAYS, " k");
    }
}

void DivOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	   {
	     case LX_ADD :
	     case LX_SUB :
	     case LX_DIV : dprintf(D_NOHEADER | D_ALWAYS, "(");
			   lArg->Display();
			   dprintf(D_NOHEADER | D_ALWAYS, ")");
			   break;
	     default     : lArg->Display();
	   }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " / ");
    if(rArg)
    {
	switch(rArg->MyType())
	   {
	     case LX_ADD :
	     case LX_SUB :
	     case LX_MULT:
	     case LX_DIV : dprintf(D_NOHEADER | D_ALWAYS, "(");
			   rArg->Display();
			   dprintf(D_NOHEADER | D_ALWAYS, ")");
			   break;
	     default     : rArg->Display();
	   }
    }
    if(unit == 'k')
    {
        dprintf(D_NOHEADER | D_ALWAYS, " k");
    }
}

void MetaEqOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			lArg->Display();
  			dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      lArg->Display();
        }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " =?= ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			rArg->Display();
  		        dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      rArg->Display();
	}   
    }
}

void MetaNeqOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			lArg->Display();
  			dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      lArg->Display();
        }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " =!= ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			rArg->Display();
  		        dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      rArg->Display();
	}   
    }
}

void EqOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			lArg->Display();
  			dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      lArg->Display();
        }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " == ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			rArg->Display();
  		        dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      rArg->Display();
	}   
    }
}

void NeqOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			lArg->Display();
  			dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      lArg->Display();
        }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " != ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			rArg->Display();
  		        dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      rArg->Display();
	}   
    }
}


void GtOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			lArg->Display();
  			dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      lArg->Display();
        }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " > ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			rArg->Display();
  		        dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      rArg->Display();
	}   
    }
}


void GeOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			lArg->Display();
  			dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      lArg->Display();
        }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " >= ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			rArg->Display();
  		        dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      rArg->Display();
	}   
    }
}


void LtOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			lArg->Display();
  			dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      lArg->Display();
        }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " < ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			rArg->Display();
  		        dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      rArg->Display();
	}   
    }
}


void LeOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			lArg->Display();
  			dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      lArg->Display();
        }
    }
    dprintf(D_NOHEADER | D_ALWAYS, " <= ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	  case LX_EQ	:
	  case LX_NEQ :
	  case LX_GT  :
	  case LX_GE  :
	  case LX_LT  :
	  case LX_LE  : dprintf(D_NOHEADER | D_ALWAYS, "(");
			rArg->Display();
  		        dprintf(D_NOHEADER | D_ALWAYS, ")");
		        break;
	  default:      rArg->Display();
	}   
    }
}

void AndOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	    case LX_ADD :
	    case LX_SUB :
	    case LX_MULT :
	    case LX_DIV :
	    case LX_EQ :
	    case LX_NEQ :
	    case LX_GT :
	    case LX_GE :
	    case LX_LT :
	    case LX_LE :
	    case LX_OR : dprintf(D_NOHEADER | D_ALWAYS, "(");
	                 lArg->Display();
	                 dprintf(D_NOHEADER | D_ALWAYS, ")");
			 break;
	    default :    lArg->Display();
			 break;
	}
    }
    
    dprintf(D_NOHEADER | D_ALWAYS, " && ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	    case LX_ADD :
	    case LX_SUB :
	    case LX_MULT :
	    case LX_DIV :
	    case LX_EQ :
	    case LX_NEQ :
	    case LX_GT :
	    case LX_GE :
	    case LX_LT :
	    case LX_LE :
	    case LX_OR : dprintf(D_NOHEADER | D_ALWAYS, "(");
	                 rArg->Display();
	                 dprintf(D_NOHEADER | D_ALWAYS, ")");
			 break;
	    default :    rArg->Display();
			 break;
	}
    }
}

void OrOpBase::Display()
{
    if(lArg)
    {
	switch(lArg->MyType())
	{
	    case LX_ADD :
	    case LX_SUB :
	    case LX_MULT :
	    case LX_DIV :
	    case LX_EQ :
	    case LX_NEQ :
	    case LX_GT :
	    case LX_GE :
	    case LX_LT :
	    case LX_LE :
	    case LX_AND : dprintf(D_NOHEADER | D_ALWAYS, "(");
	                  lArg->Display();
	                  dprintf(D_NOHEADER | D_ALWAYS, ")");
			  break;
	    default :     lArg->Display();
			  break;
	}
    }
    dprintf(D_NOHEADER | D_ALWAYS, " || ");
    if(rArg)
    {
	switch(rArg->MyType())
	{
	    case LX_ADD :
	    case LX_SUB :
	    case LX_MULT :
	    case LX_DIV :
	    case LX_EQ :
	    case LX_NEQ :
	    case LX_GT :
	    case LX_GE :
	    case LX_LT :
	    case LX_LE :
	    case LX_AND : dprintf(D_NOHEADER | D_ALWAYS, "(");
	                  rArg->Display();
	                  dprintf(D_NOHEADER | D_ALWAYS, ")");
		  	  break;
	    default :     rArg->Display();
			  break;
	}
    }
}

void AssignOpBase::Display()
{
    if(lArg)
    {
	lArg->Display();
    }
    dprintf(D_NOHEADER | D_ALWAYS, " = ");
    if(rArg)
    {
	rArg->Display();
    }
}

int IntegerBase::Value()
{
	return value;
}

float FloatBase::Value()
{
	return value;
}

char * const StringBase::Value()
{
	return value;
}

char * const ISOTimeBase::Value()
{
	return time;
}

int BooleanBase::Value()
{
	return value;
}

FunctionBase::FunctionBase(char *name)
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	this->stringSpaceIndex = string_space->getCanonical(name, SS_DUP);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	this->name = (char *) (*string_space)[stringSpaceIndex];
#else
    this->name = name;
#endif
    this->type = LX_FUNCTION;
	return;
}

FunctionBase::~FunctionBase()
{
	return;
}

int
FunctionBase::operator==(ExprTree& tree)
{
	return 0;
}

void FunctionBase::GetReferences(const AttrList *base_attrlist,
								 StringList &internal_references,
								 StringList &external_references) const
{
	return;
}

void
FunctionBase::AppendArgument(ExprTree *argument)
{
	arguments.Append(argument);
	return;
}


