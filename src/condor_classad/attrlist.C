/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
// AttrList.C
//
// Implementation of AttrList classes and AttrListList classes.
//

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_ast.h"
#include "condor_attrlist.h"
#include "condor_attributes.h"
#include "iso_dates.h"
#include "condor_xml_classads.h"
#include "condor_string.h" // for strnewp()

extern void evalFromEnvironment (const char *, EvalResult *);

// ugly, ugly hack.  remove for v6.1.9 -Todd
extern char* mySubSystem;

////////////////////////////////////////////////////////////////////////////////
// AttrListElem constructor.
////////////////////////////////////////////////////////////////////////////////
AttrListElem::AttrListElem(ExprTree* expr)
{
    tree = expr;
    dirty = false;
    name = ((Variable*)expr->LArg())->Name();
    next = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AttrListElem copy constructor. 
////////////////////////////////////////////////////////////////////////////////
AttrListElem::AttrListElem(AttrListElem& oldNode)
{
	// This old lame Copy business doesn't really make a copy
    // oldNode.tree->Copy();
    // this->tree = oldNode.tree;
	// So we do the new DeepCopy():
	this->tree = oldNode.tree->DeepCopy();
    this->dirty = false;
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
	inside_insert = false;
	chainedAttrs = NULL;
    tail = NULL;
    ptrExpr = NULL;
    ptrName = NULL;
    ptrExprInChain = false;
    ptrNameInChain = false;
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
	inside_insert = false;
	chainedAttrs = NULL;
    tail = NULL;
    ptrExpr = NULL;
    ptrName = NULL;
    ptrExprInChain = false;
    ptrNameInChain = false;
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
AttrList::
AttrList(FILE *file, char *delimitor, int &isEOF, int &error, int &empty) 
	: AttrListAbstract(ATTRLISTENTITY)
{
    ExprTree 	*tree;
	char		*delim_buffer;
	int			delimLen = strlen( delimitor );
	int 		index;
    MyString    line_buffer;

	seq 			= 0;
    exprList 		= NULL;
	inside_insert = false;
	chainedAttrs = NULL;
    associatedList 	= NULL;
    tail 			= NULL;
    ptrExpr 		= NULL;
    ptrName 		= NULL;
    ptrExprInChain = false;
    ptrNameInChain = false;
	empty			= TRUE;
    delim_buffer    = NULL;

	delim_buffer = (char *) malloc(delimLen+1);
	while( 1 ) {
			// get a line from the file
		if( fgets( delim_buffer, delimLen+1, file ) == NULL ) {
			error = ( isEOF = feof( file ) ) ? 0 : errno;
            free(delim_buffer);
			return;
		}

			// did we hit the delimitor?
		if( strncmp( delim_buffer, delimitor, delimLen ) == 0 ) {
				// yes ... stop
			isEOF = feof( file );
			error = 0;
            free(delim_buffer);
			return;
		} else {
				// no ... read the rest of the line (into the same buffer)
            line_buffer = delim_buffer;
			if( line_buffer.readLine(file, true) == false) {
				error = ( isEOF = feof( file ) ) ? 0 : errno;
                free(delim_buffer);
				return;
			}
		}

			// if the string is empty, try reading again
		if( line_buffer.Length() == 0 || strcmp( line_buffer.Value(), "\n" ) == 0 ) {
			continue;
		}

			// if the string starts with a pound character ("#"),
			// treat as a comment.
		index = 0;
		while ( line_buffer[index]==' ' || line_buffer[index]=='\t' ) {
			index++;
		}
		if ( line_buffer[index] == '#' ) {
			continue;
		}

			// parse the expression in the string
		if( Parse( line_buffer.Value(), tree ) || Insert( tree ) == FALSE ) {
				// print out where we barfed to the log file
			dprintf(D_ALWAYS,"failed to create classad; bad expr = %s\n",
				line_buffer.Value());
				// read until delimitor or EOF; whichever comes first
            delim_buffer[0] = 0;
			while( strncmp( delim_buffer, delimitor, delimLen ) && !feof( file ) ) {
				fgets( delim_buffer, delimLen+1, file );
			}
			isEOF = feof( file );
			error = -1;
            free(delim_buffer);
			return;
		} else {
			empty = FALSE;
		}
	}
	ClearAllDirtyFlags();
    free(delim_buffer);
	return;
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
	inside_insert = false;
	chainedAttrs = NULL;
    associatedList = NULL;
    tail = NULL;
    ptrExpr = NULL;
    ptrName = NULL;
    ptrExprInChain = false;
    ptrNameInChain = false;

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
				int  old_buffer_size;
				char *new_buffer;
				
				old_buffer_size = buffer_size;
				buffer_size *= 2;
				// Can you believe, someone called realloc on 
				// a buffer that had been new-ed? Now we call
				// new and copy it over with memcpy.--Alain, 23-Sep-2001
				new_buffer = new char[buffer_size];
				if(!new_buffer)
				{
					EXCEPT("Warning: you ran out of memory");
				}
				memset(new_buffer, 0, buffer_size);
				memcpy(new_buffer, buffer, old_buffer_size * sizeof(char));
				delete [] buffer;
				buffer = new_buffer;
			}
			buffer[current] = c;
			current++;
		}
		i++;
    }
	ClearAllDirtyFlags();
	return;
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
	inside_insert = false;
	chainedAttrs = NULL;
	associatedList = NULL;
	tail = NULL;
	ptrExpr = NULL;
	ptrName = NULL;
    ptrExprInChain = false;
    ptrNameInChain = false;


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

