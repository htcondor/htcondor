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

extern void evalFromEnvironment (const char *, EvalResult *);
static void printComparisonOpToStr (char *, ExprTree *, ExprTree *, char *);


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


//------tw-----------------

int Variable::
_EvalTree(AttrList* my_classad,AttrList* req_classad, EvalResult* val)
{
    ExprTree* tmp = NULL;
    
	// sanity check: fail evaluation if no variable exists
    if(!val) return FALSE;

    char * myNamePrefix = "MY.";
    char * reqNamePrefix = "TARGET.";
	char * envNamePrefix = "ENV.";

    if (name == NULL)
    {
        cout << "no name provided!!"<< endl;
        exit(1);                  
    }
   
	// check for scope of the variable.  semantics state that if a variable's
  	// scope is not defined, first look in the MY scope.  If the variable is
	// not found in the MY scope, look in the TARGET scope.  If not found in
	// target scope, lookup environment scope.  If not found in all three, 
	// return LX_UNDEFINED    --RR
	char *realName = new char [strlen (name) + 1];
	bool myScope, targetScope, envScope;

	if (strncmp(name, myNamePrefix, 3) == 0)
	{
		myScope = true;
		targetScope = false;
		envScope = false;
		strcpy (realName, name+3);
	}
	else
	if (strncmp (name, reqNamePrefix, 7) == 0)
	{	
		targetScope = true;
		myScope = false;
		envScope = false;
		strcpy (realName, name+7);
	}
	else
	if (strncmp (name, envNamePrefix, 4) == 0)
	{
		envScope = true;
		targetScope = false;
		myScope = false;
	}
	else
	{
		// not prefixed by MY, TARGET or ENV; must lookup all scopes
		myScope = true;
		targetScope = true;
		envScope = true;
		strcpy (realName, name);
	}

   	if (myScope && my_classad)
   	{
		// lookup my scope
       	tmp = my_classad->Lookup(realName);
		if (tmp)
		{
			delete [] realName;
			return (tmp->EvalTree(my_classad, req_classad, val));
		}
	}

	if (targetScope && req_classad)
	{
		// lookup target scope
		tmp = req_classad->Lookup(realName);
		if (tmp)
        {
			delete [] realName;
        	return (tmp->EvalTree(req_classad, my_classad, val));
		}
	}

	if (envScope)
	{
		// lookup environment scope
		evalFromEnvironment (realName, val);
		delete [] realName;
		return TRUE;	
	}

	// should never reach here  [But it seems that we do. -Derek 11/21/97]
	val->type = LX_UNDEFINED;	
	delete [] realName;
	return TRUE;
}


//---------------------------

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

void Boolean::PrintToStr(char* str)
{
  sprintf(str, value ? "%sTRUE" : "%sFALSE", str, str);
}

void Undefined::PrintToStr(char* str)
{
	sprintf(str, "%sUNDEFINED", str);
}

void Error::PrintToStr(char* str)
{
	sprintf(str, "%sERROR", str);
}

void AddOp::PrintToStr(char* str)
{
    if(lArg)
    {
  		switch(lArg->MyType())
		{
			case LX_ADD:
			case LX_SUB:
			case LX_MULT:
			case LX_DIV : 
			case LX_VARIABLE:
			case LX_BOOL:
			case LX_INTEGER:
			case LX_FLOAT:
			case LX_UNDEFINED:
			case LX_ERROR:
			case LX_STRING:
				((ExprTree*)lArg)->PrintToStr(str);
				break;

			default:
				strcat(str, "(");
				((ExprTree*)lArg)->PrintToStr(str);
				strcat(str, ")");
    	}
    }
    strcat(str, " + ");
    if(rArg)
    {
  		switch(rArg->MyType())
		{
			case LX_ADD:
			case LX_SUB:
			case LX_MULT:
			case LX_DIV : 
			case LX_VARIABLE:
			case LX_BOOL:
			case LX_INTEGER:
			case LX_FLOAT:
			case LX_UNDEFINED:
			case LX_ERROR:
			case LX_STRING:
				((ExprTree*)rArg)->PrintToStr(str);
				break;

			default:
				strcat(str, "(");
				((ExprTree*)rArg)->PrintToStr(str);
				strcat(str, ")");
    	}
    }
    if(unit == 'k') strcat(str, " k");
}

