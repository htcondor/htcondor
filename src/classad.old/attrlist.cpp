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
#include "my_hostname.h"
#include "HashTable.h"
#include "YourString.h"

extern void evalFromEnvironment (const char *, EvalResult *);


// Chris Torek's world famous hashing function
// Modified to be case-insensitive
static unsigned int torekHash(const YourString &s) {
	unsigned int hash = 0;

	const char *p = s.s;
	while (*p) {
		hash = (hash<<5)+hash + (unsigned char)tolower(*p);
		p++;
	}

	return hash;
}

static const int hash_size = 79; // prime research

static const char *SECRET_MARKER = "ZKM"; // "it's a Zecret Klassad, Mon!"
static bool publish_server_time = false;
void AttrList_setPublishServerTime( bool publish )
{
	publish_server_time = publish;
}


///////////////////////////////////////////////////////////////////////////////
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
AttrListAbstract::AttrListAbstract(int atype)
{
    this->type = atype;
    this->inList = NULL;
    this->next = NULL;
    this->prev = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AttrListRep constructor.
////////////////////////////////////////////////////////////////////////////////
AttrListRep::AttrListRep(AttrList* aList, AttrListList* attrListList)
: AttrListAbstract(ATTRLISTREP)
{
    this->attrList = aList;
    this->inList = attrListList;
    this->nextRep = (AttrListRep *)aList->next;
    attrList->inList = NULL;
    attrList->next = this;
}

////////////////////////////////////////////////////////////////////////////////
// AttrList class constructor when there is no AttrListList associated with it.
////////////////////////////////////////////////////////////////////////////////
AttrList::AttrList() : AttrListAbstract(ATTRLISTENTITY)
{
    exprList = NULL;
	hash = new HashTable<YourString, AttrListElem *>(hash_size, torekHash);
	chained_hash = NULL;
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
AttrList::AttrList(AttrListList* assocList) :
		  AttrListAbstract(ATTRLISTENTITY)
{
    exprList = NULL;
	hash = new HashTable<YourString, AttrListElem *>(hash_size, torekHash);
	chained_hash = NULL;
	inside_insert = false;
	chainedAttrs = NULL;
    tail = NULL;
    ptrExpr = NULL;
    ptrName = NULL;
    ptrExprInChain = false;
    ptrNameInChain = false;
    this->associatedList = assocList;
    if(associatedList)
    {
        if(!associatedList->associatedAttrLists)
        {
      	    assocList->associatedAttrLists = new AttrListList;
        }
        assocList->associatedAttrLists->Insert(this);
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
	int			delimLen = strlen( delimitor );
	int 		index;
    MyString    line_buffer;

    exprList 		= NULL;
	hash = new HashTable<YourString, AttrListElem *>(hash_size, torekHash);
	inside_insert = false;
	chainedAttrs = NULL;
	chained_hash = NULL;
    associatedList 	= NULL;
    tail 			= NULL;
    ptrExpr 		= NULL;
    ptrName 		= NULL;
    ptrExprInChain = false;
    ptrNameInChain = false;
	empty			= TRUE;

	while( 1 ) {
			// get a line from the file
		if ( line_buffer.readLine( file, false ) == false ) {
			error = ( isEOF = feof( file ) ) ? 0 : errno;
			return;
		}

			// did we hit the delimitor?
		if ( strncmp( line_buffer.Value(), delimitor, delimLen ) == 0 ) {
				// yes ... stop
			isEOF = feof( file );
			error = 0;
			return;
		}

			// Skip any leading white-space
		index = 0;
		while ( index < line_buffer.Length() &&
				( line_buffer[index] == ' ' || line_buffer[index] == '\t' ) ) {
			index++;
		}

			// if the rest of the string is empty, try reading again
			// if it starts with a pound character ("#"), treat as a comment
		if( index == line_buffer.Length() || line_buffer[index] == '\n' ||
			line_buffer[index] == '#' ) {
			continue;
		}

			// parse the expression in the string
		if( Parse( line_buffer.Value(), tree ) || Insert( tree ) == FALSE ) {
				// print out where we barfed to the log file
			dprintf(D_ALWAYS,"failed to create classad; bad expr = %s\n",
				line_buffer.Value());
				// read until delimitor or EOF; whichever comes first
			line_buffer = "";
			while ( strncmp( line_buffer.Value(), delimitor, delimLen ) &&
					!feof( file ) ) {
				line_buffer.readLine( file, false );
			}
			isEOF = feof( file );
			error = -1;
			return;
		} else {
			empty = FALSE;
		}
	}
	ClearAllDirtyFlags();
	return;
}

//
// Constructor of AttrList class, read from a string.
// The character 'delimitor' passed in or end of string delimits an expression,
// end of string delimits a AttrList input.
// If there are only white spaces between the last delimitor and the end of 
// string, they are to be ignored, no parse error.
//
AttrList::AttrList(const char *AttrLists, char delimitor) : AttrListAbstract(ATTRLISTENTITY)
{
    ExprTree *tree;

    exprList = NULL;
	hash = new HashTable<YourString, AttrListElem *>(hash_size, torekHash);
	chained_hash = NULL;
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
    while(isspace(c = AttrLists[i]))
    {
        i++;                                 // skip white spaces.
    }    

    while(1)                                 // loop character by character 
    {                                        // to the end of the stirng to
        c = AttrLists[i];                    // construct a AttrList object.
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
			while(isspace(c = AttrLists[i]))
			{
				i++;                         // skip white spaces.
			}
            if((c = AttrLists[i]) == '\0')
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
	associatedList = NULL;
	tail = NULL;
	ptrExpr = NULL;
	ptrName = NULL;
    ptrExprInChain = false;
    ptrNameInChain = false;
	exprList = NULL;
	hash = new HashTable<YourString, AttrListElem *>(hash_size, torekHash);
	chained_hash = NULL;
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

	this->hash = new HashTable<YourString, AttrListElem *>(hash_size,torekHash);
    if(old.exprList) {
		// copy all the AttrList elements. 
		// As of 14-Sep-2001, we now copy the trees, not just the pointers
		// to the trees. This happens in the copy constructor for AttrListElem
		this->exprList = new AttrListElem(*old.exprList);
		hash->insert(((Variable *)this->exprList->tree->LArg())->Name(),
				this->exprList);
		tmpThis = this->exprList;
        for(tmpOld = old.exprList->next; tmpOld; tmpOld = tmpOld->next) {
			tmpThis->next = new AttrListElem(*tmpOld);
			hash->insert(((Variable *)tmpThis->next->tree->LArg())->Name(),
				tmpThis->next);
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
	chained_hash = old.chained_hash;
	this->inside_insert = false;
    this->ptrExpr = NULL;
    this->ptrName = NULL;
    this->ptrExprInChain = false;
    this->ptrNameInChain = false;
    this->associatedList = old.associatedList;
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
	
		// Free memory associated with the hash table
	if ( hash ) {
		delete hash;
		hash = NULL;
	}

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

		if ( !this->hash ) {
			// should not happen, but just in case...
			this->hash = new HashTable<YourString, AttrListElem *>(hash_size, torekHash);
		}

		if(associatedList) {
			associatedList->associatedAttrLists->Delete(this);
		}

		// Then copy over the new stuff 
		AttrListElem* tmpOther;	// working variable
		AttrListElem* tmpThis;	// working variable
		
		if(other.exprList) {
			this->exprList = new AttrListElem(*other.exprList);
			tmpThis = this->exprList;
			hash->insert(((Variable *)tmpThis->tree->LArg())->Name(),
				tmpThis);
			for(tmpOther = other.exprList->next; tmpOther; tmpOther = tmpOther->next) {
				tmpThis->next = new AttrListElem(*tmpOther);
				tmpThis = tmpThis->next;
				hash->insert(((Variable *)tmpThis->tree->LArg())->Name(),
					tmpThis);
			}
			tmpThis->next = NULL;
			this->tail = tmpThis;
		}
		else {
			this->exprList = NULL;
			this->tail = NULL;
		}

		this->chainedAttrs = other.chainedAttrs;
		this->chained_hash = other.chained_hash;
		this->inside_insert = false;
		this->ptrExpr = NULL;
		this->ptrName = NULL;
    	this->ptrExprInChain = false;
    	this->ptrNameInChain = false;
		this->associatedList = other.associatedList;
		if (this->associatedList) {
			this->associatedList->associatedAttrLists->Insert(this);
		}
		
	}
	return *this;
}


////////////////////////////////////////////////////////////////////////////////
// Insert an expression tree into a AttrList. If the expression is not an
// assignment expression, FALSE is returned. Otherwise, it is inserted at the
// end of the expression list. 
////////////////////////////////////////////////////////////////////////////////
int AttrList::Insert(const char* str, bool check_for_dups)
{
	ExprTree*	tree = NULL;
	int result = FALSE;

	if(Parse(str, tree) != 0 || !tree)
	{
		if( tree ) {
			delete tree;
		}
		return FALSE;
	}
	result = Insert(tree, check_for_dups);
	if ( result == FALSE ) {
		delete tree;
	}
	return result;
}

int AttrList::Insert(ExprTree* expr, bool check_for_dups)
{
    if(expr == NULL || expr->MyType() != LX_ASSIGN ||
	   expr->LArg()->MyType() != LX_VARIABLE)
    {
		return FALSE;
    }

	inside_insert = true;

	if(check_for_dups && Lookup(expr->LArg()))
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

	hash->insert(((Variable *)newNode->tree->LArg())->Name(),
				newNode);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Delete an expression with the name "name" from this AttrList. Return TRUE if
// successful; FALSE if the expression can not be found.
////////////////////////////////////////////////////////////////////////////////
int AttrList::Delete(const char* name)
{
    AttrListElem*	previous = exprList;
    AttrListElem*	cur = exprList;
	int found = FALSE;

	hash->remove(name);
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
				previous->next = cur->next;
				if(tail == cur)
				{
					tail = previous;
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
			previous = cur;
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
				AssignExpr(name, 0);	// 0 means "UNDEFINED"
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

void
AttrList::ChainCollapse(bool with_deep_copy)
{
	ExprTree *tmp;

	if (!chainedAttrs) {
		// no chained attributes, we're done
		return;
	}

	AttrListElem* chained = *chainedAttrs;
	
	chainedAttrs = NULL;
	chained_hash = NULL;	// do not delete chained_hash here!

	while (chained && (tmp=chained->tree)) {
			// Move the value from our chained ad into our ad ONLY
			// if it does not already exist --- so we do a Lookup()
			// first, and only Insert() if the Lookup fails.
			// This is because we want attributes in our ad to have
			// precedent over the chained (cluster) ad when we collapse.
		if ( !Lookup(tmp->LArg()) ) {
			if ( with_deep_copy ) {
				tmp = tmp->DeepCopy();
				ASSERT(tmp);
			}
			Insert(tmp,false);	// no need for Insert() to check for dups
		}
		chained = chained->next;
	}
}


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
    AttrListElem*	tmpNode = NULL;

	ASSERT(hash);

	hash->lookup(name, tmpNode);
	if (tmpNode) {
		return tmpNode->tree;
	}

	if (chained_hash && !inside_insert) {
		chained_hash->lookup(name, tmpNode);
		if (tmpNode) {
			return tmpNode->tree;
		}
	}

    return NULL;
}

AttrListElem *AttrList::LookupElem(const char *name) const
{
	AttrListElem  *theElem = NULL;

	hash->lookup(name, theElem);

	if (theElem) {
		return theElem;
	}

	if (chained_hash && !inside_insert) {
		chained_hash->lookup(name, theElem);
	}

    return theElem;
}

ExprTree* AttrList::Lookup(const ExprTree* attr) const
{
	return Lookup (((const Variable*)attr)->Name());
}

int AttrList::LookupString (const char *name, char *value) const
{
	ExprTree *tree, *rhs;
	const char *strVal;

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
	const char *strVal;

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
	const char *strVal;

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

/* This is just a thin wrapper to the mallocing version to simplify
usage.  Having client code need to remember to free memory sucks.
Indeed, it's telling that lots of client code just does LookupString on 
a fixed length buffer, hoping that it will be big enough.
*/
int AttrList::LookupString (const char *name, MyString & value) const
{
	char * results = 0;
	int success = LookupString(name, &results);
	if( success ) value = results;
	free(results);
	return success;
}

int AttrList::LookupTime (const char *name, char **value) const
{
	ExprTree *tree, *rhs;
	const char *strVal;

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
	const char *strVal;
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


int AttrList::EvalString (const char *name, const AttrList *target, char *value) const
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
int AttrList::EvalString (const char *name, const AttrList *target, char **value) const
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

int AttrList::EvalString (const char *name, const AttrList *target, MyString & value) const
{
	char * pvalue = NULL;
	int ret = EvalString(name, target, &pvalue);
	if(ret == 0) { return ret; }
	value = pvalue;
	free(pvalue);
	return ret;
}

int AttrList::EvalInteger (const char *name, const AttrList *target, int &value) const
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

int AttrList::EvalFloat (const char *name, const AttrList *target, float &value) const
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

int AttrList::EvalBool (const char *name, const AttrList *target, int &value) const
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
    MyString	tmpStr;

    if(!tmpExpr)
    // the named expression is not found
    {
	return NULL;
    }

    tmpExpr->PrintToStr(tmpStr);
	if (buffer) {
		strncpy(buffer,tmpStr.Value(),buffersize);
		buffer[buffersize-1] = '\0';
			// Behavior is undefined if buffer is not big enough.
			// Currently, we return TRUE.
	} else {
		if ( (buffer=strdup(tmpStr.Value())) == NULL ) {
			EXCEPT("Out of memory");
		}
	}
    
	return buffer;
}

////////////////////////////////////////////////////////////////////////////////
// print the whole AttrList into a file. The expressions are in infix notation.
// Returns FALSE if the file pointer is NULL; TRUE otherwise.
////////////////////////////////////////////////////////////////////////////////
int AttrList::fPrint(FILE* f,StringList *attr_white_list)
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
			if( tmpElem->tree->invisible ) {
				continue;
			}
			if( attr_white_list && !attr_white_list->contains_anycase(((VariableBase *)tmpElem->tree->LArg())->Name()) ) {
				continue; // not in white-list
			}
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
		if( tmpElem->tree->invisible ) {
			continue;
		}
		if( attr_white_list && !attr_white_list->contains_anycase(((VariableBase *)tmpElem->tree->LArg())->Name()) ) {
			continue; // not in white-list
		}
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
			if( tmpElem->tree->invisible ) {
				continue;
			}
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
		if( tmpElem->tree->invisible ) {
			continue;
		}
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

	if( !(DebugFlags & level) ) {
		return;
	}

	// don't log private stuff into the (probably publicly visible) log file
	SetPrivateAttributesInvisible(true);

	// if this is a chained ad, print out chained attrs first. this is so
	// if this ad is scanned in from a file, the chained attrs will get
	// updated with attrs from this ad in case of duplicates.
	if ( chainedAttrs ) {
		for(tmpElem = *chainedAttrs; tmpElem; tmpElem = tmpElem->next)
		{
			tmpLine = NULL;
			if( tmpElem->tree->invisible ) {
				continue;
			}
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
		if( tmpElem->tree->invisible ) {
			continue;
		}
        tmpElem->tree->PrintToNewStr(&tmpLine);
		if (tmpLine != NULL) {
			dprintf( flag, "%s\n", tmpLine);
			free(tmpLine);
		}
    }

	SetPrivateAttributesInvisible(false);
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
}

////////////////////////////////////////////////////////////////////////////////
// Assume parameter AttrList is not NULL and it's a real AttrList, not a        //
// AttrListRep. This function doesn't do anything if AttrList is not in the     //
// AttrList list.                              				      //
////////////////////////////////////////////////////////////////////////////////
int AttrListList::Delete(AttrList* attrList)
{

		// optimization: if attrList is in this list directly
		// (i.e. not as an AttrListRep), then avoid searching
		// through the list
	if( attrList->inList == this ) {
		if( attrList == ptr ) {
			ptr = ptr->next;
		}

		if( attrList == head && attrList == tail ) {
			head = tail = NULL;
		}
		else if( attrList == head ) {
			head = head->next;
			if( head ) {
				head->prev = NULL;
			}
		}
		else if( attrList == tail ) {
			tail = attrList->prev;
			if( tail ) {
				tail->next = NULL;
			}
		}
		else {
			attrList->prev->next = attrList->next;
			attrList->next->prev = attrList->prev;
		}

		delete attrList;
		this->length--;
		return TRUE;
	}


    AttrListAbstract*	cur;
    AttrListRep*		tmpRep;
    
    for(cur = head; cur; cur = cur->next)
    {
		if(cur->Type() == ATTRLISTREP)
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


void AttrListList::fPrintAttrListList(FILE* f, bool use_xml, StringList *attr_white_list)
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
				unparser.Unparse((ClassAd *) tmpAttrList, xml, attr_white_list);
				printf("%s\n", xml.Value());
				xml = "";
			} else {
				tmpAttrList->fPrint(f,attr_white_list);
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
    for(elem = exprList; elem; elem = elem->next) {
		if( elem->tree->invisible ) {
			continue;
		}
        numExprs++;
	}

	if ( chainedAttrs ) {
		// now count up all the chained ad attrs
		for(elem = *chainedAttrs; elem; elem = elem->next) {
			if( elem->tree->invisible ) {
				continue;
			}
			numExprs++;
		}
	}

	if ( publish_server_time ) {
		// add one for the ATTR_SERVER_TIME expr
		numExprs++;
		send_server_time = true;
	}

    s.encode();

    if(!s.code(numExprs))
        return 0;

	char *line;
	int pass;
	for( pass=0; pass<2; pass++ ) {
		if( pass==0 ) {
				// copy chained attrs first, so if there are
				// duplicates, the get() method will overide the attrs
				// from the chained ad with attrs from this ad.
			if( !chainedAttrs ) {
				continue;
			}
			elem = *chainedAttrs;
		}
		else {
			elem = exprList;
		}

		for(; elem; elem = elem->next) {
			if( elem->tree->invisible ) {
				continue;
			}
			line = NULL;
			elem->tree->PrintToNewStr(&line);
			ConvertDefaultIPToSocketIP(elem->name,&line,s);

			if( ! s.prepare_crypto_for_secret_is_noop() &&
				ClassAdAttributeIsPrivate(elem->name) )
			{
				s.put(SECRET_MARKER); // tell other side we are sending secret
				s.put_secret(line);   // send the secret
			}
			else if(!s.code(line)) {
				free(line);
				return 0;
			}
			free(line);
		}
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

		// Clear out hashtable of attributes. Note we cannot
		// delete the hash table here - we can do that safely
		// only in ~AttrList() [the dtor] since many other
		// methods assume it is never NULL.
	if ( hash ) {
		hash->clear();
	}

		// Now, delete all the attributes in our list
    AttrListElem* tmp;
    for(tmp = exprList; tmp; tmp = exprList) {
        exprList = exprList->next;
        delete tmp;
    }
	exprList = NULL;

	chained_hash = NULL;	// do not delete chained_hash here!
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

bool AttrList::GetExprReferences(const char *expr, 
							 StringList &internal_references, 
							 StringList &external_references) const
{
	ExprTree  *tree = NULL;

		// A common case is that the expression is a simple attribute
		// reference.  For efficiency, handle that case specially.
	tree = Lookup(expr);
	if (tree != NULL) {
			// Unlike AttrList::GetReferences(), the attribute name
			// passed to this function is added to the list of
			// internal references.
		internal_references.append( expr );
		tree->GetReferences(this, internal_references, external_references);
	}
	else {
		if( ParseClassAdRvalExpr( expr, tree ) != 0 || tree == NULL ) {
			return false;
		}
		tree->GetReferences(this, internal_references, external_references);
		delete tree;
	}
	return true;
}

bool AttrList::IsExternalReference(const char *name, char **simplified_name) const
{
	// There are two ways to know if this is an internal or external
	// reference.  
	// 1. If it is prefixed with MY or has no prefix but refers to an 
	//    internal variable definition, it's internal. 
	// 2. If it is prefixed with TARGET or another non-MY prefix, or if
	//    it has no prefix, but there is no other variable it could refer to.
	const char  *prefix = name;
	const char  *rest = name;
	char *seperator;
	bool  is_external;

	if (name == NULL) {
		is_external = false;
	}

	seperator = (char*)strchr(name,'.');

	// We have a prefix, so we examine it. 
	if (seperator) {
		*seperator = '\0';
		rest = seperator + 1;
		if (!stricmp(prefix, "TARGET")) {
			is_external = TRUE;
		}
		else {
			is_external = FALSE;
		}
	} else {
		// No prefix means that we have to see if the name occurs within
		// the attrlist or not. We lookup not only the name.
		if (Lookup(name)) {
			is_external = FALSE;
		}
		else {
			is_external = TRUE;
		}
	}

	if (simplified_name != NULL) {
		if (rest) {
			*simplified_name = strdup(rest);
		} else {
			*simplified_name = NULL;
		}
	} 

	if ( seperator ) {
		*seperator = '.';
	}

	return is_external;
}

int
AttrList::initFromStream(Stream& s)
{
	char const *line;
    int numExprs;
	int succeeded;
	
	succeeded = 1;

	// First, clear our ad so we start with a fresh ClassAd
	clear();
	if ( !hash ) {
		// is hash ever NULL? don't think so, but just in case.
		this->hash = new HashTable<YourString, AttrListElem *>(hash_size, torekHash);
	}

	// Now, read our new set of attributes off the given stream 
    s.decode();

    if(!s.code(numExprs)) {
		dprintf(D_FULLDEBUG,"Failed to read ClassAd size.\n");
        return 0;
	}

	char *secret_line = NULL;
    
    for(int i = 0; i < numExprs; i++) {

		line = NULL;
        if(!s.get_string_ptr(line) || (line == NULL)) {
			dprintf(D_FULLDEBUG,"Failed to read ClassAd expression.\n");
            succeeded = 0;
			break;
        }

		if( strcmp(line,SECRET_MARKER)==0 ) {
			free(secret_line);
			secret_line = NULL;
			if( !s.get_secret(secret_line) ) {
				dprintf(D_FULLDEBUG,"Failed to read encrypted ClassAd expression.\n");
				succeeded = 0;
				break;
			}
			line = secret_line;
		}

		if (!Insert(line)) {
				// this debug message for tracking down initFromStream() bug
			dprintf(D_FULLDEBUG,"Failed to parse ClassAd expression: '%s'\n",
					line);
			succeeded = 0;
			break;
		}
    }
	free(secret_line);

    return succeeded;
}

bool
AttrList::initFromString(char const *str,MyString *err_msg)
{
	bool succeeded = true;

	// First, clear our ad so we start with a fresh ClassAd
	clear();
	if ( !hash ) {
		// is hash ever NULL? don't think so, but just in case.
		this->hash = new HashTable<YourString, AttrListElem *>(hash_size, torekHash);
	}

	char *exprbuf = new char[strlen(str)+1];
	ASSERT( exprbuf );

	while( *str ) {
		while( isspace(*str) ) {
			str++;
		}

		size_t len = strcspn(str,"\n");
		strncpy(exprbuf,str,len);
		exprbuf[len] = '\0';

		if( str[len] == '\n' ) {
			len++;
		}
		str += len;

		if (!Insert(exprbuf)) {
			if( err_msg ) {
				err_msg->sprintf("Failed to parse ClassAd expression: %s",
								 exprbuf);
			}
			else {
				dprintf(D_ALWAYS,"Failed to parse ClassAd expression : %s\n",
						exprbuf);
			}
			succeeded = false;
			break;
		}
	}

	delete [] exprbuf;
	return succeeded;
}


void AttrList::ChainToAd(AttrList *ad)
{
	if (!ad) {
		return;
	}

	chainedAttrs = &( ad->exprList );
	chained_hash = ad->hash;
}


ChainedPair
AttrList::unchain( void )
{
	ChainedPair p;
	p.exprList = chainedAttrs;
	p.exprHash = chained_hash;
	chainedAttrs = NULL;
	chained_hash = NULL;
	return p;
}

void AttrList::RestoreChain(const ChainedPair &p)
{
	this->chainedAttrs = p.exprList;
	this->chained_hash = p.exprHash;
}

/* This is used for %s = %s style constructs */
int AttrList::
AssignExpr(char const *variable,char const *value)
{
	ExprTree *tree = NULL;
	ExprTree *lhs = NULL;
	ExprTree *rhs = NULL;


	if ( ParseClassAdRvalExpr( variable, lhs ) != 0 || lhs == NULL ) {
		delete lhs;
		return FALSE;
	}
	if ( !value ) {
		rhs = new Undefined();
	} else {
		if ( ParseClassAdRvalExpr( value, rhs ) != 0 || rhs == NULL ) {
			delete lhs;
			delete rhs;
			return FALSE;
		}
	}
	tree = new AssignOp( lhs, rhs );
	if ( Insert( tree ) == FALSE ) {
		delete tree;
		return FALSE;
	}
	return TRUE;
}

char const *
AttrList::EscapeStringValue(char const *val,MyString &buf) {
	if( !val || !strchr(val,'"') ) {
		return val;
	}
	buf = val;
	buf.replaceString("\"","\\\"");
	return buf.Value();
}

/* This is used for %s = "%s" style constructs */
int AttrList::
Assign(char const *variable, MyString const &value)
{
	return Assign(variable,value.Value());
}

/* This is used for %s = "%s" style constructs */
int AttrList::
Assign(char const *variable,char const *value)
{
	ExprTree *tree = NULL;
	ExprTree *lhs = NULL;
	ExprTree *rhs = NULL;

	if ( ParseClassAdRvalExpr( variable, lhs ) != 0 || lhs == NULL ) {
		delete lhs;
		return FALSE;
	}
	if ( !value ) {
		rhs = new Undefined();
	} else {
		/* I apologize for this casting away of const. It's required by
		 * String's use of StringSpace. Nothing will modify the string,
		 * so this is safe to do, though it's ugly.
		 */
		rhs = new String( (char *)value );
	}
	tree = new AssignOp( lhs, rhs );
	if ( Insert( tree ) == FALSE ) {
		delete tree;
		return FALSE;
	}
	return TRUE;
}

int AttrList::
Assign(char const *variable,int value)
{
	ExprTree *tree = NULL;
	ExprTree *lhs = NULL;
	ExprTree *rhs = NULL;

	if ( ParseClassAdRvalExpr( variable, lhs ) != 0 || lhs == NULL ) {
		delete lhs;
		return FALSE;
	}
	rhs = new Integer( value );
	tree = new AssignOp( lhs, rhs );
	if ( Insert( tree ) == FALSE ) {
		delete tree;
		return FALSE;
	}
	return TRUE;
}

int AttrList::
Assign(char const *variable,unsigned int value)
{
	MyString buf;
	if (!IsValidAttrName(variable)) {
		return FALSE;
	}

	buf.sprintf("%s = %u",variable,value);
	return Insert(buf.Value());
}

int AttrList::
Assign(char const *variable,long value)
{
	MyString buf;
	if (!IsValidAttrName(variable)) {
		return FALSE;
	}

	buf.sprintf("%s = %ld",variable,value);
	return Insert(buf.Value());
}

int AttrList::
Assign(char const *variable,unsigned long value)
{
	MyString buf;
	if (!IsValidAttrName(variable)) {
		return FALSE;
	}

	buf.sprintf("%s = %lu",variable,value);
	return Insert(buf.Value());
}

int AttrList::
Assign(char const *variable,float value)
{
	ExprTree *tree = NULL;
	ExprTree *lhs = NULL;
	ExprTree *rhs = NULL;

	if ( ParseClassAdRvalExpr( variable, lhs ) != 0 || lhs == NULL ) {
		delete lhs;
		return FALSE;
	}
	rhs = new Float( value );
	tree = new AssignOp( lhs, rhs );
	if ( Insert( tree ) == FALSE ) {
		delete tree;
		return FALSE;
	}
	return TRUE;
}
int AttrList::
Assign(char const *variable,double value)
{
	ExprTree *tree = NULL;
	ExprTree *lhs = NULL;
	ExprTree *rhs = NULL;

	if ( ParseClassAdRvalExpr( variable, lhs ) != 0 || lhs == NULL ) {
		delete lhs;
		return FALSE;
	}
	rhs = new Float( value );
	tree = new AssignOp( lhs, rhs );
	if ( Insert( tree ) == FALSE ) {
		delete tree;
		return FALSE;
	}
	return TRUE;
}
int AttrList::
Assign(char const *variable,bool value)
{
	ExprTree *tree = NULL;
	ExprTree *lhs = NULL;
	ExprTree *rhs = NULL;

	if ( ParseClassAdRvalExpr( variable, lhs ) != 0 || lhs == NULL ) {
		delete lhs;
		return FALSE;
	}
	rhs = new ClassadBoolean( value ? 1 : 0 );
	tree = new AssignOp( lhs, rhs );
	if ( Insert( tree ) == FALSE ) {
		delete tree;
		return FALSE;
	}
	return TRUE;
}

bool AttrList::SetInvisible(char const *name,bool invisible)
{
	ExprTree *tree = Lookup(name);
	if( tree ) {
		bool old_state = tree->invisible;
		tree->invisible = invisible;
		return old_state;
	}
	return invisible;
}

bool AttrList::GetInvisible(char const *name)
{
	ExprTree *tree = Lookup(name);
	if( tree ) {
		return tree->invisible;
	}
	return false;
}

bool AttrList::ClassAdAttributeIsPrivate( char const *name )
{
		// keep this in sync with SetPrivateAttributesInvisible()
	if( stricmp(name,ATTR_CLAIM_ID) == 0 ) {
			// This attribute contains the secret capability cookie
		return true;
	}
	if( stricmp(name,ATTR_CAPABILITY) == 0 ) {
			// This attribute contains the secret capability cookie
		return true;
	}
	if( stricmp(name,ATTR_CLAIM_IDS) == 0 ) {
			// This attribute contains secret capability cookies
		return true;
	}
	if( stricmp(name,ATTR_TRANSFER_KEY) == 0 ) {
			// This attribute contains the secret file transfer cookie
		return true;
	}
	return false;
}

void AttrList::SetPrivateAttributesInvisible(bool make_invisible)
{
		// keep this in sync with ClassAdAttributeIsPrivate()
	SetInvisible(ATTR_CLAIM_ID,make_invisible);
	SetInvisible(ATTR_CLAIM_IDS,make_invisible);
	SetInvisible(ATTR_CAPABILITY,make_invisible);
	SetInvisible(ATTR_TRANSFER_KEY,make_invisible);
}

//	Decides if a string is a valid attribute name, the LHS
//  of an expression.  As per the manual, valid names:
//
//  Attribute names are sequences of alphabetic characters, digits and 
//  underscores, and may not begin with a digit

/* static */ bool
AttrList::IsValidAttrName(const char *name) {
		// NULL pointer certainly false
	if (!name) {
		return false;
	}

		// Must start with alpha or _
	if (!isalpha(*name) && *name != '_') {
		return false;
	}

	name++;

		// subsequent letters must be alphanum or _
	while (*name) {
		if (!isalnum(*name) && *name != '_') {
			return false;
		}
		name++;
	}

	return true;
}

// Determine if a value is valid to be written to the log. The value
// is a RHS of an expression. According to LogSetAttribute::WriteBody,
// the only invalid character is a '\n'.
bool
AttrList::IsValidAttrValue(const char *value) {
		// NULL value is not invalid, may translate to UNDEFINED.
	if (!value) {
		return true;
	}

		// According to LogSetAttribute::WriteBody, the only invalid
		// character for a value is '\n'.
	while (*value) {
		if (((*value) == '\n') ||
			((*value) == '\r')) {
			return false;
		}
		value++;
	}

	return true;
}

void
AttrList::CopyAttribute(char const *target_attr, AttrList *source_ad )
{
	CopyAttribute(target_attr,target_attr,source_ad);
}

void
AttrList::CopyAttribute(char const *target_attr, char const *source_attr, AttrList *source_ad )
{
	ASSERT( target_attr );
	ASSERT( source_attr );
	if( !source_ad ) {
		source_ad = this;
	}

	ExprTree *e = source_ad->Lookup(source_attr);
	if (e && e->MyType() == LX_ASSIGN && e->RArg()) {
		ExprTree *lhs = new Variable((char *)target_attr);
		ExprTree *rhs = e->RArg()->DeepCopy();
		ASSERT( lhs && rhs );
		ExprTree *assign = new AssignOp(lhs,rhs);
		ASSERT( assign );

		this->Insert(assign);
	} else {
		this->Delete(target_attr);
	}
}
