//******************************************************************************
// AttrList.C
//
// Implementation of AttrList classes and AttrListList classes.
//
//******************************************************************************

# include <std.h>
# include <ctype.h>
# include <assert.h>
# include <string.h>

# include "except.h"
# include "condor_ast.h"
# include "condor_registration.h"
# include "condor_expressions.h"
# include "condor_attrlist.h"
# include "condor_classad.h"
# include "condor_parser.h"

static Registration regi;                   // this is the registration for 
                                            // the AttrList type names. It 
                                            // should be defined in the calling
                                            // procedure.
static char *_FileName_ = __FILE__;         // Used by EXCEPT (see except.h)
extern "C" int _EXCEPT_(char*, ...);
extern "C" int xdr_mywrapstring (XDR *, char **);

//
// AdType Constructor.
//
AdType::AdType(char *tempName)
{
    if(tempName == NULL)
    {                                       // if empty.
        name = new char[strlen("") + 1];
	if(!name)
        {
            cerr << "Warning : you ran out of memory -- quitting !" << endl;
	    exit(1);
        }
	strcpy(name, "");
	number = -1;
    }
    else
    {
        name = new char[strlen(tempName) + 1];
	if(!name)
        {
            cerr << "Warning : you ran out of memory -- quitting !" << endl;
	    exit(1);
        }
	strcpy(name, tempName);
	number = regi.RegisterType(tempName);
    }   
}

//
// AdType Destructor. 
//
AdType::~AdType()
{
    if(name)
    {
        delete []name;
    }
}

//
// ClassAd constructors
//
ClassAd::ClassAd() : AttrList()
{
	myType = NULL;
	targetType = NULL;
}

ClassAd::ClassAd(class ProcObj* procObj) : AttrList(procObj)
{
	myType = NULL;
	targetType = NULL;
}

ClassAd::ClassAd(const CONTEXT* context) : AttrList((CONTEXT *) context)
{
	myType = NULL;
	targetType = NULL;
}

ClassAd::ClassAd(FILE* f, char* d, int& i) : AttrList(f, d, i)
{
    ExprTree *tree;
    EvalResult *val;

	myType = NULL;
	targetType = NULL;
	val = new EvalResult;
    tree = Lookup("MyType");
	if(!tree)
    {
        myType = new AdType();               // undefined type.
        if(myType == NULL)
        {
            EXCEPT("Warning : you ran out of space");
		}
    }
    else
    {
		tree->EvalTree(this, val);
		myType = new AdType(val->s);
		if(myType == NULL)
		{
            EXCEPT("Warning : you ran out of space");
		}
    }

	if(!(tree = Lookup("TargetType")))
    {
        targetType = new AdType();           // undefined type.
		if(targetType == NULL)
		{
            EXCEPT("Warning : you ran out of space");
		}
    }
    else
    {
		tree->EvalTree(this, val);
		targetType = new AdType(val->s);
		if(targetType == NULL)
		{
            EXCEPT("Warning : you ran out of space");
		}
    }

    if(val)
    {
        delete val;
    }
    Delete("MyType");                        // eliminate redundant storage.
    Delete("TargetType");
}

ClassAd::ClassAd(char* s, char d) : AttrList(s, d)
{
    ExprTree *tree;
    EvalResult *val;

	myType = NULL;
	targetType = NULL;
    val = new EvalResult;
    if(val == NULL)
    {
        cerr << "Warning : you ran out of space -- quitting !" << endl;
        exit(1);
    }

    Parse("MyType", tree);                   // set myType field by evaluation
    tree->EvalTree(this, val);               // over itself.
    if(!val || val->type!=LX_STRING)
    {
        myType = new AdType();               // undefined type.
        if(myType == NULL)
        {
            EXCEPT("Warning : you ran out of space");
		}
    }
    else
    {
		myType = new AdType(val->s);
		if(myType == NULL)
		{
				EXCEPT("Warning : you ran out of space");
		}
    }
    delete tree;

    Parse("TargetType", tree);               // set targetType field by
                                             // evaluation over itself.
    tree->EvalTree(this, val);
    if(!val || val->type!=LX_STRING)
    {
        targetType = new AdType();           // undefined type.
		if(targetType == NULL)
		{
			EXCEPT("Warning : you ran out of space");
		}
    }
    else
    {
		targetType = new AdType(val->s);
		if(targetType == NULL)
		{
			EXCEPT("Warning : you ran out of space");
		}
    }
    delete tree;

    if(val)
    {
        delete val;
    }
    Delete("MyType");                        // eliminate redundant storage.
    Delete("TargetType");
}

