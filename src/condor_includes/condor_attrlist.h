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
//******************************************************************************
// attrlist.h
//
// Definition of AttrList classes and AttrListList class.
//
//******************************************************************************

#ifndef _ATTRLIST_H
#define _ATTRLIST_H

#include "condor_exprtype.h"
#include "condor_astbase.h"
#include "condor_debug.h"
#include "MyString.h"

#include "stream.h"
#include "list.h"

#define		ATTRLIST_MAX_EXPRESSION		10240

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
        ~AttrListElem() 
			{
				if (tree != NULL) {
				   	delete tree; 
					tree = NULL;
				}
			}

		bool IsDirty(void)            { return dirty;              }
		void SetDirty(bool new_dirty) { dirty = new_dirty; return; }

        friend class AttrList;
        friend class ClassAd;
        friend class AttrListList;
  
    private :

        ExprTree*		tree;	// the tree pointed to by this element
	    bool			dirty;	// has this element been changed?
		char*			name;	// the name of the tree
        class AttrListElem*	next;	// next element in the list
};

class AttrListAbstract
{
    public :

		int		Type() { return type; }		// type of the AttrList

		friend	class		AttrList;
		friend	class		AttrListList;
		friend	class		ClassAd;
		friend	class		ClassAdList;

    protected :

		AttrListAbstract(int);
		virtual ~AttrListAbstract() {}

		int					type;		// type of this AttrList
		class AttrListList*	inList;		// I'm in this AttrList list
		class AttrListAbstract*	next;		// next AttrList in the list
		class AttrListAbstract*	prev;		// previous AttrList in the list
};

class AttrListRep: public AttrListAbstract
{
    public:

        AttrListRep(AttrList*, AttrListList*);	// constructor

		const AttrList* GetOrigAttrList() { return attrList; }

		friend	class		AttrList;
		friend	class		AttrListList;

    private:

        AttrList*		attrList;		// the original AttrList 
        AttrListRep*	nextRep;		// next copy of original AttrList 
};

class AttrList : public AttrListAbstract
{
    public :
	    void ChainToAd( AttrList * );
		void* unchain( void );
		void RestoreChain(void *);

		// ctors/dtor
		AttrList();							// No associated AttrList list
        AttrList(AttrListList*);			// Associated with AttrList list
        AttrList(FILE*,char*,int&,int&,int&);// Constructor, read from file.
//		AttrList(class ProcObj*);			// create from a proc object
//		AttrList(CONTEXT*);					// create from a CONTEXT
        AttrList(char *, char);				// Constructor, from string.
        AttrList(AttrList&);				// copy constructor
        virtual ~AttrList();				// destructor

		AttrList& operator=(const AttrList& other);

		// insert expressions into the ad
        int        	Insert(const char*, 
							bool check_for_dups=true);	// insert at the tail

        int        	Insert(ExprTree*, 
							bool check_for_dups=true);	// insert at the tail

		int			InsertOrUpdate(const char *expr) { return Insert(expr); }

		// The Assign() functions are equivalent to Insert("variable = value"),
		// with string values getting wrapped in quotes.  Strings that happen
		// to contain quotes are handled correctly, as an added plus.
		// AssignExpr() is equivalent to Insert("variable = value") without
		// the value being wrapped in quotes.
		int AssignExpr(char const *variable,char const *value);
		int Assign(char const *variable,char const *value);
		int Assign(char const *variable,int value);
		int Assign(char const *variable,float value);
		int Assign(char const *variable,bool value);

		// deletion of expressions	
        int			Delete(const char*); 	// delete the expr with the name

		// Set or clear the dirty flag for each expression.
		void        SetDirtyFlag(const char *name, bool dirty);
		void        GetDirtyFlag(const char *name, bool *exists, bool *dirty);
		void		ClearAllDirtyFlags();
#if 0
		// to update expression trees
		int			UpdateExpr(char*, ExprTree*);	// update an expression
		int			UpdateExpr(ExprTree*);
#endif

		// for iteration through expressions
		void		ResetExpr() { this->ptrExpr = exprList; this->ptrExprInChain = false; }
		ExprTree*	NextExpr();					// next unvisited expression
		ExprTree*   NextDirtyExpr();