#if 0 // don't use CONTEXTs anymore
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
    ptrExprInChain = false;
    ptrNameInChain = false;
	exprList = NULL;
	inside_insert = false;
	chainedAttrs = NULL;

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
#endif

////////////////////////////////////////////////////////////////////////////////
// AttrList class copy constructor.
////////////////////////////////////////////////////////////////////////////////
AttrList::AttrList(AttrList &old) : AttrListAbstract(ATTRLISTENTITY)
{
    AttrListElem* tmpOld;	// working variable
    AttrListElem* tmpThis;	// working variable

    if(old.exprList) {
		// copy all the AttrList elements. 
		// As of 14-Sep-2001, we now copy the trees, not just the pointers
		// to the trees. This happens in the copy constructor for AttrListElem
		this->exprList = new AttrListElem(*old.exprList);
		tmpThis = this->exprList;
        for(tmpOld = old.exprList->next; tmpOld; tmpOld = tmpOld->next) {
			tmpThis->next = new AttrListElem(*tmpOld);
			tmpThis = tmpThis->next;
        }
		tmpThis->next = NULL;
        this->tail = tmpThis;
    }
    else {
        this->exprList = NULL;
        this->tail = NULL;
    }
	this->chainedAttrs = old.chainedAttrs;
	this->inside_insert = false;
    this->ptrExpr = NULL;
    this->ptrName = NULL;
    this->ptrExprInChain = false;
    this->ptrNameInChain = false;
    this->associatedList = old.associatedList;
	this->seq = old.seq;
    if(this->associatedList) {
		this->associatedList->associatedAttrLists->Insert(this);
    }
	return;
}

////////////////////////////////////////////////////////////////////////////////
// AttrList class destructor.
////////////////////////////////////////////////////////////////////////////////
AttrList::~AttrList()
{
		// Delete all of the attributes in this list
	clear();

		// If we're part of an AttrListList (a.k.a. ClassAdList),
		// delete ourselves out of there, too.
    if(associatedList)
    {
		associatedList->associatedAttrLists->Delete(this);
    }
}

AttrList& AttrList::operator=(const AttrList& other)
{
	if (this != &other) {
		// First delete our old stuff.
		clear();

		if(associatedList) {
			associatedList->associatedAttrLists->Delete(this);
		}

		// Then copy over the new stuff 
		AttrListElem* tmpOther;	// working variable
		AttrListElem* tmpThis;	// working variable
		
		if(other.exprList) {
			this->exprList = new AttrListElem(*other.exprList);
			tmpThis = this->exprList;
			for(tmpOther = other.exprList->next; tmpOther; tmpOther = tmpOther->next) {
				tmpThis->next = new AttrListElem(*tmpOther);
				tmpThis = tmpThis->next;
			}
			tmpThis->next = NULL;
			this->tail = tmpThis;
		}
		else {
			this->exprList = NULL;
			this->tail = NULL;
		}
		this->chainedAttrs = other.chainedAttrs;
		this->inside_insert = false;
		this->ptrExpr = NULL;
		this->ptrName = NULL;
    	this->ptrExprInChain = false;
    	this->ptrNameInChain = false;
		this->associatedList = other.associatedList;
		this->seq = other.seq;
		if (this->associatedList) {
			this->associatedList->associatedAttrLists->Insert(this);
		}
		
	}
	return *this;
}


