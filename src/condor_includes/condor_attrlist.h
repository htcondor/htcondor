//******************************************************************************
// attrlist.h
//
// Definition of AttrList classes and AttrListList class.
//
//******************************************************************************

#ifndef _ATTRLIST_H
#define _ATTRLIST_H

#include <stdio.h>

#include "condor_expressions.h"
#include "condor_exprtype.h"
#include "condor_astbase.h"

//for the shipping functions -- added by Lei Cao
#include "stream.h"

enum							// various AttrLists
{
    ATTRLISTENTITY,
    ATTRLISTREP
};
enum {AGG_INSERT, AGG_REMOVE};	// operations on aggregate classes

class AttrListElem
{
    public :

        AttrListElem(ExprTree*);			// constructor
        AttrListElem(AttrListElem&);		// copy constructor
        ~AttrListElem() { delete tree; }	// destructor

        friend class AttrList;
        friend class ClassAd;
        friend class AttrListList;
  
    private :

        ExprTree*		tree;	// the tree pointed to by this element
		int				dirty;	// has this tree been changed?
		char*			name;	// the name of the tree
        AttrListElem*	next;	// next element in the list
};

class AttrListAbstract
{
    public :

		int		Type() { return type; }		// type of the AttrList

		friend	class		AttrList;
		friend	class		AttrListList;

    protected :

		AttrListAbstract(int);
		virtual ~AttrListAbstract() {}

		int					type;		// type of this AttrList
		class AttrListList*	inList;		// I'm in this AttrList list
		AttrListAbstract*	next;		// next AttrList in the list
		AttrListAbstract*	prev;		// previous AttrList in the list
};

class AttrListRep: public AttrListAbstract
{
    public:

        AttrListRep(AttrList*, AttrListList*);	// constructor

		friend			AttrList;
		friend			AttrListList;

    private:

        AttrList*		attrList;		// the original AttrList 
        AttrListRep*	nextRep;		// next copy of original AttrList 
};

class AttrList : public AttrListAbstract
{
    public :

		AttrList();							// No associated AttrList list
        AttrList(AttrListList*);			// Associated with AttrList list
        AttrList(FILE *, char *, int &);	// Constructor, read from file.
		AttrList(class ProcObj*);			// create from a proc object
		AttrList(CONTEXT*);					// create from a CONTEXT
        AttrList(char *, char);				// Constructor, from string.
        AttrList(AttrList&);				// copy constructor
        virtual ~AttrList();				// destructor

        int        	Insert(ExprTree*);		// insert at the tail
        int			Delete(char*);			// delete the expr with the name

		void		ResetFlags();			// reset the all the dirty flags
		int			UpdateExpr(char*, ExprTree*);	// update an expression

		void		ResetExpr() { this->ptrExpr = exprList; }
		void		ResetName() { this->ptrName = exprList; }
		ExprTree*	NextExpr();					// next unvisited expression
		char*		NextName();					// next unvisited name
        ExprTree*	Lookup(char*);				// look up an expression
		int			IsInList(AttrListList*);	// am I in this AttrList list?

		int			fPrintExpr(FILE*, char*);	// print an expression
        virtual int	fPrint(FILE*);				// print the AttrList to a file

        // shipping functions -- added by Lei Cao
        int put(Stream& s);
        int get(Stream& s);
        int code(Stream& s);

		friend	class	AttrListRep;			// access "next" 
		friend	class	AttrListList;			// access "UpdateAgg()"
		friend	class	ClassAd;

    private :

		// update an aggregate expression if the AttrList list associated with
		// this AttrList is changed
      	int				UpdateAgg(ExprTree*, int);
		// convert a (key, value) pair to an assignment tree. used by the
		// constructor that builds an AttrList from a proc structure.
		ExprTree*		ProcToTree(char*, LexemeType, int, float, char*);
        AttrListElem*	exprList;		// my collection of expressions
		AttrListList*	associatedList;	// the AttrList list I'm associated with
		AttrListElem*	tail;			// used by Insert
		AttrListElem*	ptrExpr;		// used by NextExpr 
		AttrListElem*	ptrName;		// used by NextName
		int				seq;			// sequence number
};

class AttrListList
{
    public:
    
        AttrListList();					// constructor
        AttrListList(AttrListList&);	// copy constructor
        ~AttrListList();				// destructor

        void 	  	OpenList();			// set pointer to the head of the queue
        void 	  	CloseList();		// set pointer to NULL
        AttrList* 	NextAttrList();		// return AttrList pointed to by "ptr"
        ExprTree* 	Lookup(char*, AttrList*&);	// look up an expression
      	ExprTree* 	Lookup(char*);

      	void 	  	Insert(AttrList*); 		// insert at the tail of the list
      	int			Delete(AttrList*); 		// delete a AttrList

      	void  	  	fPrintAttrListList(FILE *); 	// print out the list
      	int 	  	MyLength() { return length; } 	// length of this list
      	ExprTree* 	BuildAgg(char*, LexemeType);	// build aggregate expr

      	friend	  	class		AttrList;
      	friend	  	class		ClassAd;
  
    protected:

        // update aggregate expressions in associated AttrLists
		void				UpdateAgg(ExprTree*, int);

        AttrListAbstract*	head;			// head of the list
        AttrListAbstract*	tail;			// tail of the list
        AttrListAbstract*	ptr;			// used by NextAttrList
        AttrListList*		associatedAttrLists;	// associated AttrLists
        int					length;			// length of the list
};

#endif
