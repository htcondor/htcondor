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
#include "stringSpace.h"

StringSpace classad_string_space;
#endif 

////////////////////////////////////////////////////////////////////////////////
// Tree node constructors.                                                    //
////////////////////////////////////////////////////////////////////////////////

VariableBase::VariableBase(char* name)
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	this->stringSpaceIndex = classad_string_space.getCanonical(name, SS_DUP);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	this->name = (char *) classad_string_space[stringSpaceIndex];
#else
    this->name = name;
#endif
    this->ref = 0;
    this->type = LX_VARIABLE;
    this->cardinality = 0;
}

IntegerBase::IntegerBase(int v)
{
    this->value = v;
    this->ref   = 0;
    this->type  = LX_INTEGER;
    this->cardinality = 0;
}

FloatBase::FloatBase(float v)
{
    this->value = v;
    this->ref   = 0;
    this->type  = LX_FLOAT;
    this->cardinality = 0;
}

BooleanBase::BooleanBase(int v)
{
    this->value = v;
    this->ref   = 0;
    this->type  = LX_BOOL;
    this->cardinality = 0;
}

StringBase::StringBase(char* str)
{
#ifdef USE_STRING_SPACE_IN_CLASSADS
	stringSpaceIndex = classad_string_space.getCanonical(str, SS_DUP);
	// I apologize for casting away the const-ness of the char * here
	// I'm trying to make minimal changes in the code to add string space,
	// and it is safe. 
	value = (char *) classad_string_space[stringSpaceIndex];
#else
    value = str;
#endif
    this->ref = 0;
    this->type  = LX_STRING;
    this->cardinality = 0;
}

UndefinedBase::UndefinedBase()
{
    this->ref = 0;
    this->type  = LX_UNDEFINED;
    this->cardinality = 0;
}

ErrorBase::ErrorBase()
{
    this->ref = 0;
    this->type  = LX_ERROR;
    this->cardinality = 0;
}

AddOpBase::AddOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_ADD;
    this->cardinality = 2;
}

SubOpBase::SubOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_SUB;
    this->cardinality = 2;
}

MultOpBase::MultOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_MULT;
    this->cardinality = 2;
}

DivOpBase::DivOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_DIV;
    this->cardinality = 2;
}

MetaEqOpBase::MetaEqOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_META_EQ;
    this->cardinality = 2;
}

MetaNeqOpBase::MetaNeqOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_META_NEQ;
    this->cardinality = 2;
}

EqOpBase::EqOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_EQ;
    this->cardinality = 2;
}

NeqOpBase::NeqOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_NEQ;
    this->cardinality = 2;
}

GtOpBase::GtOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_GT;
    this->cardinality = 2;
}

GeOpBase::GeOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_GE;
    this->cardinality = 2;
}

LtOpBase::LtOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_LT;
    this->cardinality = 2;
}

LeOpBase::LeOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_LE;
    this->cardinality = 2;
}

AndOpBase::AndOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_AND;
    this->cardinality = 2;
}

OrOpBase::OrOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_OR;
    this->cardinality = 2;
}

AssignOpBase::AssignOpBase(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type  = LX_ASSIGN;
    this->cardinality = 2;
}

////////////////////////////////////////////////////////////////////////////////
// A dummy class used by the delete operator to avoid recursion               //
////////////////////////////////////////////////////////////////////////////////
class Dummy
{
};

////////////////////////////////////////////////////////////////////////////////
// Delete operator member functions                                           //
////////////////////////////////////////////////////////////////////////////////

void ExprTree::operator delete(void* exprTree)
{
    if(exprTree)
    {
	if(((ExprTree*)exprTree)->cardinality > 0)
	// this is a binary operator, delete the children
	{
	    if(((BinaryOpBase*)exprTree)->lArg)
	    {
	        delete ((BinaryOpBase*)exprTree)->lArg;
	    }
	    if(((BinaryOpBase*)exprTree)->rArg)
	    {
	        delete ((BinaryOpBase*)exprTree)->rArg;
	    }
	}
        if(((ExprTree*)exprTree)->ref > 0)
        // there are more than one pointer to this expression tree
        {
 	    ((ExprTree*)exprTree)->ref--;
        }
        else
        // no more pointer to this expression tree
        {
	    	if(((ExprTree*)exprTree)->type == LX_VARIABLE)
	    	{
#ifdef USE_STRING_SPACE_IN_CLASSADS
				classad_string_space.disposeByIndex(
				    ((VariableBase*)exprTree)->stringSpaceIndex);
#else
				delete []((VariableBase*)exprTree)->name;
#endif
	    	}
	    	if(((ExprTree*)exprTree)->type == LX_STRING)
	    	{
#ifdef USE_STRING_SPACE_IN_CLASSADS
				classad_string_space.disposeByIndex(
				    ((StringBase*)exprTree)->stringSpaceIndex);
#else
				delete []((StringBase*)exprTree)->value;
#endif
	    	}
	    	delete (Dummy*)exprTree;
        }
    }
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
    if(tree.MyType() == this->MyType())
    {
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

////////////////////////////////////////////////////////////////////////////////
// Increment the reference counter of the whole tree                          //
////////////////////////////////////////////////////////////////////////////////
ExprTree* ExprTree::Copy()
{
    if(this->cardinality > 0)
    // binary operator, copy children first
    {
		if(((BinaryOpBase*)this)->lArg)
		{
			((BinaryOpBase*)this)->lArg->Copy();
		}
		if(((BinaryOpBase*)this)->rArg)
		{
			((BinaryOpBase*)this)->rArg->Copy();
		}
    }
    this->ref++;
	return this;
}

////////////////////////////////////////////////////////////////////////////////
// Increment reference counter                                                //
////////////////////////////////////////////////////////////////////////////////

ExprTree* BinaryOpBase::Copy()
{
    this->ref++;
    if(this->lArg)
    {
		this->lArg->Copy();
    }
    if(this->rArg)
    {
		this->rArg->Copy();
    }
	return this;
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

int BooleanBase::Value()
{
	return value;
}