////////////////////////////////////////////////////////////////////////////////
// Insert an expression tree into a AttrList. If the expression is not an
// assignment expression, FALSE is returned. Otherwise, it is inserted at the
// end of the expression list. It is not checked if the attribute already
// exists in the list!
////////////////////////////////////////////////////////////////////////////////
int AttrList::Insert(const char* str)
{
	ExprTree*	tree = NULL;
	int result = FALSE;

	if(Parse(str, tree) != 0)
	{
		return FALSE;
	}
	result = Insert(tree);
	if ( result == FALSE ) {
		delete tree;
	}
	return result;
}

int AttrList::Insert(ExprTree* expr)
{
    if(expr->MyType() != LX_ASSIGN)
    {
		return FALSE;
    }

	inside_insert = true;

	if(Lookup(expr->LArg()))
	{
		Delete(((Variable*)expr->LArg())->Name());
	}

    AttrListElem* newNode = new AttrListElem(expr);

	newNode->SetDirty(true);

    if(!tail)
    {
		exprList = newNode;
    }
    else
    {
		tail->next = newNode;
    }
    tail = newNode;

	inside_insert = false;

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// If the attribute is already in the list, replace it with the new one.
// Otherwise just insert it.
////////////////////////////////////////////////////////////////////////////////
// No more InsertOrUpdate implementation -- we just call Insert()
#if 0
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
#endif

////////////////////////////////////////////////////////////////////////////////
// Delete an expression with the name "name" from this AttrList. Return TRUE if
// successful; FALSE if the expression can not be found.
////////////////////////////////////////////////////////////////////////////////
int AttrList::Delete(const char* name)
{
    AttrListElem*	prev = exprList;
    AttrListElem*	cur = exprList;
	int found = FALSE;

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

			delete cur;
			found = TRUE;
			break;
		}
		else
		// expression to be deleted not found, continue search
		{
			prev = cur;
			cur = cur->next;
		}
    }	// end of while loop to search the expression to be deleted

	// see this attr exists in our chained
	// ad; if so, must insert the attr into this ad as UNDEFINED.
	if ( chainedAttrs && !inside_insert) {
		for (cur = *chainedAttrs; cur; cur = cur->next ) {
			if(!strcasecmp(name, cur->name))
			// expression to be deleted is found
			{
				char buf[400];

				sprintf(buf,"%s=UNDEFINED",name);
				Insert(buf);
				found = TRUE;
				break;
			}
		}
	}


    return found; 
}

void AttrList::SetDirtyFlag(const char *name, bool dirty)
{
	AttrListElem *element;

	element = LookupElem(name);
	if (element != NULL) {
		element->SetDirty(dirty);
	}
	return;
}

