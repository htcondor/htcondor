//******************************************************************************
// classad.h
//
// Definition of ClassAd classes and ClassAdList class. They are derived from
// AttrList class and AttrListList class respectively.
//
//******************************************************************************

#ifndef _CLASSAD_H
#define _CLASSAD_H

#include <fstream.h>

#include "condor_expressions.h"
#include "condor_exprtype.h"
#include "condor_ast.h"
#include "proc_obj.h"
#include "condor_attrlist.h"

#define		CLASSAD_MAX_ADTYPE			50

//for the shipping functions -- added by Lei Cao
#include "stream.h"

struct AdType                   // type of a ClassAd.
{
    int		number;             // type number, internal thing.
    char*	name;               // type name.
    
    AdType(char * = NULL);      // constructor.
    ~AdType();                  // destructor.
};


class ClassAd : public AttrList
{
    public :

		ClassAd();								// No associated AttrList list
		ClassAd(ProcObj*);						// create from a proc object
		ClassAd(const CONTEXT*);				// create from a CONTEXT
        ClassAd(FILE *, char *, int &);			// Constructor, read from file.
        ClassAd(char *, char);					// Constructor, from string.
		ClassAd(const ClassAd&);				// copy constructor
        virtual ~ClassAd();						// destructor

        virtual int	fPrint(FILE*);				// print the AttrList to a file
        void		SetMyTypeName(char *);		// my type name set.
        char*		GetMyTypeName();			// my type name returned.
        void 		SetTargetTypeName(char *);	// target type name set.
        char*		GetTargetTypeName();		// target type name returned.
        int			GetMyTypeNumber();			// my type number returned.
        int			GetTargetTypeNumber();		// target type number returned.
        int			IsAMatch(ClassAd*);			// tests symmetric match
		friend bool operator==(ClassAd&,ClassAd&);// same as symmetric match
		friend bool operator>=(ClassAd&,ClassAd&);// lhs satisfies rhs
		friend bool operator<=(ClassAd&,ClassAd&);// rhs satisifes lhs

		ClassAd*	FindNext();

        // shipping functions -- added by Lei Cao
        int put(Stream& s);
        int get(Stream& s);
        int code(Stream& s);

		// xdr shipping
		int put (XDR *);
		int get (XDR *);

		// poor man's update function until ClassAd Update Protocol  --RR
		 void ExchangeExpressions (ClassAd *);

    private :

		AdType*		myType;						// my type field.
        AdType*		targetType;					// target type field.
};

class ClassAdList : public AttrListList
{
  public:
	ClassAdList() : AttrListList() {}

	ClassAd*	Next() { return (ClassAd*)AttrListList::Next(); }
	void	Insert(ClassAd* ca) { AttrListList::Insert((AttrList*)ca); }
	int		Delete(ClassAd* ca) { return AttrListList::Delete((AttrList*)ca); }
	ClassAd*	Lookup(const char* name);

	// User supplied function should define the "<" relation and the list
	// is sorted in ascending order
	void        Sort(int(*)(ClassAd*, ClassAd*));

  private:
	void		Sort(int(*)(ClassAd*, ClassAd*), AttrListAbstract*&);
};

#endif
