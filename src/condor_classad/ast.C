//******************************************************************************
// ast.C
//
// Implementation of the AST module with an interface to the AttrList module.
//
//******************************************************************************

#include <std.h>
#include <string.h>
#include "exprtype.h"
#include "ast.h"
#include "classad.h"
#include "buildtable.h"

#define AdvancePtr(ptr)  while(*ptr != '\0') ptr++;

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

	case LX_BOOL :

	     fprintf(f, this->b ? "TRUE" : "FALSE");
	     break;

	case LX_NULL :

	     fprintf(f, "NULL");
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

AggAddOp::AggAddOp(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type = LX_AGGADD;
    this->cardinality = 2;
}

AggEqOp::AggEqOp(ExprTree* l, ExprTree* r)
{
    this->lArg = l;
    this->rArg = r;
    this->ref  = 0;
    this->type = LX_AGGEQ;
    this->cardinality = 2;
}

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

int Variable::EvalTree(AttrListList* classads, EvalResult* val)
{
    ExprTree* tmp = NULL;
    
    if(!val) 
    {
	return FALSE;
    }
    
    if(!(tmp = classads->Lookup(name)))
    {
	val->type = LX_UNDEFINED;
        return TRUE;
    }

    return tmp->EvalTree(classads, val);
}

int Variable::EvalTree(AttrList* classad, EvalResult* val)
{
    ExprTree* tmp = NULL;
    
    if(!val) 
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


//------tw-----------------

int Variable::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
{
    ExprTree* tmp = NULL;
    
    if(!val) 
    {
	return FALSE;
    }

    char * myNamePrefix = "MY.";
    char * reqNamePrefix = "TARGET.";

    if (name == NULL)
       {
        cout << "no name provided!!"<< endl;
        exit(1);                  
       }
    
   if (strncmp(name, myNamePrefix, 3) == 0)
      {
 
      // test char * name, if the first three letters are "MY." 
      // do evaluation  in my_classad.
      
       char* realName = new char[strlen(name)-2];
       strcpy(realName, name+3);
   
       if(!(tmp = my_classad->Lookup(realName)))        //name should be without prefix "MY."
	 {
	    val->type = LX_UNDEFINED;
	    delete []realName;
	    return TRUE;
	 }
       delete []realName;
       return tmp->EvalTree(my_classad, req_classad, val);
       
       }

    else if(strncmp(name, reqNamePrefix, 7) == 0)
      {
          
	// test char * name, if the first four letters are "TARGET." 
	// do evaluation  in req_classad.

	char* realName = new char[strlen(name)-6];
	strcpy(realName, name+7);
         
	if(!(tmp = req_classad->Lookup(realName)))     //name should be without prefix "TARGET."
	  {
	    val->type = LX_UNDEFINED;
	    delete []realName;
	    return TRUE;
	  }
	delete []realName;
	return tmp->EvalTree(my_classad, req_classad, val);

      }


    else
      {
	cerr << "Variable is not prefixed by either 'MY.' or 'TARGET.'"<< endl;
	exit(1);              // for testing
	return TRUE;          //for avoiding compiler warning
      }

  }

//---------------------------

int Integer::EvalTree(AttrListList* classads, EvalResult* val)
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

int Integer::EvalTree(AttrList* classad, EvalResult* val)
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
int Integer::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
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


int Float::EvalTree(AttrListList* classads, EvalResult* val)
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


int Float::EvalTree(AttrList* classad, EvalResult* val)
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



//----------tw------------------

int Float::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
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

int String::EvalTree(AttrListList* classads, EvalResult* val)
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

int String::EvalTree(AttrList* classad, EvalResult* val)
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


//-------tw-----------------------------

int String::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
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


//-----------------------------------


int Boolean::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_BOOL;
    val->b = value;
    return TRUE;
}

int Boolean::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_BOOL;
    val->b = value;
    return TRUE;
}


//-----------tw------------------------

int Boolean::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_BOOL;
    val->b = value;
    return TRUE;
}

//-----------------------------------


int Null::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_NULL;
    return TRUE;
}

int Null::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_NULL;
    return TRUE;
}

//-------------tw----------------------------

int Null::EvalTree(AttrList* my_classad, AttrList* req_classad,EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    val->type = LX_NULL;
    return TRUE;
}
//------------------------------------------

int Error::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_ERROR;
    return TRUE;
}

int Error::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_ERROR;
    return TRUE;
}

//------------tw-------------------
int Error::EvalTree(AttrList* my_classad, AttrList* req_classad,  EvalResult* val)
{
    if(!val)
    {
	return FALSE;
    }
    val->type = LX_ERROR;
    return TRUE;
}
//--------------------------------

int AddOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }

    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->type = LX_INTEGER;
    	if(unit == 'k')
      	    val->i = (lArgVal.i + rArgVal.i) / 1024;
	else
      	    val->i = lArgVal.i + rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
    	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.f + rArgVal.f) / 1024;
	else
      	    val->f = lArgVal.f + rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
    	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.i + rArgVal.f) / 1024;
	else
      	    val->f = lArgVal.i + rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
   	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.f + rArgVal.i) / 1024;
	else
      	    val->f = lArgVal.f + rArgVal.i;
    }
    return TRUE;
}

int AddOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
    	val->type = LX_INTEGER;
	if(unit == 'k')
       	    val->i = (lArgVal.i + rArgVal.i) / 1024;
	else
      	    val->i = lArgVal.i + rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
    	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.f + rArgVal.f) / 1024;
	else
      	    val->f = lArgVal.f + rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
    	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.i + rArgVal.f) / 1024;
	else
      	    val->f = lArgVal.i + rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
    	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.f + rArgVal.i) / 1024;
	else
      	    val->f = lArgVal.f + rArgVal.i;
    }

    return TRUE;
}


//----------tw--------------------

int AddOp::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);         //---tw-------
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);         //---tw-----
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
    	val->type = LX_INTEGER;
	if(unit == 'k')
       	    val->i = (lArgVal.i + rArgVal.i) / 1024;
	else
      	    val->i = lArgVal.i + rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
    	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.f + rArgVal.f) / 1024;
	else
      	    val->f = lArgVal.f + rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
    	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.i + rArgVal.f) / 1024;
	else
      	    val->f = lArgVal.i + rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
    	val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.f + rArgVal.i) / 1024;
	else
      	    val->f = lArgVal.f + rArgVal.i;
    }

    return TRUE;
}

//--------------------------------


int SubOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
   
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->type = LX_INTEGER;
	if(unit == 'k')
            val->i = (lArgVal.i - rArgVal.i) / 1024;
	else
	    val->i = lArgVal.i - rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f - rArgVal.f) / 1024;
	else
            val->f = lArgVal.f - rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
      	    val->f = (lArgVal.i - rArgVal.f) / 1024;
	else
      	    val->f = lArgVal.i - rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f - rArgVal.i) / 1024;
	else
            val->f = lArgVal.f - rArgVal.i;
    }

    return TRUE;
}

int SubOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
    	val->type = LX_INTEGER;
	if(unit == 'k')
      	    val->i = (lArgVal.i - rArgVal.i) / 1024;
	else
	    val->i = lArgVal.i - rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f - rArgVal.f) / 1024;
	else
            val->f = lArgVal.f - rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.i - rArgVal.f) / 1024;
	else
            val->f = lArgVal.i - rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f - rArgVal.i) / 1024;
	else
            val->f = lArgVal.f - rArgVal.i;
    }

    return TRUE;
}

//---------tw--------------------------
int SubOp::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);       //----tw-------
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);       //----tw------
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
    	val->type = LX_INTEGER;
	if(unit == 'k')
      	    val->i = (lArgVal.i - rArgVal.i) / 1024;
	else
	    val->i = lArgVal.i - rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f - rArgVal.f) / 1024;
	else
            val->f = lArgVal.f - rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.i - rArgVal.f) / 1024;
	else
            val->f = lArgVal.i - rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f - rArgVal.i) / 1024;
	else
            val->f = lArgVal.f - rArgVal.i;
    }

    return TRUE;
}
//------------------------------------


int MultOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->type = LX_INTEGER;
	if(unit == 'k')
            val->i = (lArgVal.i * rArgVal.i) / 1024;
	else
            val->i = lArgVal.i * rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f * rArgVal.f) / 1024;
	else
            val->f = lArgVal.f * rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.i * rArgVal.f) / 1024;
	else
            val->f = lArgVal.i * rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f * rArgVal.i) / 1024;
	else
            val->f = lArgVal.f * rArgVal.i;
    }

    return TRUE;
}

int MultOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->type = LX_INTEGER;
	if(unit == 'k')
            val->i = (lArgVal.i * rArgVal.i) / 1024;
	else
            val->i = lArgVal.i * rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f * rArgVal.f) / 1024;
	else
            val->f = lArgVal.f * rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.i * rArgVal.f) / 1024;
	else
            val->f = lArgVal.i * rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f * rArgVal.i) / 1024;
	else
            val->f = lArgVal.f * rArgVal.i;
    }

    return TRUE;
}