void AttrList::GetDirtyFlag(const char *name, bool *exists, bool *dirty)
{
	AttrListElem *element;
	bool  _exists, _dirty;

	element = LookupElem(name);
	if (element == NULL) {
		_exists = false;
		_dirty = false;
	} else {
		_exists = true;
		_dirty = element->IsDirty();
	}

	if (exists) {
		*exists = _exists;
	}
	if (dirty) {
		*dirty = _dirty;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////
// Reset the dirty flags for each expression tree in the AttrList.
///////////////////////////////////////////////////////////////////////////////

void AttrList::ClearAllDirtyFlags()
{
    AttrListElem *current;

    current = exprList;
    while (current != NULL) {
		current->SetDirty(false);
		current = current->next;
    }
	return;
}

////////////////////////////////////////////////////////////////////////////////
// Find an attibute and replace its value with a new value.
////////////////////////////////////////////////////////////////////////////////
#if 0
int AttrList::UpdateExpr(char* name, ExprTree* tree)
{
    ExprTree*	tmpTree;	// the expression to be updated

    if(tree->MyType() == LX_ASSIGN)
    {
		return FALSE;
    }

	inside_insert = true;

    if(!(tmpTree = Lookup(name)))
    {
		return FALSE;
    }

    tree->Copy();
	delete tmpTree->RArg();
	((BinaryOp*)tmpTree)->rArg = tree;

	inside_insert = false;

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
#endif

ExprTree* AttrList::NextExpr()
{
	// After iterating through all the exprs in this ad,
	// get all the exprs in our chained ad as well.
    if (!this->ptrExpr && chainedAttrs && !ptrExprInChain ) {
		ptrExprInChain = true;
		ptrExpr = *chainedAttrs;
	}
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

ExprTree* AttrList::NextDirtyExpr()
{
	ExprTree *expr;

	// Note: no need to check chained attrs here, since in normal
	// practice they will never be dirty.

	expr = NULL;
	// Loop until we find a dirty attribute
	while (ptrExpr != NULL && !ptrExpr->IsDirty()) {
		ptrExpr = ptrExpr->next;
	}
	// If we found a dirty attribute, pull out its expression tree
	// and set us up for the next call to NextDirtyExpr()
	if (ptrExpr != NULL) {
		expr    = ptrExpr->tree;
		ptrExpr = ptrExpr->next;
	}
	return expr;
}

char* AttrList::NextName()
{
	const char *name;

    if( (name=this->NextNameOriginal()) == NULL )
    {
	return NULL;
    }
    else
    {
	char* tmp = new char[strlen(name) + 1];
	strcpy(tmp, name);
	return tmp;
    }
}

const char* AttrList::NextNameOriginal()
{
	char *name;

	// After iterating through all the names in this ad,
	// get all the names in our chained ad as well.
    if (!this->ptrName && chainedAttrs && !ptrNameInChain ) {
		ptrNameInChain = true;
		ptrName = *chainedAttrs;
	}
    if (!this->ptrName) {
		name = NULL;
    }
    else {
		name = ptrName->name;
		ptrName = ptrName->next;
    }
	return name;
}

char *AttrList::NextDirtyName()
{
	char *name;

	// Note: no need to check chained attrs here, since in normal
	// practice they will never be dirty.

	name = NULL;

	// Loop until we find a dirty attribute
	while (ptrName != NULL && !ptrName->IsDirty()) {
		ptrName = ptrName->next;
	}
	// If we found a dirty attribute, pull out its name
	// and set us up for the next call to NextDirtyName()
	if (ptrName != NULL) {
		name    = strnewp(ptrName->name);
		ptrName = ptrName->next;
	}
	return name;
}

////////////////////////////////////////////////////////////////////////////////
// Return the named expression without copying it. Return NULL if the named
// expression is not found.
////////////////////////////////////////////////////////////////////////////////
ExprTree* AttrList::Lookup(char* name) const
{
	return Lookup ((const char *) name);
}

ExprTree* AttrList::Lookup(const char* name) const
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

	// did not find it; check in our chained ad
	if ( chainedAttrs && !inside_insert ) {
		for(tmpNode = *chainedAttrs; tmpNode; tmpNode = tmpNode->next)
		{
			tmpChar = ((Variable*)tmpNode->tree->LArg())->Name();
			if(!strcasecmp(tmpChar, name))
			{
				return(tmpNode->tree);
			}
		}
	}

    return NULL;
}

AttrListElem *AttrList::LookupElem(const char *name) const
{
    AttrListElem  *currentElem, *theElem;
    char          *nodeName;

	theElem = NULL;
    for (currentElem = exprList; currentElem; currentElem = currentElem->next){
		nodeName = ((Variable*)currentElem->tree->LArg())->Name();
        if(!strcasecmp(nodeName, name)) {
            theElem = currentElem;
			break;
        }
    }

	// did not find it; check in our chained ad
	if (theElem == NULL && chainedAttrs && !inside_insert ) {
		for (currentElem = *chainedAttrs; 
			 currentElem; 
			 currentElem = currentElem->next) {

			nodeName = ((Variable*)currentElem->tree->LArg())->Name();
			if(!strcasecmp(nodeName, name)){
				theElem = currentElem;
				break;
			}
		}
	}

    return theElem;
}

ExprTree* AttrList::Lookup(const ExprTree* attr) const
{
	return Lookup (((Variable*)attr)->Name());
}

int AttrList::LookupString (const char *name, char *value) const
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

int AttrList::LookupString (const char *name, char *value, int max_len) const
{
	ExprTree *tree, *rhs;
	char     *strVal;

	tree = Lookup (name);
	if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_STRING) &&
		(strVal = ((String *) rhs)->Value()) && strVal)
	{
		strncpy (value, strVal, max_len);
		return 1;
	}

	return 0;
}

int AttrList::LookupString (const char *name, char **value) const
{
	ExprTree *tree, *rhs;
	char     *strVal;

	tree = Lookup (name);
	if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_STRING) &&
		(strVal = ((String *) rhs)->Value()) && strVal)
	{
		// Unlike the other lame version of this function call, we
		// ensure that we have sufficient space to actually do this. 
		*value = (char *) malloc(strlen(strVal) + 1);
		if (*value != NULL) {
			strcpy(*value, strVal);
			return 1;
		}
		else {
			// We shouldn't ever fail, but what if something gets corrupted
			// and we try to allocate 3billion bytes of storage?
			return 0;
		}
	}

	return 0;
}

