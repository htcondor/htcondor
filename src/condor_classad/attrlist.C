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
# include "astbase.h"
# include "condor_expressions.h"
# include "attrlist.h"
# include "parser.h"

static char *_FileName_ = __FILE__;         // Used by EXCEPT (see except.h)
extern "C" int _EXCEPT_(char*, ...);

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
    EvalResult *val;

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
        cerr << "Warning : you ran out of memory -- quitting !" << endl;
	exit(1);
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
		delete tree;
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
	        cerr << "Warning : you ran out of memory -- quitting !" << endl;
		exit(1);
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
				cerr << "Warning : you ran out of memory -- quitting !" << endl;
				exit(1);
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
    EvalResult *val;

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
        cerr << "Warning : you ran out of memory -- quitting !" << endl;
	exit(1);
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
		        cerr << "Parse error in the input string -- quitting !" << endl;
			exit(1);
		    }
		}
		else
		{
		    cerr << "Parse error in the input string -- quitting !" << endl;
		    exit(1);
		}
		Insert(tree);
		delete tree;
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
	        cerr << "Warning : you ran out of memory -- quitting !" << endl;
		exit(1);
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
		    cerr << "Warning : you ran out of memory -- quitting !" << endl;
		    exit(1);
		}
	    }
	    buffer[current] = c;
	    current++;
	}
	i++;
    }
}

ExprTree* ProcToTree(char*, LexemeType, int, float, char*);

////////////////////////////////////////////////////////////////////////////////
// Create a AttrList from a proc object.
////////////////////////////////////////////////////////////////////////////////
AttrList::AttrList(ProcObj* procObj) : AttrListAbstract(ATTRLISTENTITY)
{
	ExprTree*	tmpTree;	// trees converted from proc structure fields

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

////////////////////////////////////////////////////////////////////////////////
// Create a AttrList from a CONTEXT.
////////////////////////////////////////////////////////////////////////////////

class Stack
{
	public :

		Stack();
		~Stack();

		ELEM*	Pop();
		void	Push(ELEM*);

	private :

		int		top;
		ELEM*	data[1000];
};

int		IsOperator(ELEM*);
char*	Print_elem(ELEM*, char*);

AttrList::AttrList(CONTEXT* context) : AttrListAbstract(ATTRLISTENTITY)
{
	Stack		stack;
	ELEM*		elem, *lElem, *rElem;
	char*		tmpExpr;
	char*		tmpChar;
	int			i, j;
	ExprTree*	tmpTree;

	associatedList = NULL;
	tail = NULL;
	ptrExpr = NULL;
	ptrName = NULL;

	for(i = 0; i < context->len; i++)
    	for(j = 0; j < (context->data)[i]->len; j++)
      		if(((context->data)[i]->data)[j]->type == ENDMARKER)
			{
				elem = stack.Pop();
				Parse(elem->val.string_val, tmpTree);
				Insert(tmpTree);
      		}
			else if(((context->data)[i]->data)[j]->type == NOT)
			{
				tmpExpr = new char[1000];
				strcpy(tmpExpr, "");
				rElem = stack.Pop();
				strcat(tmpExpr, "(!");
				tmpChar = tmpExpr + 2;
				Print_elem(rElem, tmpChar);
				strcat(tmpExpr, ")");
				elem = (ELEM *)malloc(sizeof(ELEM));
				elem->val.string_val = tmpExpr;
				elem->type = EXPRSTRING;
				stack.Push(elem);
				if(rElem->type == EXPRSTRING)
				{
					delete rElem->val.string_val;
					free(rElem);
        		}
		    }
			else if(IsOperator(((context->data)[i]->data)[j]))
			{
				tmpExpr = new char[1000];
				strcpy(tmpExpr, "");
				rElem = stack.Pop();
				lElem = stack.Pop();
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
				elem = (ELEM *)malloc(sizeof(ELEM));
				elem->val.string_val = tmpExpr;
				elem->type = EXPRSTRING;
				stack.Push(elem);
				if(rElem->type == EXPRSTRING)
				{
					delete rElem->val.string_val;
					free(rElem);
       			}
				if(lElem->type == EXPRSTRING)
				{
					delete lElem->val.string_val;
					free(lElem);
				}
			} else
				stack.Push(((context->data)[i]->data)[j]);
}

// Stack implementation
Stack::~Stack()
{
	while(top >= 0)
	{
		top--;
		if(data[top]->type == EXPRSTRING)
		{
			delete data[top]->val.string_val;
		}
		delete data[top];
	}
}

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

ELEM* Stack::Pop()
{
	top--;
	if(data[top]->type == EXPRSTRING)
	{
		delete data[top]->val.string_val;
	}
	delete data[top];
	return data[top];
}

void Stack::Push(ELEM *elem)
{
  data[top] = elem;
  top++;
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
    case STRING: sprintf(str, "\"%s\"", elem->val.string_val);
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

////////////////////////////////////////////////////////////////////////////////
// Converts a (key word, value) pair from a proc structure to a tree.
////////////////////////////////////////////////////////////////////////////////
ExprTree* AttrList::ProcToTree(char* var, LexemeType t, int i, float f, char* s)
{
	ExprTree*	tmpVarTree;		// Variable node
	ExprTree*	tmpTree;		// Value tree
	char*		tmpStr;			// used to add "" to a string

	tmpVarTree = new Variable(var);
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
	}

	return new AssignOp(tmpVarTree, tmpTree);
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
// end of the expression list.
////////////////////////////////////////////////////////////////////////////////
int AttrList::Insert(ExprTree* expr)
{
    if(expr->MyType() != LX_ASSIGN)
    {
	return FALSE;
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
// Delete an expression with the name "name" from this AttrList. Return TRUE if
// successful; FALSE if the expression can not be found.
////////////////////////////////////////////////////////////////////////////////
int AttrList::Delete(char* name)
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
// Update an expression tree. Find the tree according to its name, then change
// the right hand side to a new tree. Set the dirty flag for that tree to TRUE.
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

    delete tmpTree;
    tree->Copy();
    tmpTree = new AssignOp(new Variable(name), tree);
    Insert(tmpTree);
    delete tmpTree;

    // update associated aggregate expressions
    if(inList)		// this AttrList is in only one AttrList list
    {
	inList->UpdateAgg(tmpTree, AGG_INSERT);
    }
    else if(next)	// this AttrList is in more than one AttrList lists
    {
	AttrListRep* rep = (AttrListRep*)next;
	while(rep)
	{
	    rep->attrList->inList->UpdateAgg(tmpTree, AGG_INSERT);
	}
    }
    return TRUE;
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
    AttrListElem* tmpNode;
    char*	 tmpChar;	// variable name

    for(tmpNode = exprList; tmpNode; tmpNode = tmpNode->next)
    {
	tmpChar = ((Variable*)tmpNode->tree->LArg())->Name();
        if(!strcasecmp(tmpChar, name))
        {
	    delete []tmpChar;
            return(tmpNode->tree);
        }
	delete []tmpChar;
    }
    return NULL;
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
	oldList.OpenList();
	while(tmpAttrList = oldList.NextAttrList())
	{
	    switch(tmpAttrList->Type())
	    {
			case ATTRLISTENTITY :

				Insert(new AttrList(*tmpAttrList));
				break;
	    }
	}
	oldList.CloseList();
    }
}

AttrListList::~AttrListList()
{
    this->OpenList();
    AttrList *tmpAttrList = NextAttrList();

    while(tmpAttrList)
    {
	Delete(tmpAttrList);
	tmpAttrList = NextAttrList();
    }
    this->CloseList();
}

void AttrListList::OpenList()
{
  ptr = head;
}

void AttrListList::CloseList()
{
  ptr = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Returns the pointer to the AttrList in the list pointed to by "ptr".
////////////////////////////////////////////////////////////////////////////////
AttrList *AttrListList::NextAttrList()
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
    while(tmpExpr = AttrList->NextExpr())
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
				while(tmpExpr = ((AttrList*)cur)->NextExpr())
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
				while(tmpExpr = attrList->NextExpr())
				{
					UpdateAgg(tmpExpr, AGG_REMOVE);
				}
				delete cur;
				break;
			}
		} // end of if a rep is used
    } // end of the loop through the AttrListList
    CloseList();
    return TRUE;
}

