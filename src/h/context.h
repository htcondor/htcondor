//******************************************************************************
// context.h
//
// Definition of Context classes and ContextList class.
//
//******************************************************************************

#ifndef _CONTEXT_H
#define _CONTEXT_H

#include <fstream.h>

#include "condor_expressions.h"
#include "exprtype.h"
#include "ast.h"

enum {RANGE, FIXED};              // used by VarTypeTable and Sel_best_mach()
enum 				  // various Contexts
{
    CONTEXTENTITY,
    RQSTCONTEXT,
    RSRCCONTEXT,
    CONTEXTREP
};
enum {AGG_INSERT, AGG_REMOVE};    // operations on aggregate classes

class ContextElem
{
    public :

        ContextElem(ExprTree*);		// constructor
        ContextElem(ContextElem&);	// copy constructor
        ~ContextElem() { delete tree; }	// destructor

        friend class Context;
	friend class RqstContext;
	friend class RsrcContext;
  
    private :

        ExprTree*	tree;	// the tree pointed to by this element
	int		dirty;	// has this tree been changed?
	char*		name;	// the name of the tree
        ContextElem*	next;	// next element in the list
};

class ContextAbstract
{
    public :

	int		Type() { return type; }	// type of the context

	friend	class	ContextList;		// access "next"

    protected :

	ContextAbstract(int);
	virtual ~ContextAbstract() {}

	ContextList*		inList;		// I'm in this context list
	int			type;		// type of this context
	ContextAbstract*	next;		// next context in the list
	ContextAbstract*	prev;		// previous context in the list
};

class Context : public ContextAbstract
{
    public :

        Context(ContextList*);	// I have an associated context list
	Context();		// I don't have an associated context list
        Context(Context&);	// copy constructor
        virtual ~Context();	// destructor

        int        	Insert(ExprTree*);	// insert at the tail
        int		Delete(char*);		// delete the expr with the name

	void		ResetFlags();		// reset the all the dirty flags
	int		UpdateExpr(char*, ExprTree*);	// update an expression

	void		ResetExpr() { this->ptrExpr = exprList; }
	ExprTree*	NextExpr();		// next unvisited expression
	void		ResetName() { this->ptrName = exprList; }
	char*		NextName();		// next unvisited name
        ExprTree*	Lookup(char*);		// look up an expression
	int		IsInList(ContextList*); // am I in this context list?

	int		fPrintExpr(FILE*, char*);	// print an expression
        virtual int	fPrintContext(FILE*);	// print the context to a file

	friend	class	ContextRep;		// access "next" 
	friend	class	ContextList;		// access "UpdateAgg()"

    protected :

	// update an aggregate expression if the context list associated with
	// this context is changed
      	int		UpdateAgg(ExprTree*, int);

        ContextElem*	exprList;	// my collection of expressions
        CONTEXT*	oldCONTEXT;	// my peer context
	ContextList*	associatedList;	// the context list I'm associated with
	ContextElem*	tail;		// used by Insert
	ContextElem*	ptrExpr;	// used by NextExpr 
	ContextElem*	ptrName;	// used by NextName
};

class RqstContext: public Context
{
    public :

        RqstContext(ContextList*);
	RqstContext();
        RqstContext(RqstContext&);
        virtual	~RqstContext();

        void		SetRequirement(ExprTree*);	// set requirement expr
	ExprTree*	Requirement();			// access requirement

        virtual	int	fPrintContext(FILE*);

    private :

	ExprTree*	requirement;
};

class RsrcContext: public Context
{
    public:

        RsrcContext(ContextList*);
	RsrcContext();
        RsrcContext(RsrcContext&);
        virtual ~RsrcContext();

        void		SetStart(ExprTree*);	// set start expression
	ExprTree*	Start();		// access start expression

        virtual	int	fPrintContext(FILE*);

    private :

	ExprTree*	start;
};

class ContextRep: public ContextAbstract
{
    public:

        ContextRep(Context*, ContextList*);	// constructor

	friend	class 	Context;
	friend 	class 	RqstContext;
	friend 	class 	RsrcContext;
	friend 	class 	ContextList;

    private:

        Context*	context;		// the original context
        ContextRep*	nextRep;		// next copy of original context
};

class ContextList
{
    public:
    
        ContextList();			// constructor
        ContextList(ContextList&);	// copy constructor
        ~ContextList();			// destructor

        void 	  OpenList();		// set pointer to the head of the queue
        void 	  CloseList();		// set pointer to NULL
        Context*  NextContext();	// return context pointed to by "ptr"
        ExprTree* Lookup(char*, Context*&);	// look up an expression
      	ExprTree* Lookup(char*);

      	void 	  Insert(Context*); 	// insert at the tail of the list
      	int	  Delete(Context*); 	// delete a context

      	void  	  fPrintContList(FILE *);       // print out the context list
      	int 	  MyLength() { return length; } // length of this list
      	ExprTree* BuildAgg(char*, LexemeType);	// build aggregate expr

      	friend	  class		Context;
      	friend	  class		RqstContext;
      	friend	  class		RsrcContext;
  
    private:

        // update aggregate expressions in associated contexts
	void			UpdateAgg(ExprTree*, int);

        ContextAbstract*	head;			// head of the list
        ContextAbstract*	tail;			// tail of the list
        ContextAbstract*	ptr;			// used by NextContext
        ContextList*		associatedContexts;	// associated contexts
        int			length;			// length of the list
};

class Class
{
    public:

	Class(ExprTree*);
	~Class() { delete definition; delete list; }

	ExprTree*	Definition() { return definition; }
	int		ClassRequired(ExprTree*);

	friend		class ClassList;

    private:

	ExprTree*	definition;	// definition of this class
	ContextList*	list;		// contexts belong to this class
	Class*		next;
};

class ClassList
{
    public:

	ClassList() { classes = NULL; numOfClasses = 0; tail = NULL; }
	~ClassList();

	int		BuildClassList(char*, char*);

	int		NumOfClasses() { return numOfClasses; }
	int		ClassRequired(int, ExprTree*);
	int		ClassIndex(ExprTree*);	// index of the class defined
	Class*	IndexClass(int);		// the class indexed

    private:

	Class*	classes;
	Class*	tail;
	int	numOfClasses;				// number of classes
};

#endif
