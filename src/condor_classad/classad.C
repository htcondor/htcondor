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
# include "ast.h"
# include "registration.h"
# include "condor_expressions.h"
# include "attrlist.h"
# include "classad.h"
# include "parser.h"

static Registration regi;                   // this is the registration for 
                                            // the AttrList type names. It 
                                            // should be defined in the calling
                                            // procedure.
static char *_FileName_ = __FILE__;         // Used by EXCEPT (see except.h)
extern "C" int _EXCEPT_(char*, ...);

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
ClassAd::ClassAd() : AttrList();

ClassAd::ClassAd(ProcObj* procObj) : AttrList(procObj);

ClassAd::ClassAd(CONTEXT* context) : AttrList(context);

ClassAd::ClassAd(FILE* f, char* d, int& i) : AttrList(f, d, i)
{
    ExprTree *tree;
    EvalResult *val;

	myType = NULL;
	targetType = NULL;
	val = new EvalResult;
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

    if(buffer)
    {
        delete []buffer;
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

    if(buffer)
    {
        delete []buffer;
    }

    if(val)
    {
        delete val;
    }
    Delete("MyType");                        // eliminate redundant storage.
    Delete("TargetType");
}

ClassAd::ClassAd(const ClassAd& old) : AttrList(old)
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

ClassAd::~ClassAd()
{
	AttrList::~AttrList();
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

////////////////////////////////////////////////////////////////////////////////
// print the whole ClassAd into a file. The expressions are in infix notation.
// Returns FALSE if the file pointer is NULL; TRUE otherwise.
////////////////////////////////////////////////////////////////////////////////
int ClassAd::fPrint(FILE* f)
{
    fprintf(f, "MyType = ");
    fprintf(f, "%c", '"');
    fprintf(f, "%s", GetMyTypeName());
    fprintf(f, "%c\n", '"');
    fprintf(f, "TargetType = ");
    fprintf(f, "%c", '"');
    fprintf(f, "%s", GetTargetTypeName());
    fprintf(f, "%c\n", '"');    
	
	AttrList::fPrint(f);
}