int AttrList::LookupTime (const char *name, char **value) const
{
	ExprTree *tree, *rhs;
	char     *strVal;

	tree = Lookup (name);
	if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_TIME) &&
		(strVal = ((String *) rhs)->Value()) && strVal)
	{
		*value = (char *) malloc(strlen(strVal) + 1);
		if (*value != NULL) {
			strcpy(*value, strVal);
			return 1;
		}
		else {
			// We shouldn't ever fail, but what if something gets corrupted
			// and we try to allocate 3billion bytes of storage?
			return 0;
		}
	}

	return 0;
}

int AttrList::LookupTime(const char *name, struct tm *time, bool *is_utc) const
{
	ExprTree *tree, *rhs;
	char     *strVal;
	int      succeeded;

	succeeded = 0;
	if (name != NULL && time != NULL && is_utc != NULL) {
		tree = Lookup (name);
		if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_TIME) &&
			(strVal = ((String *) rhs)->Value()) && strVal) {
			iso8601_to_time(strVal, time, is_utc);
			succeeded = 1;
		}
	}

	return succeeded;
}

int AttrList::LookupInteger (const char *name, int &value) const
{
    ExprTree *tree, *rhs;

    tree = Lookup (name);
    if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_INTEGER))
	{
        value = ((Integer *) rhs)->Value();
        return 1;
    }
    if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_BOOL))
	{
        value = ((ClassadBoolean *) rhs)->Value();
        return 1;
    }

    return 0;
}

int AttrList::LookupFloat (const char *name, float &value) const
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

int AttrList::LookupBool (const char *name, int &value) const
{   
    ExprTree *tree, *rhs;       

    tree = Lookup (name);       
    if (tree && (rhs=tree->RArg()) && (rhs->MyType() == LX_BOOL))    
    {   
        value = ((ClassadBoolean *) rhs)->Value();   
        return 1;       
    }       

    return 0;       
}   