//---------------tw-----------------------
int MultOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->type = LX_INTEGER;
	if(unit == 'k')
            val->i = (lArgVal.i * rArgVal.i) / 1024;
	else
            val->i = lArgVal.i * rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f * rArgVal.f) / 1024;
	else
            val->f = lArgVal.f * rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.i * rArgVal.f) / 1024;
	else
            val->f = lArgVal.i * rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f * rArgVal.i) / 1024;
	else
            val->f = lArgVal.f * rArgVal.i;
    }

    return TRUE;
}
//----------------------------------------


int DivOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->type = LX_INTEGER;
	if(unit == 'k')
            val->i = (lArgVal.i / rArgVal.i) / 1024;
	else
            val->i = lArgVal.i / rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f / rArgVal.f) / 1024;
	else
            val->f = lArgVal.f / rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.i / rArgVal.f) / 1024;
	else
            val->f = lArgVal.i / rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f / rArgVal.i) / 1024;
	else
            val->f = lArgVal.f / rArgVal.i;
    }

    return TRUE;
}

int DivOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->type = LX_INTEGER;
	if(unit == 'k')
            val->i = (lArgVal.i / rArgVal.i) / 1024;
	else
            val->i = lArgVal.i / rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f / rArgVal.f) / 1024;
	else
            val->f = lArgVal.f / rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.i / rArgVal.f) / 1024;
	else
            val->f = lArgVal.i / rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f / rArgVal.i) / 1024;
	else
            val->f = lArgVal.f / rArgVal.i;
    }

    return TRUE;
}


//--------------tw------------------------

int DivOp::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal, rArgVal;
    LexemeType lArgType, rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);      //----tw---
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);      //---tw----
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->type = LX_INTEGER;
	if(unit == 'k')
            val->i = (lArgVal.i / rArgVal.i) / 1024;
	else
            val->i = lArgVal.i / rArgVal.i;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f / rArgVal.f) / 1024;
	else
            val->f = lArgVal.f / rArgVal.f;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.i / rArgVal.f) / 1024;
	else
            val->f = lArgVal.i / rArgVal.f;
    }
    if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->type = LX_FLOAT;
	if(unit == 'k')
            val->f = (lArgVal.f / rArgVal.i) / 1024;
	else
            val->f = lArgVal.f / rArgVal.i;
    }

    return TRUE;
}
//---------------------------------------


int GtOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.i > rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f > rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i > rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f > rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

int GtOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.i > rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f > rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i > rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f > rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

//------------tw------------------------
int GtOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);       //----tw----
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);       //----tw----
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.i > rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f > rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i > rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f > rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}
//-------------------------------------


int GeOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i >= rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f >= rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i >= rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f >= rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

int GeOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i >= rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f >= rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i >= rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f >= rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}


//----------tw----------------------------------
int GeOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);        //---tw---
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);        //---tw---
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i >= rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f >= rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i >= rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f >= rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

//-----------------------------


int LtOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i < rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f < rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i < rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f < rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

int LtOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i < rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f < rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i < rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f < rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

//----------tw----------------------------
int LtOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);              //-----tw---
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);              //---tw----
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i < rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f < rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i < rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f < rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

//---------------------------------------


int LeOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i <= rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f <= rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i <= rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f <= rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

int LeOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i <= rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f <= rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i <= rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f <= rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

//----------tw-----------------------
int LeOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad,  &lArgVal);        //------tw------
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad,  &rArgVal);         //-------tw----
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i <= rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f <= rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i <= rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f <= rArgVal.i;
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}
//----------------------------------


int EqOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i == rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f == rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i == rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f == rArgVal.i;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->b = lArgVal.b == rArgVal.b;
    }
    else if(lArgType == LX_STRING && rArgType == LX_STRING)
    {
	if(!strcasecmp(lArgVal.s, rArgVal.s))
	{
	    val->b = TRUE;
	}
	else
	{
	    val->b = FALSE;
	}
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

int EqOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i == rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f == rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i == rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f == rArgVal.i;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->b = lArgVal.b == rArgVal.b;
    }
    else if(lArgType == LX_STRING && rArgType == LX_STRING)
    {
	if(!strcasecmp(lArgVal.s, rArgVal.s))
	{
	    val->b = TRUE;
	}
	else
	{
	    val->b = FALSE;
	}
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

//-------------tw--------------------
int EqOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);        //------tw----
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);       //-----tw------
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i == rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f == rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i == rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f == rArgVal.i;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->b = lArgVal.b == rArgVal.b;
    }
    else if(lArgType == LX_STRING && rArgType == LX_STRING)
    {
	if(!strcasecmp(lArgVal.s, rArgVal.s))
	{
	    val->b = TRUE;
	}
	else
	{
	    val->b = FALSE;
	}
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}
//---------------------------------------------


int NeqOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i != rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f != rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i != rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f != rArgVal.i;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->b = lArgVal.b != rArgVal.b;
    }
    else if(lArgType == LX_STRING && rArgType == LX_STRING)
    {
	if(!strcasecmp(lArgVal.s, rArgVal.s))
	{
	    val->b = FALSE;
	}
	else
	{
	    val->b = TRUE;
	}
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

int NeqOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i != rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f != rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i != rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f != rArgVal.i;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->b = lArgVal.b != rArgVal.b;
    }
    else if(lArgType == LX_STRING && rArgType == LX_STRING)
    {
	if(!strcasecmp(lArgVal.s, rArgVal.s))
	{
	    val->b = FALSE;
	}
	else
	{
	    val->b = TRUE;
	}
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}

//-----------tw----------------
int NeqOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);      //----tw------
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);      //-----tw-----
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;
  
    val->type = LX_BOOL;
    if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    if(lArgType == LX_INTEGER && rArgType == LX_INTEGER)
    {
	val->b = lArgVal.i != rArgVal.i;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.f != rArgVal.f;
    }
    else if(lArgType == LX_INTEGER && rArgType == LX_FLOAT)
    {
        val->b = lArgVal.i != rArgVal.f;
    }
    else if(lArgType == LX_FLOAT && rArgType == LX_INTEGER)
    {
        val->b = lArgVal.f != rArgVal.i;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->b = lArgVal.b != rArgVal.b;
    }
    else if(lArgType == LX_STRING && rArgType == LX_STRING)
    {
	if(!strcasecmp(lArgVal.s, rArgVal.s))
	{
	    val->b = FALSE;
	}
	else
	{
	    val->b = TRUE;
	}
    }
    else
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
        sumFlag = val->b;
    }

    return TRUE;
}
//----------------------------

int AndOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if((lArgType != LX_BOOL && lArgType != LX_ERROR && lArgType != LX_NULL) ||
       (rArgType != LX_BOOL && rArgType != LX_ERROR && rArgType != LX_NULL))
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    else if((lArgType == LX_BOOL && !lArgVal.b) ||
	    (rArgType == LX_BOOL && !rArgVal.b))
    {
	val->type = LX_BOOL;
	val->b = FALSE;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->type = LX_BOOL;
  	val->b = lArgVal.b && rArgVal.b;
    }
    else
    {
	val->type = LX_NULL;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
	sumFlag = val->b;
    }

    return TRUE;
}

int AndOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if((lArgType != LX_BOOL && lArgType != LX_ERROR && lArgType != LX_NULL) ||
       (rArgType != LX_BOOL && rArgType != LX_ERROR && rArgType != LX_NULL))
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    else if((lArgType == LX_BOOL && !lArgVal.b) ||
	    (rArgType == LX_BOOL && !rArgVal.b))
    {
	val->type = LX_BOOL;
	val->b = FALSE;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->type = LX_BOOL;
  	val->b = lArgVal.b && rArgVal.b;
    }
    else
    {
	val->type = LX_NULL;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
	sumFlag = val->b;
    }

    return TRUE;
}


//--------------tw--------------------
int AndOp::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad, &lArgVal);          //-----tw-----
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &rArgVal);          //-----tw-----
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if((lArgType != LX_BOOL && lArgType != LX_ERROR && lArgType != LX_NULL) ||
       (rArgType != LX_BOOL && rArgType != LX_ERROR && rArgType != LX_NULL))
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    else if((lArgType == LX_BOOL && !lArgVal.b) ||
	    (rArgType == LX_BOOL && !rArgVal.b))
    {
	val->type = LX_BOOL;
	val->b = FALSE;
    }
    else if(lArgType == LX_BOOL && rArgType == LX_BOOL)
    {
	val->type = LX_BOOL;
  	val->b = lArgVal.b && rArgVal.b;
    }
    else
    {
	val->type = LX_NULL;
	return TRUE;
    }

    if(sumFlag == FALSE)
    {
	sumFlag = val->b;
    }

    return TRUE;
}
//------------------------------------


int OrOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classads, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classads, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if((lArgType != LX_BOOL && lArgType != LX_ERROR && lArgType != LX_NULL) ||
       (rArgType != LX_BOOL && rArgType != LX_ERROR && rArgType != LX_NULL))
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    else if((lArgType == LX_BOOL && lArgVal.b) ||
	    (rArgType == LX_BOOL && rArgVal.b))
    {
	val->type = LX_BOOL;
	val->b = TRUE;
    }
    else if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    else
    {
	val->type = LX_BOOL;
	val->b = FALSE;
    }

    if(sumFlag == FALSE)
    {
	sumFlag = val->b;
    }

    return TRUE;
}

int OrOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(classad, &lArgVal);
    ((ExprTree*)rArg)->EvalTree(classad, &rArgVal);
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if((lArgType != LX_BOOL && lArgType != LX_ERROR && lArgType != LX_NULL) ||
       (rArgType != LX_BOOL && rArgType != LX_ERROR && rArgType != LX_NULL))
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    else if((lArgType == LX_BOOL && lArgVal.b) ||
	    (rArgType == LX_BOOL && rArgVal.b))
    {
	val->type = LX_BOOL;
	val->b = TRUE;
    }
    else if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    else
    {
	val->type = LX_BOOL;
	val->b = FALSE;
    }

    if(sumFlag == FALSE)
    {
	sumFlag = val->b;
    }

    return TRUE;
}

//-----------tw--------------------

int OrOp::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    EvalResult lArgVal;
    EvalResult rArgVal;
    LexemeType lArgType;
    LexemeType rArgType;

    ((ExprTree*)lArg)->EvalTree(my_classad, req_classad,  &lArgVal);        //----tw----
    ((ExprTree*)rArg)->EvalTree(my_classad, req_classad,  &rArgVal);        //---tw-----
    lArgType = lArgVal.type;
    rArgType = rArgVal.type;

    if((lArgType != LX_BOOL && lArgType != LX_ERROR && lArgType != LX_NULL) ||
       (rArgType != LX_BOOL && rArgType != LX_ERROR && rArgType != LX_NULL))
    {
	val->type = LX_UNDEFINED;
	return TRUE;
    }

    if(lArgType == LX_ERROR || rArgType == LX_ERROR)
    {
	val->type = LX_ERROR;
	return TRUE;
    }
    else if((lArgType == LX_BOOL && lArgVal.b) ||
	    (rArgType == LX_BOOL && rArgVal.b))
    {
	val->type = LX_BOOL;
	val->b = TRUE;
    }
    else if(lArgType == LX_NULL || rArgType == LX_NULL)
    {
	val->type = LX_NULL;
	return TRUE;
    }
    else
    {
	val->type = LX_BOOL;
	val->b = FALSE;
    }

    if(sumFlag == FALSE)
    {
	sumFlag = val->b;
    }

    return TRUE;
}
//----------------------------------


int AssignOp::EvalTree(AttrListList* classads, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    if(rArg)
    {
    	return ((ExprTree*)rArg)->EvalTree(classads, val);
    }
    else
    {
	val->type = LX_NULL;
    }
    return TRUE;
}

int AssignOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    if(rArg)
    {
    	return ((ExprTree*)rArg)->EvalTree(classad, val);
    }
    else
    {
	val->type = LX_NULL;
    }
    return TRUE;
}

//----------tw--------------------------
int AssignOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    if(rArg)
    {
    	return ((ExprTree*)rArg)->EvalTree(my_classad, req_classad, val);      //---tw-----
    }
    else
    {
	val->type = LX_NULL;
    }
    return TRUE;
}
//---------------------------------------


int AggEqOp::EvalTree(AttrListList* classadList, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    if(!((ExprTree*)lArg)->EvalTree(classadList, val))
    {
	return FALSE;
    }
    if(rArg)
    {
	EvalResult tmpResult;

	if(!((ExprTree*)rArg)->EvalTree(classadList, &tmpResult))
	{
	    return FALSE;
	}
	if(val->type == tmpResult.type)
	{
	    switch(val->type)
	    {
		case LX_INTEGER :
		     
		     if(val->i != tmpResult.i)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_FLOAT :
		     
		     if(val->f != tmpResult.f)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_STRING :
		     
		     if(strcmp(val->s, tmpResult.s))
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_BOOL :
		     
		     if(val->b != tmpResult.b)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		default :

		     return TRUE;
	    }
	}
	else
	{
	    val->type = LX_NULL;
	}
    }
    return TRUE;
}

int AggEqOp::EvalTree(AttrList* classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    if(!((ExprTree*)lArg)->EvalTree(classad, val))
    {
	return FALSE;
    }
    if(rArg)
    {
	EvalResult tmpResult;

	if(!((ExprTree*)rArg)->EvalTree(classad, &tmpResult))
	{
	    return FALSE;
	}
	if(val->type == tmpResult.type)
	{
	    switch(val->type)
	    {
		case LX_INTEGER :
		     
		     if(val->i != tmpResult.i)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_FLOAT :
		     
		     if(val->f != tmpResult.f)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_STRING :
		     
		     if(strcmp(val->s, tmpResult.s))
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_BOOL :
		     
		     if(val->b != tmpResult.b)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }
		
		default :

		     return TRUE;
	    }
	}
	else
	{
	    val->type = LX_NULL;
	}
    }
    return TRUE;
}


//-----------tw------------------------------------
int AggEqOp::EvalTree(AttrList* my_classad, AttrList* req_classad, EvalResult* val)
{
    if(!val) 
    {
	return FALSE;
    }
    if(!((ExprTree*)lArg)->EvalTree(my_classad, req_classad, val))       //---tw----
    {
	return FALSE;
    }
    if(rArg)
    {
	EvalResult tmpResult;

	if(!((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &tmpResult))    //---tw---
	{
	    return FALSE;
	}
	if(val->type == tmpResult.type)
	{
	    switch(val->type)
	    {
		case LX_INTEGER :
		     
		     if(val->i != tmpResult.i)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_FLOAT :
		     
		     if(val->f != tmpResult.f)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_STRING :
		     
		     if(strcmp(val->s, tmpResult.s))
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }

		case LX_BOOL :
		     
		     if(val->b != tmpResult.b)
		     {
			 val->type = LX_NULL;
			 return TRUE;
		     }
		
		default :

		     return TRUE;
	    }
	}
	else
	{
	    val->type = LX_NULL;
	}
    }
    return TRUE;
}
//---------------------------------------------------


int AggAddOp::EvalTree(AttrListList* classads, EvalResult* result)
{
    if(!result) 
    {
	return FALSE;
    }
    if(!((ExprTree*)lArg)->EvalTree(classads, result))
    {
	return TRUE;
    }
    if(rArg)
    {
	EvalResult tmpResult;

	if(!((ExprTree*)rArg)->EvalTree(classads, &tmpResult))
	{
	    return FALSE;
	}

	if(result->type == LX_INTEGER && tmpResult.type == LX_INTEGER)
	{
	    result->i = result->i + tmpResult.i;
	}
	else if(result->type == LX_FLOAT && tmpResult.type == LX_FLOAT)
	{
	    result->f = result->f + tmpResult.f;
	}
	else if(result->type == LX_INTEGER && tmpResult.type == LX_FLOAT)
	{
	    result->type = LX_FLOAT;
	    result->f = result->i + tmpResult.f;
	}
	else if(result->type == LX_FLOAT && tmpResult.type == LX_INTEGER)
	{
	    result->type = LX_FLOAT;
	    result->f = result->f + tmpResult.i;
	}
	else
	{
	    result->type = LX_UNDEFINED;
	}
    }

    return TRUE;
}

int AggAddOp::EvalTree(AttrList* classad, EvalResult* result)
{
    if(!result) 
    {
	return FALSE;
    }
    if(!((ExprTree*)lArg)->EvalTree(classad, result))
    {
	return TRUE;
    }
    if(rArg)
    {
	EvalResult tmpResult;

	if(!((ExprTree*)rArg)->EvalTree(classad, &tmpResult))
	{
	    return FALSE;
	}

	if(result->type == LX_INTEGER && tmpResult.type == LX_INTEGER)
	{
	    result->i = result->i + tmpResult.i;
	}
	else if(result->type == LX_FLOAT && tmpResult.type == LX_FLOAT)
	{
	    result->f = result->f + tmpResult.f;
	}
	else if(result->type == LX_INTEGER && tmpResult.type == LX_FLOAT)
	{
	    result->type = LX_FLOAT;
	    result->f = result->i + tmpResult.f;
	}
	else if(result->type == LX_FLOAT && tmpResult.type == LX_INTEGER)
	{
	    result->type = LX_FLOAT;
	    result->f = result->f + tmpResult.i;
	}
	else
	{
	    result->type = LX_UNDEFINED;
	}
    }

    return TRUE;
}


//---------------tw----------------------------------
int AggAddOp::EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* result)
{
    if(!result) 
    {
	return FALSE;
    }
    if(!((ExprTree*)lArg)->EvalTree(my_classad, req_classad,result))       //----tw--------
    {
	return TRUE;
    }
    if(rArg)
    {
	EvalResult tmpResult;

	if(!((ExprTree*)rArg)->EvalTree(my_classad, req_classad, &tmpResult))   //---tw----
	{
	    return FALSE;
	}

	if(result->type == LX_INTEGER && tmpResult.type == LX_INTEGER)
	{
	    result->i = result->i + tmpResult.i;
	}
	else if(result->type == LX_FLOAT && tmpResult.type == LX_FLOAT)
	{
	    result->f = result->f + tmpResult.f;
	}
	else if(result->type == LX_INTEGER && tmpResult.type == LX_FLOAT)
	{
	    result->type = LX_FLOAT;
	    result->f = result->i + tmpResult.f;
	}
	else if(result->type == LX_FLOAT && tmpResult.type == LX_INTEGER)
	{
	    result->type = LX_FLOAT;
	    result->f = result->f + tmpResult.i;
	}
	else
	{
	    result->type = LX_UNDEFINED;
	}
    }

    return TRUE;
}
//----------------------------------------------

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
  strcat(str, "\"");
  strcat(str, value);
  strcat(str, "\"");
}