void SubOp::PrintToStr(char* str)
{
    if(lArg)
    {
  		switch(lArg->MyType())
		{
			case LX_ADD:
			case LX_SUB:
			case LX_MULT:
			case LX_DIV : 
			case LX_VARIABLE:
			case LX_BOOL:
			case LX_INTEGER:
			case LX_FLOAT:
			case LX_UNDEFINED:
			case LX_ERROR:
			case LX_STRING:
				((ExprTree*)lArg)->PrintToStr(str);
				break;

			default:
				strcat(str, "(");
				((ExprTree*)lArg)->PrintToStr(str);
				strcat(str, ")");
    	}
    }
    strcat(str, " - ");
    if(rArg)
    {
		int rt = rArg->MyType();

		// ugly hack:  in case of unary minus, we should force parenthesization
		// if the expression is anything but a constant or a variable.
		if (!lArg) 
		{
			switch (rt)
			{
				case LX_VARIABLE:
				case LX_INTEGER:
				case LX_FLOAT:
				case LX_STRING:
				case LX_UNDEFINED:
				case LX_ERROR:
				case LX_BOOL:
					rt = LX_ADD;	// force no parenthesization
					break;

				default:
					rt = LX_AND;	// force parenthesization
					break;
			}
		}

  		switch(rt)
		{
			case LX_ADD:
			case LX_SUB:
			case LX_MULT:
			case LX_DIV : 
			case LX_VARIABLE:
			case LX_BOOL:
			case LX_INTEGER:
			case LX_FLOAT:
			case LX_UNDEFINED:
			case LX_ERROR:
			case LX_STRING:
				((ExprTree*)rArg)->PrintToStr(str);
				break;

			default:
				strcat(str, "(");
				((ExprTree*)rArg)->PrintToStr(str);
				strcat(str, ")");
    	}
    }
    if(unit == 'k') strcat(str, " k"); 
}


void MultOp::PrintToStr(char* str)
{
    if(lArg)
    {
  		switch(lArg->MyType())
		{
			case LX_MULT:
			case LX_DIV : 
			case LX_VARIABLE:
			case LX_BOOL:
			case LX_INTEGER:
			case LX_FLOAT:
			case LX_UNDEFINED:
			case LX_ERROR:
			case LX_STRING:
				((ExprTree*)lArg)->PrintToStr(str);
				break;

			default:
				strcat(str, "(");
				((ExprTree*)lArg)->PrintToStr(str);
				strcat(str, ")");
    	}
    }
    strcat(str, " * ");
    if(rArg)
    {
  		switch(rArg->MyType())
		{
			case LX_MULT:
			case LX_DIV : 
			case LX_VARIABLE:
			case LX_BOOL:
			case LX_INTEGER:
			case LX_FLOAT:
			case LX_UNDEFINED:
			case LX_ERROR:
			case LX_STRING:
				((ExprTree*)rArg)->PrintToStr(str);
				break;

			default:
				strcat(str, "(");
				((ExprTree*)rArg)->PrintToStr(str);
				strcat(str, ")");
    	}
    }
    if(unit == 'k') strcat(str, " k");
}

