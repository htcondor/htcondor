//******************************************************************************
// AttrList.C
//
// Implementation of AttrList classes and AttrListList classes.
//
//******************************************************************************

# include <ctype.h>
# include <assert.h>
# include <string.h>
# include <sys/time.h>
# include <iomanip.h>

# include "except.h"
# include "debug.h"
# include "condor_ast.h"
# include "condor_expressions.h"
# include "condor_attrlist.h"

static 	char *_FileName_ = __FILE__;         // Used by EXCEPT (see except.h)
extern 	"C" int _EXCEPT_(char*, ...);
extern	"C"	void dprintf(int, char* fmt, ...);
#if defined(USE_XDR)
extern  "C" int  xdr_mywrapstring (XDR *, char **);
#endif
extern  "C" int store_stmt (EXPR *, CONTEXT *);
extern void evalFromEnvironment (const char *, EvalResult *);

////////////////////////////////////////////////////////////////////////////////
// AttrListElem constructor.
////////////////////////////////////////////////////////////////////////////////
AttrListElem::AttrListElem(ExprTree* expr)
{
    tree = expr;
    dirty = FALSE;
    name = ((Variable*)expr->LArg())->Name();
    next = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AttrListElem copy constructor. 
////////////////////////////////////////////////////////////////////////////////
AttrListElem::AttrListElem(AttrListElem& oldNode)
{
    oldNode.tree->Copy();
    this->tree = oldNode.tree;
    this->dirty = FALSE;
    this->name = ((Variable*)tree->LArg())->Name();
    next = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AttrListAbstract constructor. It is called by its derived classes.
// AttrListAbstract is a purely virtual class, there is no need for a user to
// declare a AttrListAbstract instance.
////////////////////////////////////////////////////////////////////////////////
AttrListAbstract::AttrListAbstract(int type)
{
    this->type = type;
    this->inList = NULL;
    this->next = NULL;
    this->prev = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AttrListRep constructor.
////////////////////////////////////////////////////////////////////////////////
AttrListRep::AttrListRep(AttrList* attrList, AttrListList* attrListList)
: AttrListAbstract(ATTRLISTREP)
{
    this->attrList = attrList;
    this->inList = attrListList;
    this->nextRep = (AttrListRep *)attrList->next;
    attrList->inList = NULL;
    attrList->next = this;
}

////////////////////////////////////////////////////////////////////////////////
// AttrList class constructor when there is no AttrListList associated with it.
////////////////////////////////////////////////////////////////////////////////
AttrList::AttrList() : AttrListAbstract(ATTRLISTENTITY)
{
	seq = 0;
    exprList = NULL;
    tail = NULL;
    ptrExpr = NULL;
    ptrName = NULL;
    associatedList = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AttrList class constructor. The parameter indicates that this AttrList has
// an AttrListList associated with it.
////////////////////////////////////////////////////////////////////////////////
AttrList::AttrList(AttrListList* associatedList) :
		  AttrListAbstract(ATTRLISTENTITY)
{
	seq = 0;
    exprList = NULL;
    tail = NULL;
    ptrExpr = NULL;
    ptrName = NULL;
    this->associatedList = associatedList;
    if(associatedList)
    {
        if(!associatedList->associatedAttrLists)
        {
      	    associatedList->associatedAttrLists = new AttrListList;
        }
        associatedList->associatedAttrLists->Insert(this);
    }
}

//
// Constructor of AttrList class, read from a file.
// The string "delimitor" passed in or EOF delimits the end of a AttrList input.
// The newline character delimits an expression. White spaces before a new
// expression are ignored.
// 1 is passed on to the calling procedure if EOF is reached. Otherwise, 0
// is passed on.
//
AttrList::AttrList(FILE *file, char *delimitor, int &isEOF) : AttrListAbstract(ATTRLISTENTITY)
{
    ExprTree *tree;

	seq = 0;
    exprList = NULL;
    associatedList = NULL;
    tail = NULL;
    ptrExpr = NULL;
    ptrName = NULL;

    int c;
    int buffer_size = 10;                    // size of the input buffer.
    int current = 0;                         // current position in buffer. 
    char *buffer = new char[buffer_size];
    if(!buffer)
    {
        EXCEPT("Warning : you ran out of memory");
    }

    isEOF = 0;
    c = fgetc(file);
    while(isspace(c))
    {
        c = fgetc(file);                     // skip leading white spaces.
    }
    ungetc(c, file);

    while(1)                                 // loop character by character 
    {                                        // to the end of the stirng to
        c = fgetc(file);                     // construct a AttrList object.
        if(c == EOF || c == '\n')                 
	{                                    // end of an expression.
	    if(current)
	    {                                // current expression not empty.
	        buffer[current] = '\0';
		if(delimitor)
		{
		    if(!strncmp(buffer, delimitor, strlen(delimitor)))
		    {                        // end of a AttrList instance 
		        c = fgetc(file);     // input.
		        while(isspace(c))    // look at the next character.
			{
			    c = fgetc(file);
			}
			if(c == EOF)
			{
			    isEOF = 1;        // next character is EOF. 
			}
			else
			{
			    isEOF = 0;        // next character is not EOF.
			    ungetc(c, file);
			}
			break;
		    }
		}
		if(Parse(buffer, tree))
		{
			EXCEPT("Parse error in the input string");
		}
		Insert(tree);
	    }
	    if(c == EOF)
	    {                                // end of input.
	        isEOF = 1;
	        break;
	    }
	    c = fgetc(file);                 // look at the next character.
	    while(isspace(c))
	    {
	        c = fgetc(file);             // skip white spaces.
	    }
            if(c == EOF)
	    {
	        isEOF = 1;
	        break;                       // end of input.
	    }
	    ungetc(c, file);
	    buffer_size = 10;                // process next expression.
	    current = 0;                  
	    delete []buffer;
	    buffer = new char[buffer_size];
	    if(!buffer)
	    {
        	EXCEPT("Warning : you ran out of memory");
	    }	    
	}
	else
	{                                    // strore in buffer here.
	    if(current >= (buffer_size - 1))        
	    {
	        buffer_size *= 2;
			buffer = (char *) realloc(buffer, buffer_size*sizeof(char));
			if(!buffer)
			{
        		EXCEPT("Warning : you ran out of memory");
			}
	    }
	    buffer[current] = c;
	    current++;
	}
    }
}

//
// Constructor of AttrList class, read from a string.
// The character 'delimitor' passed in or end of string delimits an expression,
// end of string delimits a AttrList input.
// If there are only white spaces between the last delimitor and the end of 
// string, they are to be ignored, no parse error.
//
AttrList::AttrList(char *AttrList, char delimitor) : AttrListAbstract(ATTRLISTENTITY)
{
    ExprTree *tree;

	seq = 0;
    exprList = NULL;
    associatedList = NULL;
    tail = NULL;
    ptrExpr = NULL;
    ptrName = NULL;

    char c;
    int buffer_size = 10;                    // size of the input buffer.
    int current = 0;                         // current position in buffer. 
    char *buffer = new char[buffer_size];
    if(!buffer)
    {
		EXCEPT("Warning : you ran out of memory");
    }
    int i=0;
    while(isspace(c = AttrList[i]))
    {
        i++;                                 // skip white spaces.
    }    

    while(1)                                 // loop character by character 
    {                                        // to the end of the stirng to
        c = AttrList[i];                      // construct a AttrList object.
        if(c == delimitor || c == '\0')                 
	{                                    // end of an expression.
	    if(current)
	    {                                // if expression not empty.
	        buffer[current] = '\0';
		if(!Parse(buffer, tree))
		{
		    if(tree->MyType() == LX_ERROR)
		    {
				EXCEPT("Warning : you ran out of memory");
		    }
		}
		else
		{
			EXCEPT("Parse error in the input string");
		}
		Insert(tree);
	    }
	    delete []buffer;
	    if(c == '\0')
	    {                                // end of input.
	        break;
	    }
	    i++;                             // look at the next character.
	    while(isspace(c = AttrList[i]))
	    {
	        i++;                         // skip white spaces.
	    }
            if((c = AttrList[i]) == '\0')
	    {
	        break;                       // end of input.
	    }
	    i--;
	    buffer_size = 10;                // process next expression.
	    current = 0;                  
	    buffer = new char[buffer_size];
	    if(!buffer)
	    {
	        EXCEPT("Warning: you ran out of memory");
	    }	    
	}
	else
	{                                    // fill in the buffer.
	    if(current >= (buffer_size - 1))        
	    {
	        buffer_size *= 2;
		buffer = (char *) realloc(buffer, buffer_size*sizeof(char));
		if(!buffer)
		{
	        EXCEPT("Warning: you ran out of memory");
		}
	    }
	    buffer[current] = c;
	    current++;
	}
	i++;
    }
}

ExprTree* ProcToTree(char*, LexemeType, int, float, char*);

#if 0 /* don't want to link with ProcObj class; we shouldn't need this */
////////////////////////////////////////////////////////////////////////////////
// Create a AttrList from a proc object.
////////////////////////////////////////////////////////////////////////////////
AttrList::AttrList(ProcObj* procObj) : AttrListAbstract(ATTRLISTENTITY)
{
	ExprTree*	tmpTree;	// trees converted from proc structure fields

	seq = 0;
	exprList = NULL;
	associatedList = NULL;
	tail = NULL;
	ptrExpr = NULL;
	ptrName = NULL;


	// Convert the fields in a proc structure into expression trees and insert
	// into the classified ad.
	tmpTree = ProcToTree("Status", LX_INTEGER, procObj->get_status(), 0, NULL);
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : Status");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("Prio", LX_INTEGER, procObj->get_prio(), 0, NULL);
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : Prio");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("ClusterId", LX_INTEGER, procObj->get_cluster_id(),
						 0, NULL);
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : ClusterId");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("ProcId", LX_INTEGER, procObj->get_proc_id(), 0, NULL);
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : ProcId");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("LocalCPU", LX_FLOAT, 0, procObj->get_local_cpu(),
						 NULL);
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : LocalCPU");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("RemoteCPU", LX_FLOAT, 0, procObj->get_remote_cpu(),
						 NULL);
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : RemoteCPU");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("Owner", LX_STRING, 0, 0, procObj->get_owner());
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : Owner");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("Arch", LX_STRING, 0, 0, procObj->get_arch());
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : Arch");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("OpSys", LX_STRING, 0, 0, procObj->get_opsys());
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : OpSys");
	}
	Insert(tmpTree);

	tmpTree = ProcToTree("Requirements", LX_STRING, 0, 0,
						 procObj->get_requirements());
	if(!tmpTree)
	{
		EXCEPT("AttrList::AttrList(ProcObj*) : Requirements");
	}
	Insert(tmpTree);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Converts a (key word, value) pair from a proc structure to a tree.
////////////////////////////////////////////////////////////////////////////////
ExprTree* AttrList::ProcToTree(char* var, LexemeType t, int i, float f, char* s)
{
	ExprTree*	tmpVarTree;		// Variable node
	ExprTree*	tmpTree;		// Value tree
	char*		tmpStr;			// used to add "" to a string
	char*		tmpVarStr;		// make a copy of "var"

	tmpVarStr = new char[strlen(var)+1];
	strcpy(tmpVarStr, var);
	tmpVarTree = new Variable(tmpVarStr);
	switch(t)
	{
		case LX_INTEGER :

			tmpTree = new Integer(i);
			break;

		case LX_FLOAT :

			tmpTree = new Float(f);
			break;

		case LX_STRING :

			tmpStr = new char[strlen(s)+3];
			sprintf(tmpStr, "\"%s\"", s);
			tmpTree = new String(tmpStr);
			break;

		case LX_EXPR :

			if(Parse(s, tmpTree) != 0)
			{
				delete tmpVarTree;
				delete tmpTree;
				return NULL;
			}
			break;
		
		default:

			break;
	}

	return new AssignOp(tmpVarTree, tmpTree);
}

////////////////////////////////////////////////////////////////////////////////
// Create a AttrList from a CONTEXT.
////////////////////////////////////////////////////////////////////////////////

int IsOperator(ELEM *elem)
{
  switch(elem->type) {
    case GETS  :
    case LT    :
    case LE    :
    case GT    :
    case GE    :
    case EQ    :
    case NE    :
    case AND   :
    case OR    :
    case PLUS  :
    case MINUS :
    case MUL   :
    case DIV   : return TRUE;
    default    : return FALSE;
  }
}

ELEM* Pop(STACK* stack)
{
	stack->top--;
	return stack->data[stack->top];
}

void Push(STACK* stack, ELEM *elem)
{
  stack->data[stack->top] = elem;
  stack->top++;
}

char *Print_elem(ELEM *elem, char *str)
{
  char *tmpChar;

  switch(elem->type) {
    case GETS  : sprintf(str, " = ");
		 break;
    case LT    : sprintf(str, " < ");
		 break;
    case LE    : sprintf(str, " <= ");
		 break;
    case GT    : sprintf(str, " > ");
		 break;
    case GE    : sprintf(str, " >= ");
		 break;
    case EQ    : sprintf(str, " == ");
		 break;
    case NE    : sprintf(str, " != ");
		 break;
    case AND   : sprintf(str, " && ");
		 break;
    case OR    : sprintf(str, " || ");
		 break;
    case PLUS  : sprintf(str, " + ");
		 break;
    case MINUS : sprintf(str, " - ");
		 break;
    case MUL   : sprintf(str, " * ");
		 break;
    case DIV   : sprintf(str, " / ");
		 break;
    case NAME  : sprintf(str, "%s", elem->val.string_val);
		 break;
    case STRING: 
				 if(!elem->val.string_val)
				 {
					 sprintf(str, "(Null)");
				 }
				 else
				 {
					 sprintf(str, "\"%s\"", elem->val.string_val);
				 } 
				 break;
    case FLOAT : sprintf(str, "%f", elem->val.float_val);
		 break;
    case INT   : sprintf(str, "%d", elem->val.integer_val);
		 break;
    case BOOL  : sprintf(str, elem->val.integer_val? "TRUE" : "FALSE");
		 break;
    case NOT   : sprintf(str, "!");
		 break;
    case EXPRSTRING  : sprintf(str, "%s", elem->val.string_val);
		 break;
  }

  for(tmpChar = str; *tmpChar != '\0'; tmpChar++);
  return tmpChar;
}

AttrList::AttrList(CONTEXT* context) : AttrListAbstract(ATTRLISTENTITY)
{
	STACK		stack;
	ELEM*		elem, *lElem, *rElem;
	char*		tmpExpr;
	char*		tmpChar;
	int			i, j;
	ExprTree*	tmpTree;

	stack.top = 0;
	seq = 0;
	associatedList = NULL;
	tail = NULL;
	ptrExpr = NULL;
	ptrName = NULL;
	exprList = NULL;

	for(i = 0; i < context->len; i++)
	{
    	for(j = 0; j < (context->data)[i]->len; j++)
		{
      		if(((context->data)[i]->data)[j]->type == ENDMARKER)
			{
				elem = Pop(&stack);
				if(Parse(elem->val.string_val, tmpTree) != 0)
				{
					dprintf(D_EXPR, "Can't parse %s\n", elem->val.string_val);
				}
				else
				{
					Insert(tmpTree);
				}
				delete []elem->val.string_val;
				delete elem;
      		}
			else if(((context->data)[i]->data)[j]->type == NOT)
			{
				tmpExpr = new char[1000];
				strcpy(tmpExpr, "");
				rElem = Pop(&stack);
				strcat(tmpExpr, "(!");
				tmpChar = tmpExpr + 2;
				Print_elem(rElem, tmpChar);
				strcat(tmpExpr, ")");
				elem = new ELEM;
				elem->val.string_val = tmpExpr;
				elem->type = EXPRSTRING;
				Push(&stack, elem);
				if(rElem->type == EXPRSTRING)
				{
					delete rElem->val.string_val;
					delete rElem;
        		}
		    }
			else if(IsOperator(((context->data)[i]->data)[j]))
			{
				tmpExpr = new char[1000];
				strcpy(tmpExpr, "");
				rElem = Pop(&stack);
				lElem = Pop(&stack);
				if(((context->data)[i]->data)[j]->type != GETS)
				{
			    	strcat(tmpExpr, "(");
					tmpChar = tmpExpr + 1;
				}
				else
					tmpChar = tmpExpr;
				tmpChar = Print_elem(lElem, tmpChar);
				tmpChar = Print_elem(((context->data[i]->data)[j]), tmpChar);
				Print_elem(rElem, tmpChar);
				if(((context->data)[i]->data)[j]->type != GETS)
					strcat(tmpExpr, ")");
				elem = new ELEM;
				elem->val.string_val = tmpExpr;
				elem->type = EXPRSTRING;
				Push(&stack, elem);
				if(rElem->type == EXPRSTRING)
				{
					delete rElem->val.string_val;
					delete rElem;
       			}
				if(lElem->type == EXPRSTRING)
				{
					delete lElem->val.string_val;
					delete lElem;
				}
			} else
				Push(&stack, ((context->data)[i]->data)[j]);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// AttrList class copy constructor.
////////////////////////////////////////////////////////////////////////////////
AttrList::AttrList(AttrList &old) : AttrListAbstract(ATTRLISTENTITY)
{
    AttrListElem* tmpOld;	// working variable
    AttrListElem* tmpThis;	// working variable

    if(old.exprList)
    {
	// copy all the AttrList elements. The pointers to the trees are copied
	// but not the trees
	this->exprList = new AttrListElem(*old.exprList);
	tmpThis = this->exprList;
        for(tmpOld = old.exprList->next; tmpOld; tmpOld = tmpOld->next)
        {
	    tmpThis->next = new AttrListElem(*tmpOld);
	    tmpThis = tmpThis->next;
        }
	tmpThis->next = NULL;
        this->tail = tmpThis;
    }
    else
    {
        this->exprList = NULL;
        this->tail = NULL;
    }
    this->ptrExpr = NULL;
    this->ptrName = NULL;
    this->associatedList = old.associatedList;
	this->seq = old.seq;
    if(this->associatedList)
    {
		this->associatedList->associatedAttrLists->Insert(this);
    }
}

////////////////////////////////////////////////////////////////////////////////
// AttrList class destructor.
////////////////////////////////////////////////////////////////////////////////
AttrList::~AttrList()
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
}

////////////////////////////////////////////////////////////////////////////////
// Insert an expression tree into a AttrList. If the expression is not an
// assignment expression, FALSE is returned. Otherwise, it is inserted at the
// end of the expression list. It is not checked if the attribute already
// exists in the list!
////////////////////////////////////////////////////////////////////////////////
int AttrList::Insert(char* str)
{
	ExprTree*	tree;

	if(Parse(str, tree) != 0)
	{
		return FALSE;
	}
	return Insert(tree);
}

int AttrList::Insert(ExprTree* expr)
{
    if(expr->MyType() != LX_ASSIGN)
    {
		return FALSE;
    }

	if(Lookup(expr->LArg()))
	{
		Delete(((Variable*)expr->LArg())->Name());
	}

    AttrListElem* newNode = new AttrListElem(expr);

    if(!tail)
    {
		exprList = newNode;
    }
    else
    {
		tail->next = newNode;
    }
    tail = newNode;

    // update associated aggregate expressions
    if(inList)		// this AttrList is in only one AttrList list
    {
	inList->UpdateAgg(expr, AGG_INSERT);
    }
    else if(next)	// this AttrList is in more than one AttrList lists
    {
	AttrListRep* rep = (AttrListRep*)next;
	while(rep)
	{
	    rep->attrList->inList->UpdateAgg(expr, AGG_INSERT);
	}
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// If the attribute is already in the list, replace it with the new one.
// Otherwise just insert it.
////////////////////////////////////////////////////////////////////////////////
int AttrList::InsertOrUpdate(char* attr)
{
	ExprTree*	tree;

	if(Parse(attr, tree) != 0)
	{
		return FALSE;
	}
	if(tree->MyType() != LX_ASSIGN)
	{
		return FALSE;
	}
	if((Insert(tree) == FALSE) && (UpdateExpr(tree) == FALSE))
	{
		return FALSE;
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Delete an expression with the name "name" from this AttrList. Return TRUE if
// successful; FALSE if the expression can not be found.
////////////////////////////////////////////////////////////////////////////////
int AttrList::Delete(const char* name)
{
    AttrListElem*	prev = exprList;
    AttrListElem*	cur = exprList;

    while(cur)
    {
		if(!strcasecmp(name, cur->name))
		// expression to be deleted is found
		{
			// delete the expression
			if(cur == exprList)
			// the expression to be deleted is at the head of the list
			{
				exprList = exprList->next;
				if(tail == cur)
				{
					tail = NULL;
				}
			}
			else
			{
				prev->next = cur->next;
				if(tail == cur)
				{
					tail = prev;
				}
			}

			if(ptrExpr == cur)
			{
				ptrExpr = cur->next;
			}

			if(ptrName == cur)
			{
				ptrName = cur->next;
			}

			// update the associated aggregate expressions
			if(inList)	  // this AttrList is in only one AttrList list
			{
				inList->UpdateAgg(cur->tree, AGG_REMOVE);
			}
			else if(next) // this AttrList is in more than one AttrList lists
			{
				AttrListRep* rep = (AttrListRep*)next;
				while(rep)
				{
					rep->attrList->UpdateAgg(cur->tree, AGG_REMOVE);
				}
			}
			delete cur;
			return TRUE;
		}
		else
		// expression to be deleted not found, continue search
		{
			prev = cur;
			cur = cur->next;
		}
    }	// end of while loop to search the expression to be deleted

    return FALSE; // expression not found
}

////////////////////////////////////////////////////////////////////////////////
// Reset the dirty flags for each expression tree in the AttrList.
////////////////////////////////////////////////////////////////////////////////
void AttrList::ResetFlags()
{
    AttrListElem*	tmpElem;	// working variable

    tmpElem = exprList;
    while(tmpElem)
    {
	tmpElem->dirty = FALSE;
	tmpElem = tmpElem->next;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Find an attibute and replace its value with a new value.
////////////////////////////////////////////////////////////////////////////////
int AttrList::UpdateExpr(char* name, ExprTree* tree)
{
    ExprTree*	tmpTree;	// the expression to be updated

    if(tree->MyType() == LX_ASSIGN)
    {
		return FALSE;
    }

    if(!(tmpTree = Lookup(name)))
    {
		return FALSE;
    }

    tree->Copy();
	delete tmpTree->RArg();
	((BinaryOp*)tmpTree)->rArg = tree;
    return TRUE;
}

int AttrList::UpdateExpr(ExprTree* attr)
{
	if(attr->MyType() != LX_ASSIGN)
	{
		return FALSE;
	}
	return UpdateExpr(((Variable*)attr->LArg())->Name(), attr->RArg());
}

ExprTree* AttrList::NextExpr()
{
    if(!this->ptrExpr)
    {
		return NULL;
    }
    else
    {
		ExprTree* tmp = ptrExpr->tree;
		ptrExpr = ptrExpr->next;
		return tmp;
    }
}

char* AttrList::NextName()
{
    if(!this->ptrName)
    {
	return NULL;
    }
    else
    {
	char* tmp = new char[strlen(ptrName->name) + 1];
	strcpy(tmp, ptrName->name);
	ptrName = ptrName->next;
	return tmp;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Return the named expression without copying it. Return NULL if the named
// expression is not found.
////////////////////////////////////////////////////////////////////////////////
ExprTree* AttrList::Lookup(char* name)
{
	return Lookup ((const char *) name);
}

ExprTree* AttrList::Lookup(const char* name)
{
    AttrListElem*	tmpNode;
    char*	 		tmpChar;	// variable name

    for(tmpNode = exprList; tmpNode; tmpNode = tmpNode->next)
    {
		tmpChar = ((Variable*)tmpNode->tree->LArg())->Name();
        if(!strcasecmp(tmpChar, name))
        {
            return(tmpNode->tree);
        }
    }
    return NULL;
}

ExprTree* AttrList::Lookup(const ExprTree* attr)
{
	return Lookup (((Variable*)attr)->Name());
}

int AttrList::LookupString (const char *name, char *value)
{
	ExprTree *tree, *rhs;
	char     *strVal;

	tree = Lookup (name);
	if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_STRING) &&
		(strVal = ((String *) rhs)->Value()) && strVal)
	{
		strcpy (value, strVal);
		return 1;
	}

	return 0;
}

int AttrList::LookupInteger (const char *name, int &value)
{
    ExprTree *tree, *rhs;

    tree = Lookup (name);
    if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_INTEGER))
	{
        value = ((Integer *) rhs)->Value();
        return 1;
    }

    return 0;
}

int AttrList::LookupFloat (const char *name, float &value)
{
    ExprTree *tree, *rhs;   

    tree = Lookup (name);   
    if( tree && (rhs=tree->RArg()) ) {
		if( rhs->MyType() == LX_FLOAT ) {
			value = ((Float *) rhs)->Value();
			return 1;   
		} 
		if( rhs->MyType() == LX_INTEGER ) {
			value = (float)(((Integer *) rhs)->Value());
			return 1;   
		} 
	}		
	return 0;   
}

int AttrList::LookupBool (const char *name, int &value)  
{   
    ExprTree *tree, *rhs;       

    tree = Lookup (name);       
    if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_BOOL))    
    {   
        value = ((Boolean *) rhs)->Value();   
        return 1;       
    }       

    return 0;       
}   


int AttrList::EvalString (const char *name, AttrList *target, char *value)
{
	ExprTree *tree;
	EvalResult val;

	tree = Lookup(name);
	if(!tree) {
		if (target) {
			tree = target->Lookup(name);
		} else {
			evalFromEnvironment (name, &val);
			if (val.type == LX_STRING && val.s)
			{
				strcpy (value, val.s);
				return 1;
			}
			return 0;
		}
	}
	
	if(tree && tree->EvalTree(this,target,&val) && val.type==LX_STRING && val.s)
	{
		strcpy (value, val.s);
		return 1;
	}

	return 0;
}

int AttrList::EvalInteger (const char *name, AttrList *target, int &value) 
{
    ExprTree *tree;
    EvalResult val;   

	tree = Lookup(name);
	if(!tree) {
		if (target) {
			tree = target->Lookup(name);
		} else {
			evalFromEnvironment (name, &val);
			if (val.type == LX_INTEGER)
			{
				value = val.i;
				return 1;
			}
			return 0;
		}
	}
	
    if (tree && tree->EvalTree (this, target, &val) && val.type == LX_INTEGER)
    {
		value = val.i;
        return 1;
    }

    return 0;
}

int AttrList::EvalFloat (const char *name, AttrList *target, float &value) 
{
    ExprTree *tree;
    EvalResult val;   

    tree = Lookup(name);   

    if(!tree) {
        if (target) {
            tree = target->Lookup(name);
        } else {
			evalFromEnvironment (name, &val);
			if( val.type == LX_FLOAT ) {
				value = val.f;
				return 1;
			}
			if( val.type == LX_INTEGER ) {
				value = (float)val.i;
				return 1;
			}
            return 0;
        }
    }

    if( tree && tree->EvalTree (this, target, &val) ) {
		if( val.type == LX_FLOAT ) {
			value = val.f;
			return 1;
		}
		if( val.type == LX_INTEGER ) {
			value = (float)val.i;
			return 1;
		}
    }
    return 0;
}

int AttrList::EvalBool (const char *name, AttrList *target, int &value) 
{
    ExprTree *tree;
    EvalResult val;   

    tree = Lookup(name);   
    if(!tree) {
        if (target) {
            tree = target->Lookup(name);
        } else {
			evalFromEnvironment (name, &val);
			if (val.type == LX_BOOL)
			{
				value = val.b;
				return 1;
			}
			return 0;
        }
    }

    if (tree && tree->EvalTree (this, target, &val) && val.type == LX_BOOL)
    {
        value = val.b;
        return 1;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Test to see if the AttrList belongs to a AttrList list. Returns TRUE if it
// does, FALSE if not.
////////////////////////////////////////////////////////////////////////////////
int AttrList::IsInList(AttrListList* AttrListList)
{
    AttrListRep* tmp;

    if(!this->inList && !this->next)
    {
	// not in any list
	return FALSE;
    }
    else if(this->inList)
    {
	// in one list
	if(this->inList == AttrListList)
	{
	    return TRUE;
	}
	else
	{
	    return FALSE;
	}
    }
    else
    {
	// in more than one list
	tmp = (AttrListRep *)this->next;
	while(tmp && tmp->inList != AttrListList)
	{
	    tmp = tmp->nextRep;
	}
	if(!tmp)
	{
	    return FALSE;
	}
	else
	{
	    return TRUE;
	}
    }
}

////////////////////////////////////////////////////////////////////////////////
// Print an expression with a certain name into a file. Returns FALSE if the
// pointer to the file or the name is NULL, or the named expression can not be
// found in this AttrList; TRUE otherwise.
////////////////////////////////////////////////////////////////////////////////
int AttrList::fPrintExpr(FILE* f, char* name)
{
    if(!f || !name)
    {
	return FALSE;
    }

    ExprTree*	tmpExpr = Lookup(name);
    char	tmpStr[10000] = "";

    if(!tmpExpr)
    // the named expression is not found
    {
	return FALSE;
    }

    tmpExpr->PrintToStr(tmpStr);
    fprintf(f, "%s\n", tmpStr);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// print the whole AttrList into a file. The expressions are in infix notation.
// Returns FALSE if the file pointer is NULL; TRUE otherwise.
////////////////////////////////////////////////////////////////////////////////
int AttrList::fPrint(FILE* f)
{
    AttrListElem*	tmpElem;
    char			tmpLine[10000] = "";

    if(!f)
    {
		return FALSE;
    }

    for(tmpElem = exprList; tmpElem; tmpElem = tmpElem->next)
    {
        tmpElem->tree->PrintToStr(tmpLine);
        fprintf(f, "%s\n", tmpLine);
        strcpy(tmpLine, "");
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Update an aggregate expression. "expr" must be an assignment expression.
// FALSE is returned if it is not.
////////////////////////////////////////////////////////////////////////////////
int AttrList::UpdateAgg(ExprTree* expr, int operation)
{
    ExprTree*	oldExpr; // the aggregate expression before updating
    ExprTree*	newExpr; // the new aggregate expression

    if(expr->MyType() != LX_ASSIGN)
    {
		return FALSE;
    }

    oldExpr = Lookup(((Variable*)expr->LArg())->Name());

    if(!oldExpr)
    {
		return FALSE;
    }

    switch(operation)
    {
		case AGG_INSERT :

			if(oldExpr->RArg()->MyType() == LX_AGGADD)
			{
				expr->Copy();
				newExpr = new AggAddOp(oldExpr->RArg(), expr);
				((BinaryOp*)oldExpr)->rArg = newExpr;
			}
			if(oldExpr->RArg()->MyType() == LX_AGGEQ)
			{
				expr->Copy();
				newExpr = new AggEqOp(oldExpr->RArg(), expr);
				((BinaryOp*)oldExpr)->rArg = newExpr;
			}
			break;
	
		case AGG_REMOVE :

			 newExpr = oldExpr->RArg();
			 if(((AggOp*)newExpr)->DeleteChild((AssignOp*)expr))
			 {
				 if(!oldExpr->RArg()->LArg() && !oldExpr->RArg()->RArg())
				 {
					 Delete(((Variable*)oldExpr->LArg())->Name());
					 delete oldExpr;
				 }
			 }
			 break;
    }
    return TRUE;
}


//////////////////////////////////////////////////////////////////////////////
// Create a CONTEXT from an AttrList
//////////////////////////////////////////////////////////////////////////////
int
AttrList::MakeContext (CONTEXT *c)
{
	char *line = new char [256];
	AttrListElem *elem;
	EXPR *expr;

	for (elem = exprList; elem; elem = elem -> next)
	{
		line [0] = '\0';
		elem->tree->PrintToStr (line);
		expr = scan (line);
		if (expr == NULL)
			return FALSE;
		store_stmt (expr, c);
	}

	delete [] line;
	return TRUE;
}


AttrListList::AttrListList()
{
    head = NULL;
    tail = NULL;
    ptr = NULL;
    associatedAttrLists = NULL;
    length = 0;
}

AttrListList::AttrListList(AttrListList& oldList)
{
    AttrList*	tmpAttrList;

    head = NULL;
    tail = NULL;
    ptr = NULL;
    associatedAttrLists = NULL;
    length = 0;

    if(oldList.head)
    // the AttrList list to be copied is not empty
    {
	oldList.Open();
	while((tmpAttrList = oldList.Next()))
	{
	    switch(tmpAttrList->Type())
	    {
			case ATTRLISTENTITY :

				Insert(new AttrList(*tmpAttrList));
				break;
	    }
	}
	oldList.Close();
    }
}

AttrListList::~AttrListList()
{
    this->Open();
    AttrList *tmpAttrList = Next();

    while(tmpAttrList)
    {
		Delete(tmpAttrList);
		tmpAttrList = Next();
    }
    this->Close();
}

void AttrListList::Open()
{
  ptr = head;
}

void AttrListList::Close()
{
  ptr = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Returns the pointer to the AttrList in the list pointed to by "ptr".
////////////////////////////////////////////////////////////////////////////////
AttrList *AttrListList::Next()
{
    if(!ptr)
        return NULL;

    AttrList *tmpAttrList;

    if(ptr->Type() == ATTRLISTENTITY)
    {
	// current AttrList is in one AttrList list
	tmpAttrList = (AttrList *)ptr;
        ptr = ptr->next;
	return tmpAttrList;
    }
    else
    {
	// current AttrList is in more than one AttrList list
	tmpAttrList = (AttrList *)((AttrListRep *)ptr)->attrList;
        ptr = ptr->next;
	return tmpAttrList;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Insert a AttrList or a replication of the AttrList if the AttrList already
// belongs to some other lists at the end of a AttrList list and update the
// aggregate AttrList in that AttrList list.
////////////////////////////////////////////////////////////////////////////////
void AttrListList::Insert(AttrList* AttrList)
{
    AttrListRep *rep;

    if(AttrList->IsInList(this))
    // AttrList is already in this AttrList list
    {
	return;
    }
    if(AttrList->inList)
    // AttrList is in one AttrList list
    {
	// AttrList to AttrListRep
	AttrListAbstract *saveNext = AttrList->next;
	AttrListList *tmpList = AttrList->inList;
	AttrList->next = NULL;
	rep = new AttrListRep(AttrList, AttrList->inList);
	rep->next = saveNext;
	if(tmpList->head == (AttrListAbstract *)AttrList)
	{
	    // AttrList is the first element in the list
	    tmpList->head = rep;
	}
	else
	{
	    AttrList->prev->next = rep;
	}
	if(tmpList->tail == (AttrListAbstract *)AttrList)
	{
	    // AttrList is the last element in the list
	    tmpList->tail = rep;
	}
	else
	{
	    rep->next->prev = rep;
	}
	if(tmpList->ptr == AttrList)
	{
	    tmpList->ptr = rep;
	}
	AttrList->prev = NULL;
	AttrList->inList = NULL;

	// allocate new AttrListRep for this AttrList list
	rep = new AttrListRep(AttrList, this);
    }
    else if(AttrList->next)
    {
	// AttrList is in more than one AttrList lists
	rep = new AttrListRep(AttrList, this);
    }
    else
    {
	// AttrList is not in any AttrList list
	rep = (AttrListRep *)AttrList;
	AttrList->inList = this;
    }
    rep->prev = this->tail;
    rep->next = NULL;
    this->tail = rep;
    if(rep->prev != NULL)
    {
	rep->prev->next = rep;
    }
    else
    {
	this->head = rep;
    }

    this->length++;

    // update associated aggregate expressions
    ExprTree*	tmpExpr;

    AttrList->ResetExpr();
    while((tmpExpr = AttrList->NextExpr()))
    {
        UpdateAgg(tmpExpr, AGG_INSERT);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Assume parameter AttrList is not NULL and it's a real AttrList, not a        //
// AttrListRep. This function doesn't do anything if AttrList is not in the     //
// AttrList list.                              				      //
////////////////////////////////////////////////////////////////////////////////
int AttrListList::Delete(AttrList* attrList)
{
    AttrListAbstract*	cur;
    AttrListRep*		tmpRep;
    
    for(cur = head; cur; cur = cur->next)
    {
		if(cur->Type() == ATTRLISTENTITY) // this AttrList has no replicant
		{
			if(cur == attrList) // this is the AttrList to be deleted
			{
				if(cur == ptr) ptr = ptr->next;
				if(cur == head)
				// AttrList to be deleted is at the head of the list
				{
					head = head->next;
					if(head)
					{
						head->prev = NULL;
					}
				}
				else if(cur == tail)
				// AttrList to be deleted is at the tail of the list
				{
					tail = cur->prev;
					if(tail)
					{
						tail->next = NULL;
					}
				}
				else
				// AttrList deleted is at neither the head nor the tail
				{
					cur->prev->next = cur->next;
					cur->next->prev = cur->prev;
				}

				// update aggregate AttrList
				ExprTree*	tmpExpr;

				((AttrList*)cur)->ResetExpr();
				while((tmpExpr = ((AttrList*)cur)->NextExpr()))
				{
					UpdateAgg(tmpExpr, AGG_REMOVE);
				}
				delete cur;
				break;
			}
		}
		else // a rep is used
		{
			if(((AttrListRep *)cur)->attrList == attrList)
			{
				// this is the AttrList to be deleted
				if(cur == ptr) ptr = ptr->next;
				if(cur == head)
				{
					// AttrList to be deleted is at the head of the list
					head = head->next;
					if(head)
					{
						head->prev = NULL;
					}
				}
				else if(cur == tail)
				{
					// AttrList to be deleted is at the tail of the list
					tail = cur->prev;
					if(tail)
					{
						tail->next = NULL;
					}
				}
				else
				{
					cur->prev->next = cur->next;
					cur->next->prev = cur->prev;
				}

				// delete the rep from the rep list
				tmpRep = (AttrListRep *)((AttrListRep *)cur)->attrList->next;
				if(tmpRep == cur)
				{
					((AttrListRep *)cur)->attrList->next = ((AttrListRep *)cur)->nextRep;
				}
				else
				{
					while(tmpRep->nextRep != cur)
					{
						tmpRep = tmpRep->nextRep;
					}
					tmpRep->nextRep = ((AttrListRep *)cur)->nextRep;
				}

                // update associated aggregate expressions
                ExprTree*	tmpExpr;

				attrList->ResetExpr();
				while((tmpExpr = attrList->NextExpr()))
				{
					UpdateAgg(tmpExpr, AGG_REMOVE);
				}
				delete cur;
				break;
			}
		} // end of if a rep is used
    } // end of the loop through the AttrListList
    return TRUE;
}

ExprTree* AttrListList::Lookup(const char* name, AttrList*& attrList)
{
    AttrList*	tmpAttrList;
    ExprTree*	tmpExpr;

    Open();
    for(tmpAttrList = Next(); tmpAttrList; tmpAttrList = Next())
    {
        if((tmpExpr = tmpAttrList->Lookup(name)))
        {
            Close();
            attrList = tmpAttrList;
            return tmpExpr;
        }
    }
    Close();
    return NULL;
}

ExprTree* AttrListList::Lookup(const char* name)
{
    AttrList*	tmpAttrList;
    ExprTree*	tmpExpr;

    Open();
    for(tmpAttrList = Next(); tmpAttrList; tmpAttrList = Next())
    {
        if((tmpExpr = tmpAttrList->Lookup(name)))
        {
            Close();
            return tmpExpr;
        }
    }
    Close();
    return NULL;
}

void AttrListList::UpdateAgg(ExprTree* expr, int operation)
{
    AttrList*	tmpAttrList;	// pointer to each associated AttrList

    if(associatedAttrLists)
    {
    	associatedAttrLists->Open();
    	while((tmpAttrList = associatedAttrLists->Next()))
    	{
	    tmpAttrList->UpdateAgg(expr, operation);
    	}
    	associatedAttrLists->Close();
    }
}

void AttrListList::fPrintAttrListList(FILE* f)
{
    AttrList *tmpAttrList;

    Open();
    for(tmpAttrList = Next(); tmpAttrList; tmpAttrList = Next())
    {
	switch(tmpAttrList->Type())
	{
	    case ATTRLISTENTITY :

        	 tmpAttrList->fPrint(f);
			 break;
	}
        fprintf(f, "\n");
    }
    Close();
}

////////////////////////////////////////////////////////////////////////////////
// Build an aggregate expression with the name "name". If the expression i
// empty, meaning that no expression with such name exists in any of the
// AttrList in the list, NULL is returned. Otherwise, the aggregate expression
// is returned.
////////////////////////////////////////////////////////////////////////////////
ExprTree* AttrListList::BuildAgg(char* name, LexemeType op)
{
    AttrList*	tmpAttrList;	// each AttrList in the list
    ExprTree*	newTree = NULL;	// the newly built aggregate expression
    ExprTree*	tmpTree = NULL;	// tree to be added to the aggregate expression

    Open();
    while((tmpAttrList = Next()))
    {
	switch(op)
	{

	    case LX_AGGADD :

		if((tmpTree = tmpAttrList->Lookup(name)))
		// a branch is found for the new aggregate expression
		{
		    tmpTree->Copy();
		    if(!newTree)
		    {
			newTree = tmpTree;
		    }
		    else
		    {
		        newTree = (ExprTree*)new AggAddOp(newTree, tmpTree);
		    }
		}
		break;

	    case LX_AGGEQ :

		if((tmpTree = tmpAttrList->Lookup(name)))
		// a branch is found for the new aggregate expression
		{
		    tmpTree->Copy();
		    if(!newTree)
		    {
			newTree = tmpTree;
		    }
		    else
		    {
		        newTree = (ExprTree*)new AggEqOp(newTree, tmpTree);
		    }
		}
		break;

	    default :
		
		return NULL;
	}
    }
    Close();

    if(newTree)
    {
	tmpTree = (ExprTree*)new Variable(name);
	newTree = (ExprTree*)new AssignOp(tmpTree, newTree); 
	return newTree;
    }
    return NULL;
}

// shipping functions for AttrList -- added by Lei Cao

int AttrList::put(Stream& s)
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

    line = new char[ATTRLIST_MAX_EXPRESSION];
    for(elem = exprList; elem; elem = elem->next) {
        strcpy(line, "");
        elem->tree->PrintToStr(line);
        if(!s.code(line)) {
            delete [] line;
            return 0;
        }
    }
    delete [] line;

    return 1;
}

int AttrList::get(Stream& s)
{
    ExprTree*       tree;
    char*           line;
    int             numExprs;

    s.decode();

    if(!s.code(numExprs))
        return 0;
    
    line = new char[ATTRLIST_MAX_EXPRESSION];
    for(int i = 0; i < numExprs; i++) {
        if(!s.code(line)) {
            delete [] line;
            return 0;
        }
        
        if(!Parse(line, tree)) {
            if(tree->MyType() == LX_ERROR) {
                delete [] line;
				return 0;
            }
        }
        else {
			delete [] line;
			return 0;
        }
        
        Insert(tree);
        strcpy(line, "");
    }
    delete [] line;

    return 1;
}

#if defined(USE_XDR)
// xdr shipping code
int AttrList::put(XDR *xdrs)
{
    AttrListElem*   elem;
    char*           line;
    int             numExprs = 0;

	xdrs->x_op = XDR_ENCODE;

    //get the number of expressions
    for(elem = exprList; elem; elem = elem->next)
        numExprs++;

	// ship number of expressions
    if(!xdr_int (xdrs, &numExprs))
        return 0;

	// ship expressions themselves
    line = new char[ATTRLIST_MAX_EXPRESSION];
    for(elem = exprList; elem; elem = elem->next) {
        strcpy(line, "");
        elem->tree->PrintToStr(line);
        if(!xdr_mywrapstring (xdrs, &line)) {
            delete [] line;
            return 0;
        }
    }
    delete [] line;

    return 1;
}

int AttrList::get(XDR *xdrs)
{
    ExprTree*       tree;
    char*           line;
    int             numExprs;
	int             errorFlag = 0;

	xdrs->x_op = XDR_DECODE;

    if(!xdr_int (xdrs, &numExprs))
        return 0;
    
    line = new char[ATTRLIST_MAX_EXPRESSION];
	if (!line)
	{
		return 0;
	}

	// if we encounter a parse error, we still read the remaining strings from
	// the xdr stream --- we just don't parse these.  Also, we return a FALSE
	// indicating failure
    for(int i = 0; i < numExprs; i++) 
	{ 
		strcpy(line, "");
		if(!xdr_mywrapstring (xdrs, &line)) {
            delete [] line;
            return 0;
        }
        
		// parse iff no errorFlag
		if (!errorFlag)
		{
			int result = Parse (line, tree);
			if(result == 0 && tree->MyType() != LX_ERROR) 
			{
				Insert (tree);
			}
			else 
			{
				errorFlag = 1;
			}
		}
	}

    delete [] line;

	return (!errorFlag);
}
#endif

int AttrList::code(Stream& s)                                           
{
    if(s.is_encode())
        return put(s);
    else
        return get(s);
}