		// for iteration through names (i.e., lhs of the expressions)
		void		ResetName() { this->ptrName = exprList; this->ptrNameInChain = false; }
		char*		NextName();					// next unvisited name
		const char* NextNameOriginal();
		char*       NextDirtyName();

		// lookup values in classads  (for simple assignments)
		ExprTree*   Lookup(char *) const;  		// for convenience
        ExprTree*	Lookup(const char*) const;	// look up an expression
		ExprTree*	Lookup(const ExprTree*) const;
		AttrListElem *LookupElem(const char *name) const;
		int         LookupString(const char *, char *) const; 
		int         LookupString(const char *, char *, int) const; //uses strncpy
		int         LookupString (const char *name, char **value) const;
		int         LookupString (const char *name, MyString & value) const;
		int         LookupTime(const char *name, char **value) const;
		int         LookupTime(const char *name, struct tm *time, bool *is_utc) const;
        int         LookupInteger(const char *, int &) const;
        int         LookupFloat(const char *, float &) const;
        int         LookupBool(const char *, int &) const;
        bool        LookupBool(const char *, bool &) const;

		// evaluate values in classads
		int         EvalString (const char *, class AttrList *, char *);
        int         EvalString (const char *, class AttrList *, char **value);
		int         EvalInteger (const char *, class AttrList *, int &);
		int         EvalFloat (const char *, class AttrList *, float &);
		int         EvalBool  (const char *, class AttrList *, int &);

		int			IsInList(AttrListList*);	// am I in this AttrList list?

		// output functions
		int			fPrintExpr(FILE*, char*);	// print an expression
		char*		sPrintExpr(char*, unsigned int, const char*); // print expression to buffer
        virtual int	fPrint(FILE*);				// print the AttrList to a file
		int         sPrint(MyString &output);   // put the AttrList in a string. 
		void		dPrint( int );				// dprintf to given dprintf level

		// conversion function
//		int         MakeContext (CONTEXT *);    // create a context

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);

		void clear( void );

		void GetReferences(const char *attribute, 
						   StringList &internal_references, 
						   StringList &external_references) const;
		bool IsExternalReference(const char *name, char **simplified_name) const;

#if defined(USE_XDR)
		int put (XDR *);
		int get (XDR *);
#endif

		friend	class	AttrListRep;			// access "next" 
		friend	class	AttrListList;			// access "UpdateAgg()"
		friend	class	ClassAd;

    protected :
	    AttrListElem**	chainedAttrs;

		// update an aggregate expression if the AttrList list associated with
		// this AttrList is changed
      	int				UpdateAgg(ExprTree*, int);
		// convert a (key, value) pair to an assignment tree. used by the
		// constructor that builds an AttrList from a proc structure.
		ExprTree*		ProcToTree(char*, LexemeType, int, float, char*);
        AttrListElem*	exprList;		// my collection of expressions
		AttrListList*	associatedList;	// the AttrList list I'm associated with
		AttrListElem*	tail;			// used by Insert
		AttrListElem*	ptrExpr;		// used by NextExpr and NextDirtyExpr
		bool			ptrExprInChain;		// used by NextExpr and NextDirtyExpr
		AttrListElem*	ptrName;		// used by NextName and NextDirtyName
		bool			ptrNameInChain;		// used by NextName and NextDirtyName
		int				seq;			// sequence number
private:
	bool inside_insert;
};

class AttrListList
{
    public:
    
        AttrListList();					// constructor
        AttrListList(AttrListList&);	// copy constructor
        virtual ~AttrListList();		// destructor

        void 	  	Open();				// set pointer to the head of the queue
        void 	  	Close();			// set pointer to NULL
        AttrList* 	Next();				// return AttrList pointed to by "ptr"
        ExprTree* 	Lookup(const char*, AttrList*&);	// look up an expression
      	ExprTree* 	Lookup(const char*);

      	void 	  	Insert(AttrList*);	// insert at the tail of the list
      	int			Delete(AttrList*); 	// delete a AttrList

      	void  	  	fPrintAttrListList(FILE *, bool use_xml = false);// print out the list
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
        class AttrListList*		associatedAttrLists;	// associated AttrLists
        int					length;			// length of the list
};

#endif
