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

// Forward definition of things
class Stream;
class MyString;

#define		ATTRLIST_MAX_EXPRESSION		10240

// Ugly hack for the schedd
void AttrList_setPublishServerTime( bool );

template <class Key, class Value> class HashTable; // forward declaration
class YourString;

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

// An abstract pair returned by unchain.
struct ChainedPair {
	AttrListElem **exprList;
	HashTable<YourString, AttrListElem *> *exprHash;
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
		ChainedPair unchain( void );
		void RestoreChain(const ChainedPair &p);
		void ChainCollapse(bool with_deep_copy = true);

		// ctors/dtor
		AttrList();							// No associated AttrList list
        AttrList(AttrListList*);			// Associated with AttrList list
        AttrList(FILE*,char*,int&,int&,int&);// Constructor, read from file.
        AttrList(const char *, char);		// Constructor, from string.
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
		int Assign(char const *variable, MyString const &value);
		int Assign(char const *variable,char const *value);
		int Assign(char const *variable,int value);
		int Assign(char const *variable,unsigned int value);
		int Assign(char const *variable,long value);
		int Assign(char const *variable,unsigned long value);
		int Assign(char const *variable,float value);
		int Assign(char const *variable,double value);
		int Assign(char const *variable,bool value);

			// Copy value of source_attr in source_ad to target_attr
			// in this ad.  If source_ad is NULL, it defaults to this ad.
			// If source_attr is undefined, target_attr is deleted, if
			// it exists.
		void CopyAttribute(char const *target_attr, char const *source_attr, AttrList *source_ad=NULL );

			// Copy value of source_attr in source_ad to an attribute
			// of the same name in this ad.  Shortcut for
			// CopyAttribute(target_attr,target_attr,source_ad).
		void CopyAttribute(char const *target_attr, AttrList *source_ad );

			// Escape double quotes in a value so that it can be
			// safely turned into a ClassAd string by putting double
			// quotes around it.  This function does _not_ add the
			// surrounding double quotes.
			// Returns the escaped string.
		static char const * EscapeStringValue(char const *val,MyString &buf);

		// Make an expression invisible when serializing the ClassAd.
		// This (hopefully temporary hack) is to prevent the special
		// attributes MyType and MyTargetType from being printed
		// multiple times, once from the hard-coded value, and once
		// from the attrlist itself.
		// Returns the old invisibility state.  If the attribute doesn't
		// exist, it returns the new invisibility state specified by
		// the caller, so a caller wishing to restore state can
		// optimize away the second call if desired, since this
		// function does nothing when the attribute does not exist.
		bool SetInvisible(char const *name,bool make_invisible=true);

		// Returns invisibility state of the specified attribute.
		// (Always returns false if the attribute does not exist.)
		bool GetInvisible(char const *name);

		// Returns true if given attribute is generally known to
		// contain data which is private to the user.  Currently,
		// this is just a hard-coded list global list of attribute
		// names.
		static bool ClassAdAttributeIsPrivate( char const *name );

		// Calls SetInvisible() for each attribute that is generally
		// known to contain private data.  Invisible attributes are
		// excluded when serializing the ClassAd.
		void SetPrivateAttributesInvisible(bool make_invisible);

		// deletion of expressions	
        int			Delete(const char*); 	// delete the expr with the name

		// Set or clear the dirty flag for each expression.
		void        SetDirtyFlag(const char *name, bool dirty);
		void        GetDirtyFlag(const char *name, bool *exists, bool *dirty);
		void		ClearAllDirtyFlags();

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
		int         EvalString (const char *, const class AttrList *, char *) const;
        int         EvalString (const char *, const class AttrList *, char **value) const;
        int         EvalString (const char *, const class AttrList *, MyString & value) const;
		int         EvalInteger (const char *, const class AttrList *, int &) const;
		int         EvalFloat (const char *, const class AttrList *, float &) const;
		int         EvalBool  (const char *, const class AttrList *, int &) const;

		int			IsInList(AttrListList*);	// am I in this AttrList list?

		// output functions
		int			fPrintExpr(FILE*, char*);	// print an expression
		char*		sPrintExpr(char*, unsigned int, const char*); // print expression to buffer
        virtual int	fPrint(FILE*,StringList *attr_white_list=NULL);				// print the AttrList to a file
		int         sPrint(MyString &output);   // put the AttrList in a string. 
		void		dPrint( int );				// dprintf to given dprintf level

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);

		/*
		 * @param str The newline-delimited string of attribute assignments
		 * @param err_msg Optional buffer for error messages.
		 * @return true on success
		 */
		bool initFromString(char const *str,MyString *err_msg);

		void clear( void );

			// Create a list of all ClassAd attribute references made
			// by the value of the specified attribute.  Note that
			// the attribute itself will not be listed as one of the
			// references--only things that it refers to.
		void GetReferences(const char *attribute, 
						   StringList &internal_references, 
						   StringList &external_references) const;
			// Create a list of all ClassAd attribute references made
			// by the given expression.  Returns false if the expression
			// could not be parsed.
		bool GetExprReferences(const char *expr, 
							   StringList &internal_references, 
							   StringList &external_references) const;
		bool IsExternalReference(const char *name, char **simplified_name) const;

		friend	class	AttrListRep;			// access "next" 
		friend	class	AttrListList;			// access "UpdateAgg()"
		friend	class	ClassAd;

		static bool		IsValidAttrName(const char *);
		static bool		IsValidAttrValue(const char *);

    protected :
	    AttrListElem**	chainedAttrs;

		// update an aggregate expression if the AttrList list associated with
		// this AttrList is changed
      	int				UpdateAgg(ExprTree*, int);
        AttrListElem*	exprList;		// my collection of expressions
		AttrListList*	associatedList;	// the AttrList list I'm associated with
		AttrListElem*	tail;			// used by Insert
		AttrListElem*	ptrExpr;		// used by NextExpr and NextDirtyExpr
		bool			ptrExprInChain;		// used by NextExpr and NextDirtyExpr
		AttrListElem*	ptrName;		// used by NextName and NextDirtyName
		bool			ptrNameInChain;		// used by NextName and NextDirtyName

		HashTable<YourString, AttrListElem *> *hash;
		HashTable<YourString, AttrListElem *> *chained_hash;

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

      	void  	  	fPrintAttrListList(FILE *, bool use_xml = false, StringList *attr_white_list=NULL );// print out the list
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