ClassAd::ClassAd(const ClassAd& old) : AttrList((AttrList&) old)
{
	myType = NULL;
	targetType = NULL;
    if(old.myType)
    {
        this->myType = new AdType(old.myType->name);
		if(this->myType == NULL)
		{
			EXCEPT("Warning : you ran out of meomory");
		}
    }
    if(old.targetType)
    {
        this->targetType = new AdType(old.targetType->name);
		if(this->targetType == NULL)
		{
			EXCEPT("Warning : you ran out of meomory");
		}
	}
}

ClassAd::~ClassAd()
{
    AttrListElem* tmp;

    for(tmp = exprList; tmp; tmp = exprList)
    {
        exprList = exprList->next;
        delete tmp;
    }
    if(associatedList)
    {
	associatedList->associatedAttrLists->Delete(this);
    }
    if(myType)
    {
        delete myType;
    }
    if(targetType)
    {
        delete targetType;
    }
}

//
// This member function of class AttrList sets myType name.
//
void ClassAd::SetMyTypeName(char *tempName)
{
    if(myType)
    {
        delete myType;
    }
    myType = new AdType(tempName);
    if(!myType)
    {
        cerr << "Warning : you ran out of memory -- quitting !" << endl;
        exit(1);
    }
}

//
// This member function of class AttrList returns myType name.
//
char *ClassAd::GetMyTypeName()
{
    if(!myType)
    {
        char *temp = "";                      // undefined type.
        return temp;
    }
    else
    {
        return myType->name;
    }
}

//
// This member function of class AttrList sets targetType name.
//
void ClassAd::SetTargetTypeName(char *tempName)
{
    if(targetType)
    {
        delete targetType;
    }
    targetType = new AdType(tempName);
    if(!targetType)
    {
        cerr << "Warning : you ran out of memory -- quitting !" << endl;
        exit(1);
    }
}

//
// This member function of class AttrList returns targetType name.
//
char *ClassAd::GetTargetTypeName()
{
    if(!targetType)
    {
        char *temp = "";                      // undefined.
	return temp;
    }
    else
    {
        return targetType->name;
    }
}

//
// This member function of class AttrList returns myType number.
//
int ClassAd::GetMyTypeNumber()
{
    if(!myType)
    {
        return -1;                            // undefined type.
    }
    else
    {
        return myType->number;
    }
}

//
// This member function of class AttrList returns targetType number.
//
int ClassAd::GetTargetTypeNumber()
{
    if(!targetType)
    {
        return -1;                            // undefined type.
    }
    else
    {
        return targetType->number;
    }
}

//
// This member function tests whether two ClassAds match mutually.
//
int ClassAd::IsAMatch(ClassAd* temp)
{
    ExprTree *tree;
    EvalResult *val;

    if(!temp)
    {
        return 0;
    }
  
    if((GetMyTypeNumber() != temp->GetTargetTypeNumber()) ||
       (GetTargetTypeNumber() !=  temp->GetMyTypeNumber()))
    { 
        return 0;                            // types don't match.
    }

    val = new EvalResult;
    if(val == NULL)
    {
        cerr << "Warning : you ran out of memory -- quitting !" << endl;
        exit(1);
    }

    Parse("MY.Requirement", tree);           // convention.

    tree->EvalTree(this, temp, val);         // match in one direction.
    if(!val || val->type != LX_BOOL)
    {
        delete tree;
	delete val;
	return 0;
    }
    else
    {
      if(!val->b)
      {
	  delete tree;
	  delete val;
	  return 0; 
      }
    }

    tree->EvalTree(temp, this, val);         // match in the other direction.
    if(!val || val->type != LX_BOOL)
    {
        delete tree;
	delete val;
	return 0;
    }
    else
    {
        if(!val->b)
	{
	    delete tree;
	    delete val;
	    return 0; 
        }
    }  

    delete tree;
    delete val;
    return 1;   
}

bool operator== (ClassAd &lhs, ClassAd &rhs)
{
	return (lhs >= rhs && rhs >= lhs);
}


bool operator<= (ClassAd &lhs, ClassAd &rhs)
{
	return (rhs >= lhs);
}


