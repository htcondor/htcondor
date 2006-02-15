/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
// classad.h
//
// Definition of ClassAd classes and ClassAdList class. They are derived from
// AttrList class and AttrListList class respectively.
//

#ifndef _CLASSAD_H
#define _CLASSAD_H

#include <fstream.h>

#include "condor_exprtype.h"
#include "condor_ast.h"
#include "condor_attrlist.h"

#define		CLASSAD_MAX_ADTYPE			50

//for the shipping functions -- added by Lei Cao
#include "stream.h"

struct AdType                   // type of a ClassAd.
{
    int		number;             // type number, internal thing.
    char*	name;               // type name.
    
    AdType(const char * = NULL);      // constructor.
    ~AdType();                  // destructor.
};


class ClassAd : public AttrList
{
    public :

		ClassAd();								// No associated AttrList list
//		ClassAd(ProcObj*);						// create from a proc object
//		ClassAd(const CONTEXT*);				// create from a CONTEXT
        ClassAd(FILE*,char*,int&,int&,int&);	// Constructor, read from file.
        ClassAd(char *, char);					// Constructor, from string.
		ClassAd(const ClassAd&);				// copy constructor
        virtual ~ClassAd();						// destructor

		ClassAd& operator=(const ClassAd& other);

		// Type operations
        void		SetMyTypeName(const char *); /// my type name set.
        const char*	GetMyTypeName();		// my type name returned.
        void 		SetTargetTypeName(const char *);// target type name set.
        const char*	GetTargetTypeName();	// target type name returned.
        int			GetMyTypeNumber();			// my type number returned.
        int			GetTargetTypeNumber();		// target type number returned.

		// Requirement operations
#if 0
		int			SetRequirements(char *);
		void        SetRequirements(ExprTree *);
#endif
		ExprTree	*GetRequirements(void);

		// Ranking operations
#if 0
		int 		SetRankExpr(char *);
		void		SetRankExpr(ExprTree *);
#endif
		ExprTree	*GetRankExpr(void);

		// Sequence numbers
		void		SetSequenceNumber(int);
		int			GetSequenceNumber(void);

		// Matching operations
        int			IsAMatch(class ClassAd*);			  // tests symmetric match
		friend bool operator==(class ClassAd&,class ClassAd&);// same as symmetric match
		friend bool operator>=(class ClassAd&,class ClassAd&);// lhs satisfies rhs
		friend bool operator<=(class ClassAd&,class ClassAd&);// rhs satisifes lhs

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);

#if defined(USE_XDR)
		// xdr shipping
		int put (XDR *);
		int get (XDR *);
#endif

		// misc
		class ClassAd*	FindNext();
        virtual int	fPrint(FILE*);				// print the AttrList to a file
		int         sPrint(MyString &output);   
		void		dPrint( int );				// dprintf to given dprintf level
		void		clear( void );				// clear out all attributes

		// poor man's update function until ClassAd Update Protocol  --RR
		 void ExchangeExpressions (class ClassAd *);

    private :

		AdType*		myType;						// my type field.
        AdType*		targetType;					// target type field.
		// (sequence number is stored in attrlist)
};

typedef int (*SortFunctionType)(AttrList*,AttrList*,void*);

class ClassAdList : public AttrListList
{
  public:
	ClassAdList() : AttrListList() {}

	ClassAd*	Next() { return (ClassAd*)AttrListList::Next(); }
	void		Rewind() { AttrListList::Open(); }
	int			Length() { return AttrListList::MyLength(); }
	void		Insert(ClassAd* ca) { AttrListList::Insert((AttrList*)ca); }
	int			Delete(ClassAd* ca){return AttrListList::Delete((AttrList*)ca);}
	ClassAd*	Lookup(const char* name);

	// User supplied function should define the "<" relation and the list
	// is sorted in ascending order.  User supplied function should
	// return a "1" if relationship is less-than, else 0.
	// NOTE: Sort() is _not_ thread safe!
	void   Sort(SortFunctionType,void* =NULL);

	// Count ads in list that meet constraint.
	int         Count( ExprTree *constraint );

  private:
	void	Sort(SortFunctionType,void*,AttrListAbstract*&);
	static int SortCompare(const void*, const void*);
};

#endif
