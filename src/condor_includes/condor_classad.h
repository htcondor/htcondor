/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
    
    AdType(char * = NULL);      // constructor.
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

		// Type operations
        void		SetMyTypeName(char *);		// my type name set.
        char*		GetMyTypeName();			// my type name returned.
        void 		SetTargetTypeName(char *);	// target type name set.
        char*		GetTargetTypeName();		// target type name returned.
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

        // shipping functions -- added by Lei Cao
        int put(Stream& s);
        int get(Stream& s);
        int code(Stream& s);

#if defined(USE_XDR)
		// xdr shipping
		int put (XDR *);
		int get (XDR *);
#endif

		// misc
		class ClassAd*	FindNext();
        virtual int	fPrint(FILE*);				// print the AttrList to a file
		void		dPrint( int );				// dprintf to given dprintf level

		// poor man's update function until ClassAd Update Protocol  --RR
		 void ExchangeExpressions (class ClassAd *);

    private :

		AdType*		myType;						// my type field.
        AdType*		targetType;					// target type field.
		// (sequence number is stored in attrlist)
};

typedef int (*SortFunctionType)(AttrListAbstract*,AttrListAbstract*,void*);

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

  private:
	void	Sort(SortFunctionType,void*,AttrListAbstract*&);
	static int SortCompare(const void*, const void*);
};

#endif