void DivOp::PrintToStr(char* str)
{
    if(lArg)
    {
  		switch(lArg->MyType())
		{
			case LX_MULT:
			case LX_DIV : 
			case LX_VARIABLE:
			case LX_BOOL:
			case LX_INTEGER:
			case LX_FLOAT:
			case LX_UNDEFINED:
			case LX_ERROR:
			case LX_STRING:
				((ExprTree*)lArg)->PrintToStr(str);
				break;

			default:
				strcat(str, "(");
				((ExprTree*)lArg)->PrintToStr(str);
				strcat(str, ")");
    	}
    }
    strcat(str, " / ");
    if(rArg)
    {
  		switch(rArg->MyType())
		{
			case LX_MULT:
			case LX_DIV : 
			case LX_VARIABLE:
			case LX_BOOL:
			case LX_INTEGER:
			case LX_FLOAT:
			case LX_UNDEFINED:
			case LX_STRING:
			case LX_ERROR:
				((ExprTree*)rArg)->PrintToStr(str);
				break;

			default:
				strcat(str, "(");
				((ExprTree*)rArg)->PrintToStr(str);
				strcat(str, ")");
    	}
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
    if(lArg)
  	switch(lArg->MyType()) {
		case LX_VARIABLE:
	    case LX_AND :
		case LX_OR:
	    case LX_INTEGER :
	    case LX_FLOAT :
		case LX_ERROR:
		case LX_UNDEFINED:
	    case LX_STRING :
	    case LX_BOOL : ((ExprTree*)lArg)->PrintToStr(str);
			   break;
     	    default : strcat(str, "(");
		      ((ExprTree*)lArg)->PrintToStr(str);
		      strcat(str, ")");
    	}
    strcat(str, " && ");
    if(rArg)
  	switch(rArg->MyType()) {
		case LX_VARIABLE:
	    case LX_AND :
		case LX_OR:
	    case LX_INTEGER :
	    case LX_FLOAT :
		case LX_ERROR:
		case LX_UNDEFINED:
	    case LX_STRING :
	    case LX_BOOL : ((ExprTree*)rArg)->PrintToStr(str);
			   break;
    	    default : strcat(str, "(");
	 	      ((ExprTree*)rArg)->PrintToStr(str);
		      strcat(str, ")");
  	}
}

void OrOp::PrintToStr(char* str)
{
    if(lArg)
  	switch(lArg->MyType()) {
		case LX_VARIABLE:
	    case LX_AND :
		case LX_OR:
	    case LX_INTEGER :
	    case LX_FLOAT :
		case LX_ERROR:
		case LX_UNDEFINED:
	    case LX_STRING :
	    case LX_BOOL : ((ExprTree*)lArg)->PrintToStr(str);
			   break;
     	    default : strcat(str, "(");
		      ((ExprTree*)lArg)->PrintToStr(str);
		      strcat(str, ")");
    	}
    strcat(str, " || ");
    if(rArg)
  	switch(rArg->MyType()) {
		case LX_VARIABLE:
	    case LX_AND :
		case LX_OR:
	    case LX_INTEGER :
	    case LX_FLOAT :
		case LX_ERROR:
		case LX_UNDEFINED:
	    case LX_STRING :
	    case LX_BOOL : ((ExprTree*)rArg)->PrintToStr(str);
			   break;
    	    default : strcat(str, "(");
	 	      ((ExprTree*)rArg)->PrintToStr(str);
		      strcat(str, ")");
  	}
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
    if(lArg)
    {
    	switch(lArg->MyType())
		{
			case LX_AND:
			case LX_OR:
				strcat(str, "(");
			  	((ExprTree*)lArg)->PrintToStr(str);
			  	strcat(str, ")");
			  	break;

	    	default:      
				((ExprTree*)lArg)->PrintToStr(str);
   		}
    }
    strcat(str, op);
    if(rArg)
    {
    	switch(rArg->MyType())
		{
			case LX_AND:
			case LX_OR:
				strcat(str, "(");
			  	((ExprTree*)rArg)->PrintToStr(str);
			  	strcat(str, ")");
			  	break;

	    	default:      
				((ExprTree*)rArg)->PrintToStr(str);
   		}
    }
}
