//****************************************************************************//
// astbase.C 				                                      //
//  	 				                                      //
// Implementation of the base Abstract Syntax Tree (AST) module.              //
//                                                                            //
//****************************************************************************//

#include <std.h>
#include <varargs.h>
#include <string.h>
#include "debug.h"    // required by dprintf
#include "exprtype.h"
#include "astbase.h"

#define AdvancePtr(ptr)  while(*ptr != '\0') ptr++;

// extern  void dprintf(int, char *fmt, ...);

////////////////////////////////////////////////////////////////////////////////
// Tree node constructors.                                                    //
////////////////////////////////////////////////////////////////////////////////

VariableBase::VariableBase(char* name)
{
    this->name = name;
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
    value = str;
    this->ref = 0;
    this->type  = LX_STRING;
    this->cardinality = 0;
}

NullBase::NullBase()
{
    this->ref = 0;
    this->type  = LX_NULL;
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
				delete []((VariableBase*)exprTree)->name;
	    	}
	    	if(((ExprTree*)exprTree)->type == LX_STRING)
	    	{
				delete []((StringBase*)exprTree)->value;
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

int ExprTree::operator >(ExprTree& tree)
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

int ExprTree::operator >=(ExprTree& tree)
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

int ExprTree::operator <(ExprTree& tree)
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

int ExprTree::operator <=(ExprTree& tree)
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
// This function returns TRUE if in order for a tree to be TRUE, "tree"
// has to be true.
//
////////////////////////////////////////////////////////////////////////////////

int ExprTree::RequireClass(ExprTree* tree)
{
    if(*this == *tree)
    {
	return TRUE;
    }
    return FALSE;
}

int EqOpBase::RequireClass(ExprTree* tree)
{
    switch(tree->MyType())
    {
	case LX_OR :

	    if(RequireClass(tree->LArg()) || RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

	case LX_AND :

	    if(RequireClass(tree->LArg()) && RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

        case LX_EQ :

	    if(*this == *tree)
	    {
		return TRUE;
	    }
	    break;

	default :

	    break;
    }
    return FALSE;
}

int NeqOpBase::RequireClass(ExprTree* tree)
{
    switch(tree->MyType())
    {
	case LX_OR :

	    if(RequireClass(tree->LArg()) || RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

	case LX_AND :

	    if(RequireClass(tree->LArg()) && RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

        case LX_NEQ :

	    if(*this == *tree)
	    {
		return TRUE;
	    }
	    break;

        default :

	    break;
    }
    return FALSE;
}

int GtOpBase::RequireClass(ExprTree* tree)
{
    ExprTree*	tmpTree;
    int		tmpResult;

    switch(tree->MyType())
    {
	case LX_OR :

	    if(RequireClass(tree->LArg()) || RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

	case LX_AND :

	    if(RequireClass(tree->LArg()) && RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

        case LX_GT :
	case LX_GE :

	    if(lArg->MyType() == LX_VARIABLE)
	    {
		if(tree->LArg()->MyType() == LX_VARIABLE)
		{
		    return *rArg >= *(tree->RArg());
		}
	    }
	    else if(rArg->MyType() == LX_VARIABLE)
	    {
		lArg->Copy();
		rArg->Copy();
		tmpTree = new LtOpBase(rArg, lArg);
		tmpResult = tmpTree->RequireClass(tree);
		delete tmpTree;
		return tmpResult;
	    }
	    else if(*this == *tree)
	    {
		return TRUE;
	    }
	    break;
	
	case LX_LT :
	case LX_LE :

	    if(tree->RArg()->MyType() == LX_VARIABLE)
	    {
		tree->LArg()->Copy();
		tree->RArg()->Copy();
		tmpTree = new GtOpBase(tree->RArg(), tree->LArg());
		tmpResult = RequireClass(tmpTree);
		delete tmpTree;
		return tmpResult;
	    }
	    break;

	default :

	    break;
    }
    return FALSE;
}

int GeOpBase::RequireClass(ExprTree* tree)
{
    ExprTree*	tmpTree;
    int		tmpResult;

    switch(tree->MyType())
    {
	case LX_OR :

	    if(RequireClass(tree->LArg()) || RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

	case LX_AND :

	    if(RequireClass(tree->LArg()) && RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

        case LX_GT :
	case LX_GE :

	    if(lArg->MyType() == LX_VARIABLE)
	    {
		if(tree->LArg()->MyType() == LX_VARIABLE)
		{
		    return *rArg > *(tree->RArg());
		}
	    }
	    else if(rArg->MyType() == LX_VARIABLE)
	    {
		lArg->Copy();
		rArg->Copy();
		tmpTree = new LtOpBase(rArg, lArg);
		tmpResult = tmpTree->RequireClass(tree);
		delete tmpTree;
		return tmpResult;
	    }
	    else if(*this == *tree)
	    {
		return TRUE;
	    }
	    break;
	
	case LX_LT :
	case LX_LE :

	    if(tree->RArg()->MyType() == LX_VARIABLE)
	    {
		tree->LArg()->Copy();
		tree->RArg()->Copy();
		tmpTree = new GtOpBase(tree->RArg(), tree->LArg());
		tmpResult = RequireClass(tmpTree);
		delete tmpTree;
		return tmpResult;
	    }
	    break;

	default :
	 
	    break;
    }
    return FALSE;
}

int LtOpBase::RequireClass(ExprTree* tree)
{
    ExprTree*	tmpTree;
    int		tmpResult;

    switch(tree->MyType())
    {
	case LX_OR :

	    if(RequireClass(tree->LArg()) || RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

	case LX_AND :

	    if(RequireClass(tree->LArg()) && RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

        case LX_LT :
	case LX_LE :

	    if(lArg->MyType() == LX_VARIABLE)
	    {
		if(tree->LArg()->MyType() == LX_VARIABLE)
		{
		    return *rArg <= *(tree->RArg());
		}
	    }
	    else if(rArg->MyType() == LX_VARIABLE)
	    {
		lArg->Copy();
		rArg->Copy();
		tmpTree = new LtOpBase(rArg, lArg);
		tmpResult = tmpTree->RequireClass(tree);
		delete tmpTree;
		return tmpResult;
	    }
	    else if(*this == *tree)
	    {
		return TRUE;
	    }
	    break;
	
	case LX_GT :
	case LX_GE :

	    if(tree->RArg()->MyType() == LX_VARIABLE)
	    {
		tree->LArg()->Copy();
		tree->RArg()->Copy();
		tmpTree = new LtOpBase(tree->RArg(), tree->LArg());
		tmpResult = RequireClass(tmpTree);
		delete tmpTree;
		return tmpResult;
	    }
	    break;

	default :
	 
	    break;
    }
    return FALSE;
}

int LeOpBase::RequireClass(ExprTree* tree)
{
    ExprTree*	tmpTree;
    int		tmpResult;

    switch(tree->MyType())
    {
	case LX_OR :

	    if(RequireClass(tree->LArg()) || RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

	case LX_AND :

	    if(RequireClass(tree->LArg()) && RequireClass(tree->RArg()))
	    {
		return TRUE;
	    }
	    break;

        case LX_LT :
	case LX_LE :

	    if(lArg->MyType() == LX_VARIABLE)
	    {
		if(tree->LArg()->MyType() == LX_VARIABLE)
		{
		    return *rArg < *(tree->RArg());
		}
	    }
	    else if(rArg->MyType() == LX_VARIABLE)
	    {
		lArg->Copy();
		rArg->Copy();
		tmpTree = new LtOpBase(rArg, lArg);
		tmpResult = tmpTree->RequireClass(tree);
		delete tmpTree;
		return tmpResult;
	    }
	    else if(*this == *tree)
	    {
		return TRUE;
	    }
	    break;
	
	case LX_GT :
	case LX_GE :

	    if(tree->RArg()->MyType() == LX_VARIABLE)
	    {
		tree->LArg()->Copy();
		tree->RArg()->Copy();
		tmpTree = new LtOpBase(tree->RArg(), tree->LArg());
		tmpResult = RequireClass(tmpTree);
		delete tmpTree;
		return tmpResult;
	    }
	    break;

	default :
	 
	    break;
    }
    return FALSE;
}

int AndOpBase::RequireClass(ExprTree* tree)
{
    switch(tree->MyType())
    {
	case LX_AND :

	    return RequireClass(tree->LArg()) && RequireClass(tree->RArg());

	case LX_OR :

	    return RequireClass(tree->LArg()) || RequireClass(tree->RArg());

	default :

	    return lArg->RequireClass(tree) || rArg->RequireClass(tree);
    }
    return FALSE;
}

int OrOpBase::RequireClass(ExprTree* tree)
{
    switch(tree->MyType())
    {
	case LX_AND :

	    return RequireClass(tree->LArg()) && RequireClass(tree->RArg());

        case LX_OR :

	    return RequireClass(tree->LArg()) && RequireClass(tree->RArg());
        
	default :

	    return lArg->RequireClass(tree) && rArg->RequireClass(tree);
    }
    return FALSE;
}

int AssignOpBase::RequireClass(ExprTree* tree)
{
	return rArg->RequireClass(tree);
}

////////////////////////////////////////////////////////////////////////////////
// Increment the reference counter of the whole tree                          //
////////////////////////////////////////////////////////////////////////////////
void ExprTree::Copy()
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
}

////////////////////////////////////////////////////////////////////////////////
// Increment reference counter                                                //
////////////////////////////////////////////////////////////////////////////////

void BinaryOpBase::Copy()
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
    dprintf(D_NOHEADER | D_ALWAYS, value ? "TRUE" : "FALSE");
}

void NullBase::Display()
{
    dprintf(D_NOHEADER | D_ALWAYS, "NULL");
}

void ErrorBase::Display()
{
    if(partialTree) partialTree->Display();
}

void AddOpBase::Display()
{
    if(lArg)
    {
	lArg->Display();
    }
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