void Boolean::PrintToStr(char* str)
{
  sprintf(str, value ? "%sTRUE" : "%sFALSE", str, str);
}

void Null::PrintToStr(char* str)
{
  sprintf(str, "%sNULL", str);
}

void Error::PrintToStr(char* str)
{
    if(partialTree) partialTree->PrintToStr(str);
}

void AddOp::PrintToStr(char* str)
{
    if(lArg)
    {
	((ExprTree*)lArg)->PrintToStr(str);
    }
    else
    {
	strcat(str, "NULL");
    }
    strcat(str, " + ");
    if(rArg)
    {
	((ExprTree*)rArg)->PrintToStr(str);
    }
    else
    {
	strcat(str, "NULL");
    }
    if(unit == 'k')
    {
	strcat(str, " k");
    }
}

void SubOp::PrintToStr(char* str)
{
    if(lArg)
    {
	((ExprTree*)lArg)->PrintToStr(str);
    }
    else
    {
	strcat(str, "NULL");
    }
    strcat(str, " - ");
    if(rArg && rArg->MyType() == LX_ADD || rArg->MyType() == LX_SUB)
    {
	strcat(str, "(");
    	((ExprTree*)rArg)->PrintToStr(str);
    	strcat(str, ")");
    }
    else if(!rArg)
    {
	strcat(str, "NULL");
    }
    else
    {
	((ExprTree*)rArg)->PrintToStr(str);
    }
    if(unit == 'k')
    {
	strcat(str, " k");
    }
}

void MultOp::PrintToStr(char* str)
{
    if(lArg && lArg->MyType() == LX_ADD || lArg->MyType() == LX_SUB)
    {
    	strcat(str, "(");
    	((ExprTree*)lArg)->PrintToStr(str);
    	strcat(str, ")");
    }
    else if(!lArg)
    {
	strcat(str, "NULL");
    }
    else
    {
	((ExprTree*)lArg)->PrintToStr(str);
    }
    strcat(str, " * ");
    if(rArg && rArg->MyType() == LX_ADD || rArg->MyType() == LX_SUB)
    {
    	strcat(str, "(");
    	((ExprTree*)rArg)->PrintToStr(str);
    	strcat(str, ")");
    }
    else if(!rArg)
    {
	strcat(str, "NULL");
    }
    else
    {
	((ExprTree*)rArg)->PrintToStr(str);
    }
    if(unit == 'k')
    {
	strcat(str, " k");
    }
}

