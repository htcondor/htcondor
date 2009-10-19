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

// classad.h
//
// Definition of ClassAd classes and ClassAdList class. They are derived from
// AttrList class and AttrListList class respectively.
//

#ifndef _CLASSAD_H
#define _CLASSAD_H

#include "condor_exprtype.h"
#include "condor_ast.h"
#include "condor_attrlist.h"
#include "condor_parser.h"

#define		CLASSAD_MAX_ADTYPE			50

// Forward definition of stream
class Stream;

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
        ClassAd(FILE*,char*,int&,int&,int&);	// Constructor, read from file.
        ClassAd(char *, char);					// Constructor, from string.
		ClassAd(const ClassAd&);				// copy constructor
        virtual ~ClassAd();						// destructor

		ClassAd& operator=(const ClassAd& other);

		// Type operations
        void		SetMyTypeName(const char *); /// my type name set.
        const char*	GetMyTypeName() const;		// my type name returned.
        void 		SetTargetTypeName(const char *);// target type name set.
        const char*	GetTargetTypeName() const;	// target type name returned.
        int			GetMyTypeNumber() const;			// my type number returned.
        int			GetTargetTypeNumber() const;		// target type number returned.

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);

		/*
		 * @param str The newline-delimited string of attribute assignments
		 * @param err_msg Optional buffer for error messages.
		 * @return true on success
		 */
		bool initFromString(char const *str,MyString *err_msg=NULL);

		// misc
		class ClassAd*	FindNext();
			// When printing as_XML, the XML headers and footers
			// are NOT added.  This is so you can wrap a whole set
			// of classads in a single header.  
			// Callers will want to use something like the following:
			//
			// #include "condor_xml_classads.h"
			// ClassAdXMLUnparser unparser;
			// MyString out;
			// unparser.AddXMLFileHeader(out);
			// classad.sPrint(out);
			// unparser.AddXMLFileFooter(out);
        virtual int	fPrint(FILE*);				// print the AttrList to a file
        int	fPrintAsXML(FILE* F);
		int         sPrint(MyString &output);   
		int         sPrintAsXML(MyString &output,StringList *attr_white_list=NULL);
		void		dPrint( int );				// dprintf to given dprintf level

		void		clear( void );				// clear out all attributes

		// poor man's update function until ClassAd Update Protocol  --RR
		 void ExchangeExpressions (class ClassAd *);

    private :

		AdType*		myType;						// my type field.
        AdType*		targetType;					// target type field.
		// (sequence number is stored in attrlist)

		// This function is called to update myType and targetType
		// variables.  It should be called whenever the attributes that
		// these variables are "bound" to may have been updated.
		void updateBoundVariables();
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

bool IsAMatch( const ClassAd *ad1, const ClassAd *ad2 );
bool IsAHalfMatch( const ClassAd *ad1, const ClassAd *ad2 );

#endif
