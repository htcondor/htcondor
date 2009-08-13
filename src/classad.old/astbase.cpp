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
#include "MyString.h"
#include "string_list.h"

#define AdvancePtr(ptr)  while(*ptr != '\0') ptr++;

#include "stringSpace.h"
StringSpace *ExprTree::string_space = NULL;
int ExprTree::string_space_references = 0;

////////////////////////////////////////////////////////////////////////////////
// Tree node constructors.                                                    //
////////////////////////////////////////////////////////////////////////////////

VariableBase::VariableBase(char* varName)
{
	this->stringSpaceIndex = string_space->getCanonical((const char *&)varName);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	this->name = (char *) (*string_space)[stringSpaceIndex];
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
	stringSpaceIndex = string_space->getCanonical((const char *&)str);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	value = (char *) (*string_space)[stringSpaceIndex];
    this->type  = LX_STRING;
}

ISOTimeBase::ISOTimeBase(char* str)
{
	stringSpaceIndex = string_space->getCanonical((const char *&)str);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	time = (char *) (*string_space)[stringSpaceIndex];
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
	string_space->disposeByIndex(stringSpaceIndex);
}

StringBase::~StringBase()
{
	string_space->disposeByIndex(stringSpaceIndex);
}

ISOTimeBase::~ISOTimeBase()
{
	string_space->disposeByIndex(stringSpaceIndex);
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
	recipient->invisible    = invisible;
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

void ExprTree::PrintToStr(MyString & str) {
	char * tmp = 0;
	PrintToNewStr(&tmp);
	str = tmp;
	free(tmp);
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

const char * StringBase::Value()
{
	return value;
}

const char * ISOTimeBase::Value()
{
	return time;
}

int BooleanBase::Value()
{
	return value;
}

FunctionBase::FunctionBase(char *tName)
{
	this->stringSpaceIndex = string_space->getCanonical((const char *&)tName);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	this->name = (char *) (*string_space)[stringSpaceIndex];
    this->type = LX_FUNCTION;
	arguments = new List<ExprTree>();
	return;
}

FunctionBase::~FunctionBase()
{
	ExprTree *arg = NULL;

	arguments->Rewind();
	while(arguments->Next(arg)) {
		if(arg) {
			delete arg;
			arg = NULL;
		}
	}
	delete arguments;

	string_space->disposeByIndex(stringSpaceIndex);

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

	ExprTree *arg;

    // These two lines make me shudder. 
    // This function is const, but calling Rewind() and Next() 
    // on the arguments list makes it non-const. In practice, 
    // though, it's really const: we always rewind before we 
    // look at the arguments. The problem is that the iterator
    // is part of the List class itself. In order to maintain
    // const-ness of this function, I'm going to take the easy
    // way out and magically turn it mutable, by hiding the 
    // fact that it should be const. 
    FunctionBase *mutable_this = (FunctionBase *) this;
	mutable_this->arguments->Rewind();

	while (mutable_this->arguments->Next(arg)) {
        arg->GetReferences(base_attrlist, 
                           internal_references, external_references);
	}

	return;
}

void
FunctionBase::AppendArgument(ExprTree *argument)
{
	arguments->Append(argument);
	return;
}

void
FunctionBase::EvaluateArgument(
	ExprTree *arg,
	const AttrList *attrlist1, 
	const AttrList *attrlist2,
	EvalResult *result) const
{
	if ( arg ) {
		if (attrlist2 == NULL) {
			// This will let us refer to attributes in a ClassAd, like "MY"
			arg->EvalTree(attrlist1, result);
		} else {
			// This will let us refer to attributes in two ClassAds: like 
			// "My" and "Target"
			arg->EvalTree(attrlist1, attrlist2, result);
		}
	}
	return;
}

// I added a contructor for this base class to initialize
// unit to something.  later we check to see if unit is "k"
// and without the contructor, we're doing that from unitialized
// memory which (belive it!) on HPUX happened to be 'k', and
// really bad things happened!  Yes, this bug was a pain 
// in the #*&@! to find!  -Todd, 7/97
// and now init sumFlag as well... not sure if it should be
// FALSE or TRUE! but it needs to be initialized -Todd, 9/10
// and now init evalFlag as well (to detect circular eval'n) -Rajesh
// We no longer initialze sumFlag, because it has been removed. 
// It's not used, that's why you couldn't figure out how to initialize
// it, Todd.-Alain 26-Sep-2001
ExprTree::ExprTree() : unit('\0'), evalFlag(FALSE)
{
	if (string_space_references == 0) {
		string_space = new StringSpace;
	}
	string_space_references++;
	invisible = false;
	return;
}

ExprTree::~ExprTree()
{
	string_space_references--;
	if (string_space_references == 0) {
		delete string_space;
		string_space = NULL;
	}
	return;
}