void DivOp::PrintToStr(char* str)
{
    if(lArg)
    {
  	switch(lArg->MyType())
	{
	    case LX_ADD :
	    case LX_SUB :
	    case LX_DIV : strcat(str, "(");
			  ((ExprTree*)lArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
    	    default     : ((ExprTree*)lArg)->PrintToStr(str);
    	}
    }
    else
    {
  	strcat(str, "NULL");
    }
    strcat(str, " / ");
    if(rArg)
    {
  	switch(rArg->MyType())
	{
	    case LX_ADD :
	    case LX_SUB :
	    case LX_DIV : strcat(str, "(");
			  ((ExprTree*)rArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
    	    default     : ((ExprTree*)rArg)->PrintToStr(str);
    	}
    }
    else
    {
  	strcat(str, "NULL");
    }
    if(unit == 'k')
    {
	strcat(str, " k");
    }
}

void EqOp::PrintToStr(char* str)
{
    if(lArg)
    {
    	switch(lArg->MyType())
	{
	    case LX_EQ  :
	    case LX_NEQ :
	    case LX_GT  :
	    case LX_GE  :
	    case LX_LT  :
	    case LX_LE  : strcat(str, "(");
			  ((ExprTree*)lArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
	    default:      ((ExprTree*)lArg)->PrintToStr(str);
   	}
    }
    else
    {
  	strcat(str, "NULL");
    }
    strcat(str, " == ");
    if(rArg)
    {
  	switch(rArg->MyType())
	{
    	    case LX_EQ  :
    	    case LX_NEQ :
    	    case LX_GT  :
    	    case LX_GE  :
    	    case LX_LT  :
    	    case LX_LE  : strcat(str, "(");
	  		  ((ExprTree*)rArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
    	    default:      ((ExprTree*)rArg)->PrintToStr(str);
  	}
    }
    else
    {
  	strcat(str, "NULL");
    }
}

void NeqOp::PrintToStr(char* str)
{
    if(lArg)
    {
    	switch(lArg->MyType())
	{
	    case LX_EQ  :
	    case LX_NEQ :
	    case LX_GT  :
	    case LX_GE  :
	    case LX_LT  :
	    case LX_LE  : strcat(str, "(");
			  ((ExprTree*)lArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
	    default:      ((ExprTree*)lArg)->PrintToStr(str);
   	}
    }
    else
    {
  	strcat(str, "NULL");
    }
    strcat(str, " != ");
    if(rArg)
    {
  	switch(rArg->MyType())
	{
    	    case LX_EQ  :
    	    case LX_NEQ :
    	    case LX_GT  :
    	    case LX_GE  :
    	    case LX_LT  :
    	    case LX_LE  : strcat(str, "(");
	  		  ((ExprTree*)rArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
    	    default:      ((ExprTree*)rArg)->PrintToStr(str);
  	}
    }
    else
    {
  	strcat(str, "NULL");
    }
}

void GtOp::PrintToStr(char* str)
{
    if(lArg)
    	switch(lArg->MyType()) {
	    case LX_EQ  :
	    case LX_NEQ :
	    case LX_GT  :
	    case LX_GE  :
	    case LX_LT  :
	    case LX_LE  : strcat(str, "(");
			  ((ExprTree*)lArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
	    default:      ((ExprTree*)lArg)->PrintToStr(str);
   	}
    else
  	strcat(str, "NULL");
    strcat(str, " > ");
    if(rArg)
  	switch(rArg->MyType()) {
    	    case LX_EQ  :
    	    case LX_NEQ :
    	    case LX_GT  :
    	    case LX_GE  :
    	    case LX_LT  :
    	    case LX_LE  : strcat(str, "(");
	  		  ((ExprTree*)rArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
    	    default:      ((ExprTree*)rArg)->PrintToStr(str);
  	}
    else
  	strcat(str, "NULL");
}

void GeOp::PrintToStr(char* str)
{
    if(lArg)
    	switch(lArg->MyType()) {
	    case LX_EQ  :
	    case LX_NEQ :
	    case LX_GT  :
	    case LX_GE  :
	    case LX_LT  :
	    case LX_LE  : strcat(str, "(");
			  ((ExprTree*)lArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
	    default:      ((ExprTree*)lArg)->PrintToStr(str);
   	}
    else
  	strcat(str, "NULL");
    strcat(str, " >= ");
    if(rArg)
  	switch(rArg->MyType()) {
    	    case LX_EQ  :
    	    case LX_NEQ :
    	    case LX_GT  :
    	    case LX_GE  :
    	    case LX_LT  :
    	    case LX_LE  : strcat(str, "(");
	  		  ((ExprTree*)rArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
    	    default:      ((ExprTree*)rArg)->PrintToStr(str);
  	}
    else
  	strcat(str, "NULL");
}

void LtOp::PrintToStr(char* str)
{
    if(lArg)
    	switch(lArg->MyType()) {
	    case LX_EQ  :
	    case LX_NEQ :
	    case LX_GT  :
	    case LX_GE  :
	    case LX_LT  :
	    case LX_LE  : strcat(str, "(");
			  ((ExprTree*)lArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
	    default:      ((ExprTree*)lArg)->PrintToStr(str);
   	}
    else
  	strcat(str, "NULL");
    strcat(str, " < ");
    if(rArg)
  	switch(rArg->MyType()) {
    	    case LX_EQ  :
    	    case LX_NEQ :
    	    case LX_GT  :
    	    case LX_GE  :
    	    case LX_LT  :
    	    case LX_LE  : strcat(str, "(");
	  		  ((ExprTree*)rArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
    	    default:      ((ExprTree*)rArg)->PrintToStr(str);
  	}
    else
  	strcat(str, "NULL");
}

void LeOp::PrintToStr(char* str)
{
    if(lArg)
    	switch(lArg->MyType()) {
	    case LX_EQ  :
	    case LX_NEQ :
	    case LX_GT  :
	    case LX_GE  :
	    case LX_LT  :
	    case LX_LE  : strcat(str, "(");
			  ((ExprTree*)lArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
	    default:      ((ExprTree*)lArg)->PrintToStr(str);
   	}
    else
  	strcat(str, "NULL");
    strcat(str, " <= ");
    if(rArg)
  	switch(rArg->MyType()) {
    	    case LX_EQ  :
    	    case LX_NEQ :
    	    case LX_GT  :
    	    case LX_GE  :
    	    case LX_LT  :
    	    case LX_LE  : strcat(str, "(");
	  		  ((ExprTree*)rArg)->PrintToStr(str);
			  strcat(str, ")");
			  break;
    	    default:      ((ExprTree*)rArg)->PrintToStr(str);
  	}
    else
  	strcat(str, "NULL");
}

void AndOp::PrintToStr(char* str)
{
    if(lArg)
  	switch(lArg->MyType()) {
	    case LX_AND :
	    case LX_INTEGER :
	    case LX_FLOAT :
	    case LX_STRING :
	    case LX_BOOL : ((ExprTree*)lArg)->PrintToStr(str);
			   break;
     	    default : strcat(str, "(");
		      ((ExprTree*)lArg)->PrintToStr(str);
		      strcat(str, ")");
    	}
    else
  	strcat(str, "NULL");
    strcat(str, " && ");
    if(rArg)
  	switch(rArg->MyType()) {
	    case LX_AND :
	    case LX_INTEGER :
	    case LX_FLOAT :
	    case LX_STRING :
	    case LX_BOOL : ((ExprTree*)rArg)->PrintToStr(str);
			   break;
    	    default : strcat(str, "(");
	 	      ((ExprTree*)rArg)->PrintToStr(str);
		      strcat(str, ")");
  	}
    else
  	strcat(str, "NULL");
}

void OrOp::PrintToStr(char* str)
{
    if(lArg)
  	switch(lArg->MyType()) {
	    case LX_AND :
	    case LX_INTEGER :
	    case LX_FLOAT :
	    case LX_STRING :
	    case LX_BOOL : ((ExprTree*)lArg)->PrintToStr(str);
			   break;
     	    default : strcat(str, "(");
		      ((ExprTree*)lArg)->PrintToStr(str);
		      strcat(str, ")");
    	}
    else
  	strcat(str, "NULL");
    strcat(str, " || ");
    if(rArg)
  	switch(rArg->MyType()) {
	    case LX_AND :
	    case LX_INTEGER :
	    case LX_FLOAT :
	    case LX_STRING :
	    case LX_BOOL : ((ExprTree*)rArg)->PrintToStr(str);
			   break;
    	    default : strcat(str, "(");
	 	      ((ExprTree*)rArg)->PrintToStr(str);
		      strcat(str, ")");
  	}
    else
  	strcat(str, "NULL");
}

void AssignOp::PrintToStr(char* str)
{
    if(lArg)
  	((ExprTree*)lArg)->PrintToStr(str);
    else
  	strcat(str, "NULL");
    strcat(str, " = ");
    if(rArg)
  	((ExprTree*)rArg)->PrintToStr(str);
    else
  	strcat(str, "NULL");
}

void AggAddOp::PrintToStr(char* str)
{
    if(lArg)
    {
        if(lArg->MyType() == LX_AGGADD)
	{
	    ((AggAddOp*)lArg)->PrintToStr(str);
	}
        else
        // lArg is assignment expression
        {
            // strcat(str, "(");
    	    ((ExprTree*)this)->LArg()->RArg()->PrintToStr(str);
    	    // strcat(str, ")");
        }
    }

    if(rArg && lArg)
    {
	strcat(str, " ++ ");
    }

    if(rArg)
    {
	if(rArg->MyType() == LX_AGGADD)
	{
	    ((AggAddOp*)rArg)->PrintToStr(str);
	}
	else
	{
  	    // strcat(str, "(");
  	    ((ExprTree*)this)->RArg()->RArg()->PrintToStr(str);
  	    // strcat(str, ")");
	}
    }
}

void AggEqOp::PrintToStr(char* str)
{
    if(lArg)
    {
        if(lArg->MyType() == LX_AGGEQ)
	{
	    ((AggEqOp*)lArg)->PrintToStr(str);
	}
        else
        // lArg is assignment expression
        {
            // strcat(str, "(");
    	    ((ExprTree*)this)->LArg()->RArg()->PrintToStr(str);
    	    // strcat(str, ")");
        }
    }

    if(rArg && lArg)
    {
	strcat(str, " ?= ");
    }

    if(rArg)
    {
	if(rArg->MyType() == LX_AGGEQ)
	{
	    ((AggEqOp*)rArg)->PrintToStr(str);
	}
	else
	{
  	    // strcat(str, "(");
  	    ((ExprTree*)this)->RArg()->RArg()->PrintToStr(str);
  	    // strcat(str, ")");
	}
    }
}

/*
////////////////////////////////////////////////////////////////////////////////
// Print spaces to a string for as long as an expresion.                      //
////////////////////////////////////////////////////////////////////////////////

void Variable::Indent(char** str)
{
  int i;

  for(i = strlen(name); i > 0; i--) {
 	**str = ' ';
	*str = *str + 1;
  }
  **str = '\0';
}

void Integer::Indent(char **str)
{
  char tmp[100];
  int  i;

  sprintf(tmp, "%d", value);
  for(i = strlen(tmp); i > 0; i--) {
	**str = ' ';
	*str = *str + 1;
  }
  if(unit == 'k') {
	sprintf(*str, "  ");
	*str = *str + 2;
  }
  **str = '\0';
}

void Float::Indent(char **str)
{
  char tmp[100];
  int  i;

  sprintf(tmp, "%f", value);
  for(i = strlen(tmp); i > 0; i--) {
	**str = ' ';
	*str = *str + 1;
  }
  if(unit == 'k') {
 	sprintf(*str, "  ");
	*str = *str + 2;
  }
  **str = '\0';
}

void String::Indent(char **str)
{
  int i;

  for(i = strlen(value)+2; i > 0; i--) {
 	**str = ' ';
	*str = *str + 1;
  }
  **str = '\0';
}

void Boolean::Indent(char **str)
{
  sprintf(*str, value? "%s    \0" : "%s     \0", *str, *str);
}

void Null::Indent(char **str)
{
  sprintf(*str, "%s    \0");
}

void AddOp::Indent(char **str)
{
  lArg->Indent(str);
  strcat(*str, "   ");
  *str = *str + 3;
  rArg->Indent(str);
  if(unit == 'k') {
	sprintf(*str, "  ");
	*str = *str + 2;
	**str = '\0';
  }
}

void SubOp::Indent(char **str)
{
  lArg->Indent(str);
  strcat(*str, "   ");
  *str = *str + 3;
  if(rArg->MyType() == LX_ADD || rArg->MyType() == LX_SUB) {
	sprintf(*str, "  ");
	*str = *str + 2;
  }
  rArg->Indent(str);

  if(unit == 'k') {
	sprintf(*str, "  ");
	*str = *str + 2;
	**str = '\0';
  }
}

void MultOp::Indent(char **str)
{
  if(lArg->MyType() == LX_ADD || lArg->MyType() == LX_SUB) {
	sprintf(*str, "  ");
	*str = *str + 2;
  }
  lArg->Indent(str);
  sprintf(*str, "   ");
  *str = *str + 3;
  if(rArg->MyType() == LX_ADD || rArg->MyType() == LX_SUB) {
	sprintf(*str, "  ");
	*str = *str + 2;
  }
  rArg->Indent(str);
  if(unit == 'k') {
	sprintf(*str, "  ");
	*str = *str + 2;
	**str = '\0';
  }
}

void DivOp::Indent(char **str)
{
  switch(lArg->MyType()) {
	case LX_ADD :
	case LX_SUB :
	case LX_DIV : sprintf(*str, "  ");
	              *str = *str + 2;
    default     : lArg->Indent(str);
  }
  sprintf(*str, "   ");
  *str = *str + 3;
  switch(rArg->MyType()) {
	case LX_ADD :
	case LX_SUB :
	case LX_MULT:
	case LX_DIV : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : lArg->Indent(str);
  }
  if(unit == 'k') {
	sprintf(*str, "  ");
	*str = *str + 2;
  }
}

void EqOp::Indent(char **str)
{
  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : lArg->Indent(str);
  }
  sprintf(*str, "    ");
  *str = *str + 4;
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : rArg->Indent(str);                  
  }
}

void NeqOp::Indent(char **str)
{
  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : lArg->Indent(str);
  }
  sprintf(*str, "    ");
  *str = *str + 4;
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : rArg->Indent(str);                  
  }
}

void GtOp::Indent(char **str)
{
  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : lArg->Indent(str);
  }
  sprintf(*str, "   ");
  *str = *str + 3;
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : rArg->Indent(str);                  
  }
}

void GeOp::Indent(char **str)
{
  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : lArg->Indent(str);
  }
  sprintf(*str, "    ");
  *str = *str + 4;
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : rArg->Indent(str);                  
  }
}

void LtOp::Indent(char **str)
{
  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : lArg->Indent(str);
  }
  sprintf(*str, "   ");
  *str = *str + 3;
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : rArg->Indent(str);                  
  }
}

void LeOp::Indent(char **str)
{
  int l, r;
  int n = 0;

  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : lArg->Indent(str);
  }
  sprintf(*str, "    ");
  *str = *str + 4;
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : sprintf(*str, "  ");
				  *str = *str + 2;
    default     : rArg->Indent(str);                  
  }
}

void AndOp::Indent(char **str)
{
  switch(lArg->MyType()) {
	case LX_OR : sprintf(*str, "  ");
				 *str = *str + 2;
    default    : lArg->Indent(str);
  }
  sprintf(*str, "    ");
  *str = *str + 4;
  switch(rArg->MyType()) {
	case LX_OR : sprintf(*str, "  ");
				 *str = *str + 2;
    default    : rArg->Indent(str);
  }
}

void OrOp::Indent(char **str)
{
  switch(lArg->MyType()) {
    case LX_AND : sprintf(*str, "  ");
 	 	  *str = *str + 2;
    default     : lArg->Indent(str);
  }
  sprintf(*str, "    ");
  *str = *str + 4;
  switch(rArg->MyType()) {
    case LX_AND : sprintf(*str, "  ");
		  *str = *str + 2;
    default     : rArg->Indent(str);
  }
}

void AssignOp::Indent(char **str)
{
  lArg->Indent(str);
  sprintf(*str, "   ");
  *str = *str + 3;
  rArg->Indent(str);
}

void AggEqOp::Indent(char **str)
{
    if(lArg->MyType() == LX_AGGEQ)
    {
	lArg->Indent(str);
    }
    else
    {
  	sprintf(*str, "  ");       // "()"
  	*str = *str + 2;
  	lArg->rArg->Indent(str);
    }
    if(rArg)
    {
  	sprintf(*str, "    ");     // " ?= "
  	*str = *str + 4;
	if(rArg->MyType() == LX_AGGEQ)
	{
	    rArg->Indent(str);
	}
	else
	{
  	    sprintf(*str, "  ");   // "()"
  	    *str = *str + 2;
  	    rArg->rArg->Indent(str);
	}
    }
}

void AggAddOp::Indent(char **str)
{
    if(lArg->MyType() == LX_AGGADD)
    {
	lArg->Indent(str);
    }
    else
    {
  	sprintf(*str, "  ");       // "()"
  	*str = *str + 2;
  	lArg->rArg->Indent(str);
    }
    if(rArg)
    {
  	sprintf(*str, "    ");     // " ?= "
  	*str = *str + 4;
	if(rArg->MyType() == LX_AGGADD)
	{
	    rArg->Indent(str);
	}
	else
	{
  	    sprintf(*str, "  ");   // "()"
  	    *str = *str + 2;
  	    rArg->rArg->Indent(str);
	}
    }
}

////////////////////////////////////////////////////////////////////////////////
// Return the length of an expresion.                                         //
////////////////////////////////////////////////////////////////////////////////

int Integer::MyLength()
{
  char tmp[100];

  sprintf(tmp, "%d", value);
  if(unit == 'k')
	return strlen(tmp) + 2;
  else
    return strlen(tmp);
}

int Float::MyLength()
{
  char tmp[100];

  sprintf(tmp, "%d", value);
  if(unit == 'k')
	return strlen(tmp) + 2;
  else
    return strlen(tmp);
}

int Boolean::MyLength()
{
    if(value)
	return 4;
    else
	return 5;
}

int Null::MyLength()
{
    return 4;
}

int String::MyLength()
{
    return strlen(value)+2;
}

int AddOp::MyLength()
{
  if(unit == 'k')
	return lArg->MyLength() + rArg->MyLength() + 3 + 2;
  else
	return lArg->MyLength() + rArg->MyLength() + 3;
}

int SubOp::MyLength()
{
  int n = 0;

  if(rArg->MyType() == LX_ADD || rArg->MyType() == LX_SUB)
	n = 2;
  
  if(unit == 'k')
	return lArg->MyLength() + rArg->MyLength() + 3 + n + 2;
  else
	return lArg->MyLength() + rArg->MyLength() + 3 + n;
}

int MultOp::MyLength()
{
  int n = 0;

  if(lArg->MyType() == LX_ADD || lArg->MyType() == LX_SUB)
    n = 2;
  if(rArg->MyType() == LX_ADD || rArg->MyType() == LX_SUB)
	n = n + 2;
  
  if(unit == 'k')
	return lArg->MyLength() + rArg->MyLength() + 3 + n + 2;
  else
	return lArg->MyLength() + rArg->MyLength() + 3 + n;
}

int DivOp::MyLength()
{
  int n = 0;

  switch(lArg->MyType()) {
	case LX_ADD :
	case LX_SUB :
	case LX_DIV : n = n + 2;
				  break;
  }
  switch(rArg->MyType()) {
	case LX_ADD :
	case LX_SUB :
	case LX_MULT:
	case LX_DIV : n = n + 2;
				  break;
  }
  if(unit == 'k')
	return lArg->MyLength() + rArg->MyLength() + 3 + n + 2;
  else
	return lArg->MyLength() + rArg->MyLength() + 3 + n;
}

int EqOp::MyLength()
{
  int n = 0;

  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = 2;
				  break;
  }
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = n + 2;
				  break;
  }
  return lArg->MyLength() + rArg->MyLength() + 4 + n;
}

int NeqOp::MyLength()
{
  int n = 0;

  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = 2;
				  break;
  }
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = n + 2;
				  break;
  }
  return lArg->MyLength() + rArg->MyLength() + 4 + n;
}

int GtOp::MyLength()
{
  int n = 0;

  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = 2;
				  break;
  }
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = n + 2;
				  break;
  }
  return lArg->MyLength() + rArg->MyLength() + 3 + n;
}

int GeOp::MyLength()
{
  int n = 0;

  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = 2;
				  break;
  }
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = n + 2;
				  break;
  }
  return lArg->MyLength() + rArg->MyLength() + 4 + n;
}

int LtOp::MyLength()
{
  int n = 0;

  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = 2;
				  break;
  }
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = n + 2;
				  break;
  }
  return lArg->MyLength() + rArg->MyLength() + 3 + n;
}

int LeOp::MyLength()
{
  int n = 0;

  switch(lArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = 2;
				  break;
  }
  switch(rArg->MyType()) {
	case LX_EQ  :
	case LX_NEQ :
	case LX_GT  :
	case LX_GE  :
	case LX_LT  :
	case LX_LE  : n = n + 2;
				  break;
  }
  return lArg->MyLength() + rArg->MyLength() + 4 + n;
}

int AndOp::MyLength()
{
  int n = 0;

  if(lArg->MyType() == LX_OR)
	n = n + 2;
  if(rArg->MyType() == LX_OR)
	n = n + 2;

  return lArg->MyLength() + rArg->MyLength() + 4 + n;
}

int OrOp::MyLength()
{
  int n = 0;

  if(lArg->MyType() == LX_AND)
	n = n + 2;
  if(rArg->MyType() == LX_AND)
	n = n + 2;

  return lArg->MyLength() + rArg->MyLength() + 4 + n;
}

int AssignOp::MyLength()
{
  return lArg->MyLength() + rArg->MyLength() + 3;
}

int AggEqOp::MyLength()
{
    int length = 0;

    if(lArg->MyType() != LX_ASSIGN)
    {
	length = length + lArg->MyLength();
    }
    else
    {
	length = length + 2 + lArg->rArg->MyLength();
    }

    if(rArg)
    {
	length = length + 4;
	if(rArg->MyType() != LX_ASSIGN)
	{
	    length = length + rArg->MyLength();
	}
	else
	{
	    length = length + 2 + rArg->rArg->MyLength();
	}
    }

    return length;
}

int AggAddOp::MyLength()
{
    int length = 0;

    if(lArg->MyType() != LX_ASSIGN)
    {
	length = length + lArg->MyLength();
    }
    else
    {
	length = length + 2 + lArg->rArg->MyLength();
    }

    if(rArg)
    {
	length = length + 3;
	if(rArg->MyType() != LX_ASSIGN)
	{
	    length = length + rArg->MyLength();
	}
	else
	{
	    length = length + 2 + rArg->rArg->MyLength();
	}
    }

    return length;
}

////////////////////////////////////////////////////////////////////////////////
// Analize the reason whether an expression will be satisfied by a classad.   //
////////////////////////////////////////////////////////////////////////////////

void AndOp::AnalTree(char* info, AttrListList* classads)
{
  EvalResult lval, rval;
  char* tmp1 = info;
  char* tmp2 = info;
  int tmp, i;
  
  AdvancePtr(tmp1);
  AdvancePtr(tmp2);
  lArg->EvalTree(classads, &lval);
  rArg->EvalTree(classads, &rval);

  if(lArg->MyType() == LX_OR) {
	*tmp1 = ' ';
	tmp1++;
  }
  if(lval.ValType() == LX_BOOL && !lval.b)
    lArg->AnalTree(info, classads);
  tmp1 = tmp1 + lArg->MyLength();
  AdvancePtr(tmp2);

  tmp = tmp2 - tmp1;
  if(lArg->MyType() == LX_OR)
    for(i = 5 - tmp; i > 0; i--) {
      *tmp2 = ' ';
      tmp2++;
    }
  else
    for(i = 4 - tmp; i > 0; i--) {
      *tmp2 = ' ';
      tmp2++;
    }

  if(rArg->MyType() == LX_OR) {
    *tmp2 = '\0';
    tmp2++;
  }
  if(rval.ValType() == LX_BOOL && !rval.b)
    rArg->AnalTree(info, classads);
  else
    rArg->Indent(&tmp2);
  if(rArg->MyType() == LX_OR)
    sprintf(tmp2, " \0");
}

void  OrOp::AnalTree(char* info, AttrListList* classads)
{
  EvalResult lval, rval;
  char* tmp1 = info;
  char* tmp2 = info;
  int  tmp, i;
  
  AdvancePtr(tmp1);
  AdvancePtr(tmp2);
  lArg->EvalTree(classads, &lval);
  rArg->EvalTree(classads, &rval);

  if(lval.ValType() == LX_BOOL && !lval.b &&
     rval.ValType() == LX_BOOL && !rval.b) {
    if(lArg->MyType() == LX_AND) {
      strcat(info, " ");
      tmp1++;
    }
    lArg->AnalTree(info, classads);
    AdvancePtr(tmp1);

    tmp2 = tmp2 + lArg->MyLength();
    tmp = tmp2 - tmp1;
    if(lArg->MyType() == LX_AND)
      for(i = 5 - tmp; i > 0; i--) {
        *tmp1 = ' ';
        tmp1++;
      }
    else
      for(i = 4 - tmp; i > 0; i--) {
        *tmp1 = ' ';
        tmp1++;
      }


    if(rArg->MyType() == LX_AND) {
      *tmp1 = '\0';
      tmp1++;
    }
    rArg->AnalTree(info, classads);
    if(rArg->MyType() == LX_AND)
      strcat(info, " ");

    Display();
    dprintf(D_NOHEADER | D_ALWAYS, "\n%s\n", info);
  }
}

void GtOp::AnalTree(char* info, AttrListList* classads)
{
  if(lArg->MyType() == LX_VARIABLE)
	lArg->AnalTree(info, classads);
  else if(rArg->MyType() == LX_VARIABLE)
	rArg->AnalTree(info, classads);
  else {
	int n = MyLength();
	for( ; n > 0; n--) strcat(info, "_");
  }
}

void GeOp::AnalTree(char* info, AttrListList* classads)
{
  if(lArg->MyType() == LX_VARIABLE)
	lArg->AnalTree(info, classads);
  else if(rArg->MyType() == LX_VARIABLE)
	rArg->AnalTree(info, classads);
  else {
	int n = MyLength();
	for( ; n > 0; n--) strcat(info, "_");
  }
}

void LtOp::AnalTree(char* info, AttrListList* classads)
{
  if(lArg->MyType() == LX_VARIABLE)
	lArg->AnalTree(info, classads);
  else if(rArg->MyType() == LX_VARIABLE)
	rArg->AnalTree(info, classads);
  else {
	int n = MyLength();
	for( ; n > 0; n--) strcat(info, "_");
  }
}

void LeOp::AnalTree(char* info, AttrListList* classads)
{
  if(lArg->MyType() == LX_VARIABLE)
	lArg->AnalTree(info, classads);
  else if(rArg->MyType() == LX_VARIABLE)
	rArg->AnalTree(info, classads);
  else {
	int n = MyLength();
	for( ; n > 0; n--) strcat(info, "_");
  }
}

void EqOp::AnalTree(char* info, AttrListList* classads)
{
  if(lArg->MyType() == LX_VARIABLE)
	lArg->AnalTree(info, classads);
  else if(rArg->MyType() == LX_VARIABLE)
	rArg->AnalTree(info, classads);
  else {
	int n = MyLength();
	for( ; n > 0; n--) strcat(info, "_");
  }
}

void NeqOp::AnalTree(char* info, AttrListList* classads)
{
  if(lArg->MyType() == LX_VARIABLE)
	lArg->AnalTree(info, classads);
  else if(rArg->MyType() == LX_VARIABLE)
	rArg->AnalTree(info, classads);
  else {
	int n = MyLength();
	for( ; n > 0; n--) strcat(info, "_");
  }
}

void Variable::AnalTree(char* info, AttrListList* classads)
{
  ExprTree* temp;
 
  temp = classads->Lookup(name);
  if(temp->MyType() == LX_STRING) {
    sprintf(info, "%s%s == \"", info, name);
    temp->PrintToStr(info);
	strcat(info, "\"");
  } else {
    sprintf(info, "%s%s == ", info, name);
    temp->PrintToStr(info);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Summarize the reason whether an expression can be satisfied in a pool of   //
// AttrLists.                     					      //
////////////////////////////////////////////////////////////////////////////////

void Variable::Summarize(StatTable* table)
{
  if(table->EntryType(name) == LX_INTEGER)
    dprintf(D_NOHEADER | D_ALWAYS, "The lArgest %s in the pool is %d\n", name, table->FindEntry(name)->IntVal());
  else if(table->EntryType(name) == LX_FLOAT)
    dprintf(D_NOHEADER | D_ALWAYS, "The lArgest %s in the pool is %f\n", name, table->FindEntry(name)->FloatVal());
  else {
    table->PrintEntry(name);
    dprintf(D_NOHEADER | D_ALWAYS, "\n");
  }
}

void EqOp::Summarize(StatTable* table)
{
  dprintf(D_NOHEADER | D_ALWAYS, "No machine satisfies '");
  Display();
  dprintf(D_NOHEADER | D_ALWAYS, "'\n");
  if(lArg->MyType() == LX_VARIABLE)
    lArg->Summarize(table);
  else if(rArg->MyType() == LX_VARIABLE)
    rArg->Summarize(table);
}

void NeqOp::Summarize(StatTable* table)
{
  dprintf(D_NOHEADER | D_ALWAYS, "No machine satisfies '");
  Display();
  dprintf(D_NOHEADER | D_ALWAYS, "'\n");
  if(lArg->MyType() == LX_VARIABLE)
    lArg->Summarize(table);
  else if(rArg->MyType() == LX_VARIABLE)
    rArg->Summarize(table);
}

void GtOp::Summarize(StatTable* table)
{
  dprintf(D_NOHEADER | D_ALWAYS, "No machine satisfies '");
  Display();
  dprintf(D_NOHEADER | D_ALWAYS, "'\n");
  if(lArg->MyType() == LX_VARIABLE)
    lArg->Summarize(table);
  else if(rArg->MyType() == LX_VARIABLE)
    rArg->Summarize(table);
}

void GeOp::Summarize(StatTable* table)
{
  dprintf(D_NOHEADER | D_ALWAYS, "No machine satisfies '");
  Display();
  dprintf(D_NOHEADER | D_ALWAYS, "'\n");
  if(lArg->MyType() == LX_VARIABLE)
    lArg->Summarize(table);
  else if(rArg->MyType() == LX_VARIABLE)
    rArg->Summarize(table);
}

void LtOp::Summarize(StatTable* table)
{
  dprintf(D_NOHEADER | D_ALWAYS, "No machine satisfies '");
  Display();
  dprintf(D_NOHEADER | D_ALWAYS, "'\n");
  if(lArg->MyType() == LX_VARIABLE)
    lArg->Summarize(table);
  else if(rArg->MyType() == LX_VARIABLE)
    rArg->Summarize(table);
}

void LeOp::Summarize(StatTable* table)
{
  dprintf(D_NOHEADER | D_ALWAYS, "No machine satisfies '");
  Display();
  dprintf(D_NOHEADER | D_ALWAYS, "'\n");
 if(lArg->MyType() == LX_VARIABLE)
    lArg->Summarize(table);
  else if(rArg->MyType() == LX_VARIABLE)
    rArg->Summarize(table);
}

void AndOp::Summarize(StatTable* table)
{
  if(lArg->sumFlag == TRUE && rArg->sumFlag == TRUE) {
    dprintf(D_NOHEADER | D_ALWAYS, "No machine satisfies the combination '");
    Display();
    dprintf(D_NOHEADER | D_ALWAYS, "'\n");
  }
  if(lArg->sumFlag == FALSE)
    lArg->Summarize(table);
  if(rArg->sumFlag == FALSE)
    rArg->Summarize(table);
}

void OrOp::Summarize(StatTable* table)
{
  lArg->Summarize(table);
  rArg->Summarize(table);
}
*/

////////////////////////////////////////////////////////////////////////////////
// The MinTree() functions returns a minimum part of an expression which      //
// contribute to the TRUE result of the expression.                           //
////////////////////////////////////////////////////////////////////////////////

ExprTree* Variable::MinTree(AttrListList* classads)
{
  ExprTree* tmpExpr;

  if(!(tmpExpr = classads->Lookup(name)))
    return NULL;
  else
    return tmpExpr->MinTree(classads);
}

ExprTree* AndOp::MinTree(AttrListList* classads)
{
  ExprTree* l,* r;

  if(!(l = lArg->MinTree(classads)))
  {
    return NULL;
  }
  if(!(r = rArg->MinTree(classads)))
  {
    return NULL;
  }
  return new AndOp(l, r);
}

ExprTree* OrOp::MinTree(AttrListList* classads)
{
  ExprTree* l, *r;

  if(l = lArg->MinTree(classads))
  {
    return l;
  }
  if(r = rArg->MinTree(classads))
  {
    return r;
  }
  return NULL;
}

ExprTree* EqOp::MinTree(AttrListList* classads)
{
  EvalResult tmpFactor;

  EvalTree(classads, &tmpFactor);
  if(tmpFactor.b)
  {
    Copy();
    return this;
  }
  return NULL;
}

ExprTree* NeqOp::MinTree(AttrListList *classads)
{
  EvalResult tmpFactor;

  EvalTree(classads, &tmpFactor);
  if(tmpFactor.b)
  {
    Copy();
    return this;
  }
  return NULL;
}

ExprTree* GtOp::MinTree(AttrListList* classads)
{
  EvalResult tmpFactor;

  EvalTree(classads, &tmpFactor);
  if(tmpFactor.b)
  {
    Copy();
    return this;
  }
  return NULL;
}

ExprTree* GeOp::MinTree(AttrListList *classads)
{
  EvalResult tmpFactor;

  EvalTree(classads, &tmpFactor);
  if(tmpFactor.b)
  {
    Copy();
    return this;
  }
  return NULL;
}

ExprTree* LtOp::MinTree(AttrListList* classads)
{
  EvalResult tmpFactor;

  EvalTree(classads, &tmpFactor);
  if(tmpFactor.b)
  {
    Copy();
    return this;
  }
  return NULL;
}

ExprTree* LeOp::MinTree(AttrListList* classads)
{
  EvalResult tmpFactor;

  EvalTree(classads, &tmpFactor);
  if(tmpFactor.b)
  {
    Copy();
    return this;
  }
  return NULL;
}

ExprTree* AggEqOp::MinTree(AttrListList* classads)
{
    EvalResult tmpFactor;

    EvalTree(classads, &tmpFactor);
    if(tmpFactor.type == LX_NULL)
    {
	return NULL;
    }
    return this;
}

ExprTree* AssignOp::MinTree(AttrListList* classads)
{
    ExprTree*	minTree;

    minTree = rArg->MinTree(classads);
    if(minTree)
    {
	lArg->Copy();
	minTree = new AssignOp(lArg, minTree);
	return minTree;
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Subtracts and expression from a AttrList list.           		      //
////////////////////////////////////////////////////////////////////////////////

void AndOp::Diff(AttrListList* classads, VarTypeTable* table)
{
  lArg->Diff(classads, table);
  rArg->Diff(classads, table);
}

void Variable::Diff(AttrListList* classads, VarTypeTable* table)
{
  classads->Lookup(name)->Diff(classads, table);
}

void EqOp::Diff(AttrListList* classads, VarTypeTable* table)
{
  ExprTree* 	tmpExpr;
  ExprTree* 	newExpr;
  AttrList*	tmpClassAd;
  char*		name;

  if(lArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)lArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      rArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), rArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  else if(rArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)rArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      lArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), lArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  delete []name;
}

void NeqOp::Diff(AttrListList* classads, VarTypeTable* table)
{
  ExprTree* 	tmpExpr;
  ExprTree* 	newExpr;
  AttrList*	tmpClassAd;
  char*		name;

  if(lArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)lArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      rArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), rArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  else if(rArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)rArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      lArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), lArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  delete []name;
}

void GtOp::Diff(AttrListList* classads, VarTypeTable* table)
{
  ExprTree* 	tmpExpr;
  ExprTree* 	newExpr;
  AttrList*	tmpClassAd;
  char*		name;

  if(lArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)lArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      rArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), rArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  else if(rArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)rArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      lArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), lArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  delete []name;
}

void GeOp::Diff(AttrListList* classads, VarTypeTable* table)
{
  ExprTree* 	tmpExpr;
  ExprTree* 	newExpr;
  AttrList*	tmpClassAd;
  char*		name;

  if(lArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)lArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      rArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), rArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  else if(rArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)rArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      lArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), lArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  delete []name;
}

void LtOp::Diff(AttrListList* classads, VarTypeTable* table)
{
  ExprTree* 	tmpExpr;
  ExprTree* 	newExpr;
  AttrList*	tmpClassAd;
  char*		name;

  if(lArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)lArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      rArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), rArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  else if(rArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)rArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      lArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), lArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  delete []name;
}

void LeOp::Diff(AttrListList* classads, VarTypeTable* table)
{
  ExprTree* 	tmpExpr;
  ExprTree* 	newExpr;
  AttrList*	tmpClassAd;
  char*		name;

  if(lArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)lArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      rArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), rArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  else if(rArg->MyType() == LX_VARIABLE)
  {
    name = ((Variable*)rArg)->Name();
    if(table->FindType(name) == RANGE)
    {
      tmpExpr = classads->Lookup(name, tmpClassAd);
      if(!tmpExpr)
      {
	return;
      }
      tmpExpr->LArg()->Copy();
      tmpExpr->RArg()->Copy();
      lArg->Copy();
      newExpr = new SubOp(tmpExpr->RArg(), lArg);
      newExpr = new AssignOp(tmpExpr->LArg(), newExpr);
      tmpClassAd->Delete(name);
      tmpClassAd->Insert(newExpr);
      delete newExpr;
    }
  }
  delete []name;
}

void AssignOp::Diff(AttrListList* classads, VarTypeTable* table)
{
    rArg->Diff(classads, table);
}

void AggEqOp::Diff(AttrListList* classads, VarTypeTable* table)
{
    ExprTree*	tmpTree;	// used to find the expression to be changed

    for(tmpTree = rArg; tmpTree->MyType() != LX_ASSIGN && tmpTree; tmpTree = tmpTree->RArg());
    if(tmpTree)
    {
	tmpTree->Diff(classads, table);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Delete a descendant from an aggregate tree.                                //
////////////////////////////////////////////////////////////////////////////////

int AggOp::DeleteChild(AssignOp* expr)
{
    int 	deletedL = FALSE; // flags to indicate whether "expr" is deleted
    int 	deletedR = FALSE; //
    ExprTree* 	tmp; 		  // used when deleting a branch

    if(!lArg && !rArg)
    {
	return FALSE;
    }
    if(lArg && lArg->MyType() == LX_ASSIGN && lArg == expr)
    {
	delete lArg;
	lArg = rArg;
	rArg = NULL;
	return TRUE;
    }
    else if(rArg && rArg->MyType() == LX_ASSIGN && rArg == expr)
    {
	delete rArg;
	rArg = NULL;
	return TRUE;
    }
    if(lArg && lArg->MyType() != LX_ASSIGN && (deletedL = ((AggOp* )lArg)->DeleteChild(expr)))
    {
	if(!lArg->LArg() && !lArg->RArg())
	{
	    delete lArg;
	    lArg = rArg;
	    rArg = NULL;
	}
	else if(!lArg->RArg())
	{
	    tmp = lArg;
	    lArg = lArg->LArg();
	    ((BinaryOpBase*)tmp)->lArg = NULL;
	    delete tmp;
	}
	return TRUE;
    }
    if(rArg && rArg->MyType() != LX_ASSIGN && (deletedR = ((AggOp* )rArg)->DeleteChild(expr)))
    {
        if(!rArg->LArg() && !rArg->RArg())
	{
	    delete rArg;
	    rArg = NULL;
	}
	else if(!rArg->RArg())
	{
	    tmp = rArg;
	    rArg = rArg->LArg();
	    ((BinaryOpBase*)tmp)->lArg = NULL;
	    delete tmp;
	}
	return TRUE;
    }
    return FALSE;
}

// used by accountant
int EqOp::RequireClass(ExprTree* tree)
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

int NeqOp::RequireClass(ExprTree* tree)
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

int GtOp::RequireClass(ExprTree* tree)
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
		tmpTree = new LtOp(rArg, lArg);
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
		tmpTree = new GtOp(tree->RArg(), tree->LArg());
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

int GeOp::RequireClass(ExprTree* tree)
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
		tmpTree = new LtOp(rArg, lArg);
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
		tmpTree = new GtOp(tree->RArg(), tree->LArg());
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

int LtOp::RequireClass(ExprTree* tree)
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
		tmpTree = new LtOp(rArg, lArg);
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
		tmpTree = new LtOp(tree->RArg(), tree->LArg());
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

int LeOp::RequireClass(ExprTree* tree)
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
		tmpTree = new LtOp(rArg, lArg);
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
		tmpTree = new LtOp(tree->RArg(), tree->LArg());
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

int AndOp::RequireClass(ExprTree* tree)
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

int OrOp::RequireClass(ExprTree* tree)
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

int	AssignOp::RequireClass(ExprTree* tree)
{
	return rArg->RequireClass(tree);
}