ExprTree* AttrListList::Lookup(char* name, AttrList*& attrList)
{
    AttrList*	tmpAttrList;
    ExprTree*	tmpExpr;

    OpenList();
    for(tmpAttrList = NextAttrList(); tmpAttrList; tmpAttrList = NextAttrList())
    {
        if(tmpExpr = tmpAttrList->Lookup(name))
        {
            CloseList();
            attrList = tmpAttrList;
            return tmpExpr;
        }
    }
    CloseList();
    return NULL;
}

ExprTree* AttrListList::Lookup(char* name)
{
    AttrList*	tmpAttrList;
    ExprTree*	tmpExpr;

    OpenList();
    for(tmpAttrList = NextAttrList(); tmpAttrList; tmpAttrList = NextAttrList())
    {
        if(tmpExpr = tmpAttrList->Lookup(name))
        {
            CloseList();
            return tmpExpr;
        }
    }
    CloseList();
    return NULL;
}

void AttrListList::UpdateAgg(ExprTree* expr, int operation)
{
    AttrList*	tmpAttrList;	// pointer to each associated AttrList

    if(associatedAttrLists)
    {
    	associatedAttrLists->OpenList();
    	while(tmpAttrList = associatedAttrLists->NextAttrList())
    	{
	    tmpAttrList->UpdateAgg(expr, operation);
    	}
    	associatedAttrLists->CloseList();
    }
}

void AttrListList::fPrintAttrListList(FILE* f)
{
    AttrList *tmpAttrList;

    OpenList();
    for(tmpAttrList = NextAttrList(); tmpAttrList; tmpAttrList = NextAttrList())
    {
	switch(tmpAttrList->Type())
	{
	    case ATTRLISTENTITY :

        	 tmpAttrList->fPrint(f);
			 break;
	}
        fprintf(f, "\n");
    }
    CloseList();
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

    OpenList();
    while(tmpAttrList = NextAttrList())
    {
	switch(op)
	{

	    case LX_AGGADD :

		if(tmpTree = tmpAttrList->Lookup(name))
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

		if(tmpTree = tmpAttrList->Lookup(name))
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
    CloseList();

    if(newTree)
    {
	tmpTree = (ExprTree*)new Variable(name);
	newTree = (ExprTree*)new AssignOp(tmpTree, newTree); 
	return newTree;
    }
    return NULL;
}