bool AttrList::LookupBool (const char *name, bool &value) const
{   
	int val;
	if( LookupBool(name, val) ) {
		value = (bool)val;
		return true;
	}
	return false;
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

// Unlike the other lame version of this function call, we ensure that
// we allocate the value, to ensure that we have sufficient space.
int AttrList::EvalString (const char *name, AttrList *target, char **value)
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
                *value = (char *) malloc(strlen(val.s) + 1);
                if (*value != NULL) {
                    strcpy(*value, val.s);
                    return 1;
                } else {
                    return 0;
                }
			}
			return 0;
		}
	}
	
	if(tree && tree->EvalTree(this,target,&val) && val.type==LX_STRING && val.s)
	{
		*value = (char *) malloc(strlen(val.s) + 1);
		if (*value != NULL) {
			strcpy(*value, val.s);
			return 1;
		} else {
			return 0;
		}
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
			if (val.type == LX_INTEGER)
			{
				value = (val.i ? 1 : 0);
				return 1;
			}
			return 0;
        }
    }

    if (tree && tree->EvalTree (this, target, &val))
    {
 		switch (val.type)
		{
			case LX_INTEGER: value = (val.i ? 1 : 0); break;
			case LX_FLOAT: value = (val.f ? 1 : 0); break;

			default:
				return 0;
		}
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
// Print an expression with a certain name into a buffer. Returns FALSE if the
// named expression can not be found in this AttrList; TRUE if otherwise.
// The caller should pass the size of the buffer in buffersize.
// If buffer is NULL, then space will be allocated with malloc(), and it needs
// to be free-ed with free() by the user.
////////////////////////////////////////////////////////////////////////////////
char *
AttrList::sPrintExpr(char *buffer, unsigned int buffersize, const char* name)
{
    if(!name)
    {
	return NULL;
    }

    ExprTree*	tmpExpr = Lookup(name);
    char	tmpStr[ATTRLIST_MAX_EXPRESSION] = "";

    if(!tmpExpr)
    // the named expression is not found
    {
	return NULL;
    }

    tmpExpr->PrintToStr(tmpStr);
	if (buffer) {
		strncpy(buffer,tmpStr,buffersize);
	} else {
		if ( (buffer=strdup(tmpStr)) == NULL ) {
			EXCEPT("Out of memory");
		}
	}
    
	return buffer;
}

////////////////////////////////////////////////////////////////////////////////
// print the whole AttrList into a file. The expressions are in infix notation.
// Returns FALSE if the file pointer is NULL; TRUE otherwise.
////////////////////////////////////////////////////////////////////////////////
int AttrList::fPrint(FILE* f)
{
    AttrListElem*	tmpElem;
    char			*tmpLine;

    if(!f)
    {
		return FALSE;
    }

	// if this is a chained ad, print out chained attrs first. this is so
	// if this ad is scanned in from a file, the chained attrs will get
	// updated with attrs from this ad in case of duplicates.
	if ( chainedAttrs ) {
		for(tmpElem = *chainedAttrs; tmpElem; tmpElem = tmpElem->next)
		{
			tmpLine = NULL;
			tmpElem->tree->PrintToNewStr(&tmpLine);
			if (tmpLine != NULL) {
				fprintf(f, "%s\n", tmpLine);
				free(tmpLine);
			}
		}
	}

    for(tmpElem = exprList; tmpElem; tmpElem = tmpElem->next)
    {
		tmpLine = NULL;
        tmpElem->tree->PrintToNewStr(&tmpLine);
		if (tmpLine != NULL) {
			fprintf(f, "%s\n", tmpLine);
			free(tmpLine);
		}
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// print the whole AttrList into a file. The expressions are in infix notation.
// Returns FALSE if the file pointer is NULL; TRUE otherwise.
////////////////////////////////////////////////////////////////////////////////
int AttrList::sPrint(MyString &output)
{
    AttrListElem*	tmpElem;
    char			*tmpLine;

	// if this is a chained ad, print out chained attrs first. this is so
	// if this ad is scanned in from a file, the chained attrs will get
	// updated with attrs from this ad in case of duplicates.
	if ( chainedAttrs ) {
		for(tmpElem = *chainedAttrs; tmpElem; tmpElem = tmpElem->next)
		{
			tmpLine = NULL;
			tmpElem->tree->PrintToNewStr(&tmpLine);
			if (tmpLine != NULL) {
				output += tmpLine;
				output += '\n';
				free(tmpLine);
			}
		}
	}

    for(tmpElem = exprList; tmpElem; tmpElem = tmpElem->next)
    {
		tmpLine = NULL;
        tmpElem->tree->PrintToNewStr(&tmpLine);
		if (tmpLine != NULL) {
			output += tmpLine;
			output += '\n';
			free(tmpLine);
		}
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// print the whole AttrList to the given debug level. The expressions
// are in infix notation.  
////////////////////////////////////////////////////////////////////////////////
void
AttrList::dPrint( int level )
{
    AttrListElem*	tmpElem;
    char			*tmpLine;
	int				flag = D_NOHEADER | level;

	// if this is a chained ad, print out chained attrs first. this is so
	// if this ad is scanned in from a file, the chained attrs will get
	// updated with attrs from this ad in case of duplicates.
	if ( chainedAttrs ) {
		for(tmpElem = *chainedAttrs; tmpElem; tmpElem = tmpElem->next)
		{
			tmpLine = NULL;
			tmpElem->tree->PrintToNewStr(&tmpLine);
			if (tmpLine != NULL) {
				dprintf( flag, "%s\n", tmpLine);
				free(tmpLine);
			}
		}
	}

    for(tmpElem = exprList; tmpElem; tmpElem = tmpElem->next)
    {
		tmpLine = NULL;
        tmpElem->tree->PrintToNewStr(&tmpLine);
		if (tmpLine != NULL) {
			dprintf( flag, "%s\n", tmpLine);
			free(tmpLine);
		}
    }
}


#if 0 // don't use CONTEXTs anymore
//////////////////////////////////////////////////////////////////////////////
// Create a CONTEXT from an AttrList
//////////////////////////////////////////////////////////////////////////////
/*
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
*/
#endif


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
				if(cur == head && cur == tail)
				// AttrList to be deleted is the only entry in the list
				{
					head = NULL;
					tail = NULL;
				}
				else if(cur == head)
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
					// we should be able to say 
					//    cur->prev->next = cur->next
					// but sometimes cur->prev is NULL!!!!  since
					// v6.1 will be completely replacing classads
					// and I've spent quite some time and failed to
					// find the bug, I've just patched the bug
					// here.  -Todd 1/98
					if ( cur->prev == NULL ) {
						// this should not happen, but it did.
						// figure out what cur->prev should be
						// by traversing the next pointers.
						// if we cannot figure it out by 
						// traversing all the next pointers,
						// we'll fall through and SEGV - this is
						// better than an EXCEPT since we'll get a core
						AttrListAbstract *cur1;
						for (cur1=head; cur1; cur1=cur1->next ) {
							if ( cur1->next == cur ) {
								cur->prev = cur1;
								break;
							}
						}
					}
					cur->prev->next = cur->next;
					cur->next->prev = cur->prev;
				}

				delete cur;
    			this->length--;
				break;
			}
		}
		else // a rep is used
		{
			if(((AttrListRep *)cur)->attrList == attrList)
			{
				// this is the AttrList to be deleted
				if(cur == ptr) ptr = ptr->next;
				
				if ( cur != head && cur != tail ) 
				{
					cur->prev->next = cur->next;
					cur->next->prev = cur->prev;
				} else {
					if(cur == head)
					{
						// AttrList to be deleted is at the head of the list
						head = head->next;
						if(head)
						{
							head->prev = NULL;
						}
					}

					if(cur == tail)
					{
						// AttrList to be deleted is at the tail of the list
						tail = cur->prev;
						if(tail)
						{
							tail->next = NULL;
						}
					}
				}

				// delete the rep from the rep list
				tmpRep = (AttrListRep *)((AttrListRep *)cur)->attrList->next;
				if(tmpRep == cur)
				{
					((AttrListRep *)cur)->attrList->next = ((AttrListRep *)cur)->nextRep;
					if ( ((AttrListRep *)cur)->nextRep == NULL ) {
						// here we know this attrlist now no longer exists in any
						// other attrlistlist.  so, since the user has now removed
						// it from all lists, actually delete the ad itself.
						// -Todd Tannenbaum, 9/19/2001 <tannenba@cs.wisc.edu>
						AttrList*	adToRemove = ((AttrListRep *)cur)->attrList;
						if ( adToRemove ) delete adToRemove;
					}

				}
				else
				{
					while(tmpRep->nextRep != cur)
					{
						tmpRep = tmpRep->nextRep;
					}
					tmpRep->nextRep = ((AttrListRep *)cur)->nextRep;
				}

				delete cur;
    			this->length--;
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


void AttrListList::fPrintAttrListList(FILE* f, bool use_xml)
{
    AttrList            *tmpAttrList;
	ClassAdXMLUnparser  unparser;
	MyString            xml;
				
	if (use_xml) {
		unparser.SetUseCompactSpacing(false);
		unparser.AddXMLFileHeader(xml);
		printf("%s\n", xml.Value());
		xml = "";
	}

    Open();
    for(tmpAttrList = Next(); tmpAttrList; tmpAttrList = Next()) {
		switch(tmpAttrList->Type()) {
		case ATTRLISTENTITY :
			if (use_xml) {
				unparser.Unparse((ClassAd *) tmpAttrList, xml);
				printf("%s\n", xml.Value());
				xml = "";
			} else {
				tmpAttrList->fPrint(f);
			}
			break;
		}
        fprintf(f, "\n");
    }
	if (use_xml) {
		unparser.AddXMLFileFooter(xml);
		printf("%s\n", xml.Value());
		xml = "";
	}
    Close();
}


// shipping functions for AttrList -- added by Lei Cao
int AttrList::put(Stream& s)
{
    AttrListElem*   elem;
    int             numExprs = 0;
	bool			send_server_time = false;

    //get the number of expressions
    for(elem = exprList; elem; elem = elem->next)
        numExprs++;

	if ( chainedAttrs ) {
		// now count up all the chained ad attrs
		for(elem = *chainedAttrs; elem; elem = elem->next)
			numExprs++;
	}

	if ( mySubSystem && (strcmp(mySubSystem,"SCHEDD")==0) ) {
		// add one for the ATTR_SERVER_TIME expr
		numExprs++;
		send_server_time = true;
	}

    s.encode();

    if(!s.code(numExprs))
        return 0;

	char *line;
	// copy chained attrs first, so if there are duplicates, the get()
	// method will overide the attrs from the chained ad with attrs
	// from this ad.
	if ( chainedAttrs ) {
		for(elem = *chainedAttrs; elem; elem = elem->next) {
			line = NULL;
			elem->tree->PrintToNewStr(&line);
			if(!s.code(line)) {
				free(line);
				return 0;
			}
			free(line);
		}
	}
    for(elem = exprList; elem; elem = elem->next) {
        line = NULL;
        elem->tree->PrintToNewStr(&line);
        if(!s.code(line)) {
			free(line);
            return 0;
        }
		free(line);
    }
	if ( send_server_time ) {
		// insert in the current time from the server's (schedd)
		// point of view.  this is used so condor_q can compute some
		// time values based upon other attribute values without 
		// worrying about the clocks being different on the condor_schedd
		// machine -vs- the condor_q machine.
		line = (char *) malloc(strlen(ATTR_SERVER_TIME)
							   + 3   // for " = "
							   + 12  // for integer
							   + 1); // for null termination
		sprintf( line, "%s = %ld", ATTR_SERVER_TIME, (long)time(NULL) );
		if(!s.code(line)) {
			free(line);
			return 0;
		}
		free(line);
	}

    return 1;
}


void
AttrList::clear( void )
{
		// First, unchain ourselves, if we're a chained classad
	unchain();

		// Now, delete all the attributes in our list
    AttrListElem* tmp;
    for(tmp = exprList; tmp; tmp = exprList) {
        exprList = exprList->next;
        delete tmp;
    }
	exprList = NULL;
	tail = NULL;
}


void AttrList::GetReferences(const char *attribute, 
							 StringList &internal_references, 
							 StringList &external_references) const
{
	ExprTree  *tree;

	tree = Lookup(attribute);
	if (tree != NULL) {
		tree->GetReferences(this, internal_references, external_references);
	}

	return;
}

bool AttrList::IsExternalReference(const char *name, char **simplified_name) const
{
	// There are two ways to know if this is an internal or external
	// reference.  
	// 1. If it is prefixed with MY or has no prefix but refers to an 
	//    internal variable definition, it's internal. 
	// 2. If it is prefixed with TARGET or another non-MY prefix, or if
	//    it has no prefix, but there is no other variable it could refer to.
	char  *prefix;
	char  *rest;
	int   number_of_fields;
	int   name_length;
	bool  is_external;

	if (name == NULL) {
		is_external = false;
	}

	name_length = strlen(name);
	prefix      = (char *) malloc(name_length + 1);
	rest        = (char *) malloc(name_length + 1);

	number_of_fields = sscanf(name, "%[^.].%s", prefix, rest);

	// We have a prefix, so we examine it. 
	if (number_of_fields == 2) {
		if (!strcmp(prefix, "MY")) {
			is_external = FALSE;
		}
		else {
			is_external = TRUE;
		}
	} else {
		// No prefix means that we have to see if the name occurs within
		// the attrlist or not. We lookup not only the name, but 
		if (Lookup(name)) {
			is_external = FALSE;
		}
		else {
			is_external = TRUE;
		}
	}

	if (simplified_name != NULL) {
		if (number_of_fields == 1) {
			*simplified_name = prefix;
			free(rest);
		} else {
			*simplified_name = rest;
			free(prefix);
		}
	} else {
		free(prefix);
		free(rest);
	}

	return is_external;
}

int
AttrList::initFromStream(Stream& s)
{
	char *line;
    int numExprs;
	int succeeded;
	
	succeeded = 1;

	// First, clear our ad so we start with a fresh ClassAd
	clear();

	// Now, read our new set of attributes off the given stream 
    s.decode();

    if(!s.code(numExprs)) 
        return 0;
    
    for(int i = 0; i < numExprs; i++) {

		// This code used to read into a static buffer. 
		// Happily, cedar will allocate a buffer for us instead. 
		// That is much better, paticularly when people send around 
		// large attributes (like the environment) which are bigger
		// than the static buffer was.--Alain, 1-Nov-2001
		line = NULL;
        if(!s.code(line)) {
            succeeded = 0;
			break;
        }
        
		if (!Insert(line)) {
			succeeded = 0;
			break;
		}
		if (line != NULL) {
			free(line);
		}
    }

    return succeeded;
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

void AttrList::ChainToAd(AttrList *ad)
{
	if (!ad) {
		return;
	}

	chainedAttrs = &( ad->exprList );
}


void*
AttrList::unchain( void )
{
	void* old_value = (void*) chainedAttrs;
	chainedAttrs = NULL;
	return old_value;
}

void AttrList::RestoreChain(void* old_value)
{
	chainedAttrs = (AttrListElem**) old_value;
}

int AttrList::
Assign(char const *variable,char const *value)
{
	MyString buf;

	buf += variable;

	if(!value) {
		buf += " = UNDEFINED";
	}
	else {
		buf += " = \"";
		while(*value) {
			// NOTE: in old classads, we do not escape backslashes.
			// There is a hacky provision for dealing with the case
			// where a backslash comes at the end of the string.
			size_t len = strcspn(value,"\"");

			buf.sprintf_cat("%.*s",len,value);
			value += len;

			if(*value) {
				//escape special characters
				buf += '\\';
				buf += *(value++);
			}
		}
		buf += '\"';
	}

	return Insert(buf.GetCStr());
}
int AttrList::
Assign(char const *variable,int value)
{
	MyString buf;
	buf.sprintf("%s = %d",variable,value);
	return Insert(buf.GetCStr());
}
int AttrList::
Assign(char const *variable,float value)
{
	MyString buf;
	buf.sprintf("%s = %f",variable,value);
	return Insert(buf.GetCStr());
}

