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

#include "proc_obj.h"
#include "condor_expressions.h"
#include "exprtype.h"
#include "ast.h"
#include "attrlist.h"

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
		ClassAd(CONTEXT*);						// create from a CONTEXT
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
        int			IsAMatch(ClassAd*);			// tests whether it's a match
												// mutually.
    private :

		AdType*		myType;						// my type field.
        AdType*		targetType;					// target type field.
};

typedef	AttrListList ClassAdList;

#endif