bool operator>= (ClassAd &lhs, ClassAd &rhs)
{
	ExprTree *tree;
	EvalResult *val;	
	
	if (lhs.GetMyTypeNumber() != rhs.GetTargetTypeNumber())
		return false;

	if ((val = new EvalResult) == NULL)
	{
		cerr << "Out of memory -- quitting" << endl;
		exit (1);
	}

	Parse ("MY.Requirement", tree);
	tree -> EvalTree (&rhs, &lhs, val);
	if (!val || val->type != LX_BOOL)
	{
		delete tree;	
		delete val;
		return false;
	}
	else
	if (!val->b)
	{
		delete tree;	
		delete val;
		return false;
	}

	delete tree;
	delete val;
	return true;
}

	
////////////////////////////////////////////////////////////////////////////////
// print the whole ClassAd into a file. The expressions are in infix notation.
// Returns FALSE if the file pointer is NULL; TRUE otherwise.
////////////////////////////////////////////////////////////////////////////////
int ClassAd::fPrint(FILE* f)
{
	if(!f)
	{
		return FALSE;
	}
    fprintf(f, "MyType = ");
    fprintf(f, "%c", '"');
	if(GetMyTypeName())
	{
		fprintf(f, "%s", GetMyTypeName());
	}
    fprintf(f, "%c\n", '"');
    fprintf(f, "TargetType = ");
    fprintf(f, "%c", '"');
	if(GetMyTypeName())
	{
		fprintf(f, "%s", GetTargetTypeName());
	}
    fprintf(f, "%c\n", '"');    
	
	return AttrList::fPrint(f);
}

// shipping functions for ClassAd -- added by Lei Cao

int ClassAd::put(Stream& s)
{
    AttrListElem*   elem;
    char*           line;
    int             numExprs = 0;

    //get the number of expressions
    for(elem = exprList; elem; elem = elem->next)
        numExprs++;

    s.encode();

    if(!s.code(numExprs))
        return 0;

    line = new char[200];
    for(elem = exprList; elem; elem = elem->next) {
        strcpy(line, "");
        elem->tree->PrintToStr(line);
        if(!s.code(line)) {
            delete [] line;
            return 0;
        }
    }
    delete [] line;

    if(!s.code(myType->name))
        return 0;
    if(!s.code(targetType->name))
        return 0;

    return 1;
}

int ClassAd::get(Stream& s)
{
    ExprTree*       tree;
    char*           name;
    char*           line;
    int             numExprs;

    exprList       = NULL;
    associatedList = NULL;
    tail           = NULL;
    ptrExpr        = NULL;
    ptrName        = NULL;
    myType         = NULL;
    targetType     = NULL;

    s.decode();

    if(!s.code(numExprs)) 
        return 0;
    
    line = new char[200];
    for(int i = 0; i < numExprs; i++) {
        if(!s.code(line)) {
            delete [] line;
            return 0;
        }
        
        if(!Parse(line, tree)) {
            if(tree->MyType() == LX_ERROR) {
                cerr << "Parse error in the incomming stream -- quitting !"
					 << endl;
                delete [] line;
                exit(1);
            }
        }
        else {
            cerr << "Parse error in the incomming stream -- quitting !" << endl;
			delete [] line;
            exit(1);
        }
        
        Insert(tree);
        strcpy(line, "");
        delete tree;
    }
    delete [] line;

    name = new char[50];
    if(!s.code(name)) {
        delete [] name;
        return 0;
    }
    SetMyTypeName(name);
    delete [] name;

    name = new char[50];
    if(!s.code(name)) {
        delete [] name;
        return 0;
    }
    SetTargetTypeName(name);
    delete [] name;

    return 1; 
}

int ClassAd::code(Stream& s)                                           
{
    if(s.is_encode())
        return put(s);
    else
        return get(s);
}

int ClassAd::put (XDR *xdrs)
{
	xdrs->x_op = XDR_ENCODE;

	if (!AttrList::put (xdrs))
		return 0;

	if (!xdr_mywrapstring (xdrs, &myType->name))
		return 0;

	if (!xdr_mywrapstring (xdrs, &targetType->name))
		return 0;

	return 1;
}


int ClassAd::get (XDR *xdrs)
{
	char buf[100];
	char *line = buf;

	if (!line) return 0;

	xdrs->x_op = XDR_DECODE;

	if (!AttrList::get (xdrs)) 
		return 0;

	if (!xdr_mywrapstring (xdrs, &line)) 
		return 0;

	SetMyTypeName (line);

	if (!xdr_mywrapstring (xdrs, &line)) 
		return 0;

	SetTargetTypeName (line);

	return 1;
}
