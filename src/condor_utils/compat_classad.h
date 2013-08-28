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

#ifndef COMPAT_CLASSAD_H
#define COMPAT_CLASSAD_H

#include "classad/classad_distribution.h"
#include "MyString.h"
#include "classad_oldnew.h"

class StringList;
class Stream;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE 
#define FALSE 0
#endif

#ifndef ATTRLIST_MAX_EXPRESSION
#define	ATTRLIST_MAX_EXPRESSION 10240
#endif

namespace compat_classad {

typedef enum {
	TargetMachineAttrs,
	TargetJobAttrs,
	TargetScheddAttrs
} TargetAdType;

class ClassAdFileParseHelper;

bool ClassAdAttributeIsPrivate( char const *name );

	/** Print the ClassAd as an old ClassAd to the FILE
		@param file The file handle to print to.
		@return TRUE
	*/
int	fPrintAd(FILE *file, const classad::ClassAd &ad, bool exclude_private = false, StringList *attr_white_list = NULL);

	/** Print the ClassAd as an old ClasAd with dprintf
		@param level The dprintf level.
	*/
void dPrintAd( int level, const classad::ClassAd &ad );

	/** Format the ClassAd as an old ClassAd into the MyString.
		@param output The MyString to write into
		@return TRUE
	*/
int sPrintAd( MyString &output, const classad::ClassAd &ad, bool exclude_private = false, StringList *attr_white_list = NULL );

	/** Format the ClassAd as an old ClassAd into the std::string.
		@param output The std::string to write into
		@return TRUE
	*/
int sPrintAd( std::string &output, const classad::ClassAd &ad, bool exclude_private = false, StringList *attr_white_list = NULL );

class ClassAd : public classad::ClassAd
{
 public:
	ClassAd();

	ClassAd( const ClassAd &ad );

	ClassAd( const classad::ClassAd &ad );

	virtual ~ClassAd();

		/**@name Deprecated functions (only for use within Condor) */
		//@{

		/** A constructor that reads old ClassAds from a FILE */
	ClassAd(FILE*,const char*delim,int&isEOF,int&error,int&empty);	// Constructor, read from file.

		/* helper for constructor that reads from file 
		 * returns number of attributes added, 0 if none, -1 if parse error
		 */
	int InsertFromFile(FILE*, bool& is_eof, int& error, ClassAdFileParseHelper* phelp=NULL);

		/* This is a pass-through to ClassAd::Insert(). Because we define
		 * our own Insert() below, our parent's Insert() won't be found
		 * by users of this class.
		 */
	bool Insert( const std::string &attrName, classad::ExprTree *& expr, bool bCache = true );

	int Insert(const char *name, classad::ExprTree *& expr, bool bCache = true );

		/** Insert an attribute/value into the ClassAd 
		 *  @param str A string of the form "Attribute = Value"
		 */
	int Insert(const char *str);

		/* Insert an attribute/value into the Classad
		 */
	int AssignExpr(char const *name,char const *value);

		/* Insert an attribute/value into the Classad with the
		 * appropriate type.
		 */
	int Assign(char const *name, MyString const &value)
	{ return InsertAttr( name, value.Value()) ? TRUE : FALSE; }

	int Assign(char const *name, std::string const &value)
	{ return InsertAttr( name, value.c_str()) ? TRUE : FALSE; }

	int Assign(char const *name,char const *value);

	int Assign(char const *name,int value)
	{ return InsertAttr( name, value) ? TRUE : FALSE; }

	int Assign(char const *name,unsigned int value)
	{ return InsertAttr( name, (long long)value) ? TRUE : FALSE; }

	int Assign(char const *name,long value)
	{ return InsertAttr( name, value) ? TRUE : FALSE; }

	int Assign(char const *name,long long value)
	{ return InsertAttr( name, value) ? TRUE : FALSE; }

	int Assign(char const *name,unsigned long value)
	{ return InsertAttr( name, (long long)value) ? TRUE : FALSE; }

	int Assign(char const *name,float value)
	{ return InsertAttr( name, (double)value) ? TRUE : FALSE; }

	int Assign(char const *name,double value)
	{ return InsertAttr( name, value) ? TRUE : FALSE; }

	int Assign(char const *name,bool value)
	{ return InsertAttr( name, value) ? TRUE : FALSE; }

		// for iteration through expressions
//		void		ResetExpr();
//		classad::ExprTree*	NextExpr();

		// lookup values in classads  (for simple assignments)
      classad::ExprTree* LookupExpr(const char* name) const
	  { return Lookup( name ); }

		/** Lookup (don't evaluate) an attribute that is a string.
		 *  @param name The attribute
		 *  @param value The string, copied with strcpy (DANGER)
		 *  @return true if the attribute exists and is a string, false otherwise
		 */
//	int LookupString(const char *name, char *value) const; 

		/** Lookup (don't evaluate) an attribute that is a string.
		 *  @param name The attribute
		 *  @param value The string, copied with strncpy
		 *  @param max_len The maximum number of bytes in the string to copy
		 *  @return true if the attribute exists and is a string, false otherwise
		 */
	int LookupString(const char *name, char *value, int max_len) const;

		/** Lookup (don't evaluate) an attribute that is a string.
		 *  @param name The attribute
		 *  @param value The string, allocated with malloc() not new.
		 *  @return true if the attribute exists and is a string, false otherwise
		 */
	int LookupString (const char *name, char **value) const;

		/** Lookup (don't evaluate) an attribute that is a string.
		 *  @param name The attribute
		 *  @param value The string
		 *  @return true if the attribute exists and is a string, false otherwise
		 */
	int LookupString(const char *name, MyString &value) const; 

		/** Lookup (don't evaluate) an attribute that is a string.
		 *  @param name The attribute
		 *  @param value The string
		 *  @return true if the attribute exists and is a string, false otherwise
		 */
	int LookupString(const char *name, std::string &value) const; 

		/** Lookup (don't evaluate) an attribute that is an integer.
		 *  @param name The attribute
		 *  @param value The integer
		 *  @return true if the attribute exists and is an integer, false otherwise
		 */
	int LookupInteger(const char *name, int &value) const;
	int LookupInteger(const char *name, long &value) const;
	int LookupInteger(const char *name, long long &value) const;

		/** Lookup (don't evaluate) an attribute that is a float.
		 *  @param name The attribute
		 *  @param value The integer
		 *  @return true if the attribute exists and is a float, false otherwise
		 */
	int LookupFloat(const char *name, float &value) const;
	int LookupFloat(const char *name, double &value) const;

		/** Lookup (don't evaluate) an attribute that can be considered a boolean
		 *  @param name The attribute
		 *  @param value 0 if the attribute is 0, 1 otherwise
		 *  @return true if the attribute exists and is a boolean/integer, false otherwise
		 */

	int LookupBool(const char *name, int &value) const;

		/** Lookup (don't evaluate) an attribute that can be considered a boolean
		 *  @param name The attribute
		 *  @param value false if the attribute is 0, true otherwise
		 *  @return true if the attribute exists and is a boolean/integer, false otherwise
		 */

	int LookupBool(const char *name, bool &value) const;

		/** Lookup and evaluate an attribute in the ClassAd whose type is not known
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we the copy value
		 *  @return 1 on success, 0 if the attribute doesn't exist
		 */
	int EvalAttr (const char *name, classad::ClassAd *target, classad::Value & value);

		/** Lookup and evaluate an attribute in the ClassAd that is a string
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we the copy the string. Danger: we just use strcpy.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a string.
		 */
	int EvalString (const char *name, classad::ClassAd *target, char *value);

        /** Same as EvalString, but ensures we have enough space for value first.
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we the copy the string. We ensure there is enough space. 
		 *    This parameter is only modified on success.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a string.
         */
    int EvalString (const char *name, classad::ClassAd *target, char **value);
        /** MyString version of EvalString()
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value A MyString where we the copy the string. We ensure there is enough space. 
		 *    This parameter is only modified on success.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a string.
         */
    int EvalString (const char *name, classad::ClassAd *target, MyString & value);

        /** std::string version of EvalString()
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value A std::string where we the copy the string.
		 *    This parameter is only modified on success.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a string.
         */
    int EvalString (const char *name, classad::ClassAd *target, std::string & value);

		/** Lookup and evaluate an attribute in the ClassAd that is an integer
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we the copy the value.
		 *    This parameter is only modified on success.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not an integer
		 */
	int EvalInteger (const char *name, classad::ClassAd *target, long long &value);
	int EvalInteger (const char *name, classad::ClassAd *target, int& value) {
		long long ival = 0;
		int result = EvalInteger(name, target, ival);
		if ( result ) {
			value = (int)ival;
		}
		return result;
	}
	int EvalInteger (const char *name, classad::ClassAd *target, long & value) {
		long long ival = 0;
		int result = EvalInteger(name, target, ival);
		if ( result ) {
			value = (long)ival;
		}
		return result;
	}

		/** Lookup and evaluate an attribute in the ClassAd that is a float
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we the copy the value. Danger: we just use strcpy.
		 *    This parameter is only modified on success.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a float.
		 */

	int EvalFloat (const char *name, classad::ClassAd *target, double &value);
	int EvalFloat (const char *name, classad::ClassAd *target, float &value) {
		double dval = 0.0;
		int result = EvalFloat(name, target, dval);
		if ( result ) {
			value = dval;
		}
		return result;
	}

		/** Lookup and evaluate an attribute in the ClassAd that is a boolean
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we a 1 (if the value is non-zero) or a 1. 
		 *    This parameter is only modified on success.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a number.
		 */
	int EvalBool  (const char *name, classad::ClassAd *target, int &value);

	bool initFromString(char const *str,MyString *err_msg=NULL);

    void ResetExpr();

	void ResetName();
	const char *NextNameOriginal();

	void AddTargetRefs( TargetAdType target_type, bool do_version_check = true );

	bool NextExpr( const char *&name, ExprTree *&value );

    /** Gets the next dirty expression tree
     * @return The ExprTree associated with the next dirty attribute, or null if one does not exist.
     */
    bool NextDirtyExpr(const char *&name, classad::ExprTree *&expr);

	// Set or clear the dirty flag for each expression.
	void SetDirtyFlag(const char *name, bool dirty);
	void GetDirtyFlag(const char *name, bool *exists, bool *dirty);

	// Copy value of source_attr in source_ad to target_attr
	// in this ad.  If source_ad is NULL, it defaults to this ad.
	// If source_attr is undefined, target_attr is deleted, if
	// it exists.
	void CopyAttribute(char const *target_attr, char const *source_attr, classad::ClassAd *source_ad=NULL );

	// Copy value of source_attr in source_ad to an attribute
	// of the same name in this ad.  Shortcut for
	// CopyAttribute(target_attr,target_attr,source_ad).
	void CopyAttribute(char const *target_attr, classad::ClassAd *source_ad );

    /** Takes the ad this is chained to, copies over all the 
     *  attributes from the parent ad that aren't in this classad
     *  (so attributes in both the parent ad and this ad retain the 
     *  values from this ad), and then makes this ad not chained to
     *  the parent.
     */
    void ChainCollapse();

    void GetReferences(const char* attr,
                StringList &internal_refs,
                StringList &external_refs);

    bool GetExprReferences(const char* expr,
                StringList &internal_refs,
                StringList &external_refs);

	static void Reconfig();
	static bool m_initConfig;
	static bool m_strictEvaluation;

 private:
	void evalFromEnvironment( const char *name, classad::Value val );

	enum ItrStateEnum {
		ItrUninitialized,
		ItrInThisAd,
		ItrInChain
	};

	classad::ClassAd::iterator m_nameItr;
	ItrStateEnum m_nameItrState;

	classad::ClassAd::iterator m_exprItr;
	ItrStateEnum m_exprItrState;

    classad::DirtyAttrList::iterator m_dirtyItr;
    bool m_dirtyItrInit;

	void _GetReferences(classad::ExprTree *tree,
						StringList &internal_refs,
						StringList &external_refs);

	// poison Assign of ExprTree* type for public users
	// otherwise the compiler will resolve against the bool overload 
	// and quietly leak the tree.
	int Assign(char const *name,classad::ExprTree * tree)
	{ return Insert(name, tree) ? TRUE : FALSE; }

};

class ClassAdFileParseHelper
{
 public:
	// Some compilers whine when you have virtual methods but not an
	// explicit virtual destructor
	virtual ~ClassAdFileParseHelper() {}
	// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
	virtual int PreParse(std::string & line, ClassAd & ad, FILE* file)=0;
	// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
	virtual int OnParseError(std::string & line, ClassAd & ad, FILE* FILE)=0;
};

// this implements a classad file parse helper that
// ignores lines of blanks and lines that begin with #, 
// treats empty lines as class-ad record separators
class CondorClassAdFileParseHelper : public ClassAdFileParseHelper
{
 public:
	// Some compilers whine when you have virtual methods but not an
	// explicit virtual destructor
	virtual ~CondorClassAdFileParseHelper() {}
	// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
	virtual int PreParse(std::string & line, ClassAd & ad, FILE* file);
	// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
	virtual int OnParseError(std::string & line, ClassAd & ad, FILE* FILE);
	CondorClassAdFileParseHelper(std::string delim) : ad_delimitor(delim) {};
 private:
	std::string ad_delimitor;
};

/** Is this value valid for being written to the log?
 *  The value is a RHS of an expression. Only '\n' or '\r' are invalid.
 *
 *  @param value The thing we check to see if valid.
 *  @return True, unless value had '\n' or '\r'.
 */
bool IsValidAttrValue(const char *value);

/** Decides if a string is a valid attribute name, the LHS
 *  of an expression.  As per the manual, valid names:
 *
 *  Attribute names are sequences of alphabetic characters, digits and 
 *  underscores, and may not begin with a digit.
 *
 *  @param name The thing we check to see if valid.
 *  @return true if valid, else false
 */
bool IsValidAttrName(const char *name);

/* Prints out the classad as xml to a file.
 * @param fp The file to be printed to.
 * @param ad The classad containing the attribute
 * @param An optional white-list of attributes to be printed.
 * @return TRUE as long as the file existed.
 */
int fPrintAdAsXML(FILE *fp, const classad::ClassAd &ad,
				  StringList *attr_white_list = NULL);

/* Prints the classad as XML to a string. fPrintAdAsXML calls this.
 * @param output The string to have filled with the XML-ified classad.
 *   The string is appended to, not overwritten.
 * @param ad The ad to be printed.
 * @param An optional white-list of attributes to be printed.
 * @return TRUE
 */
int sPrintAdAsXML(MyString &output, const classad::ClassAd &ad,
				  StringList *attr_white_list = NULL);
int sPrintAdAsXML(std::string &output, const classad::ClassAd &ad,
				  StringList *attr_white_list = NULL);

/** Given an attribute name, return a buffer containing the name
 *  and it's unevaluated value, like so:
 *    <name> = <expression>
 *  The returned buffer is malloc'd and must be free'd by the
 *  caller.
 *  @param ad The classad containing the attribute
 *  @param name The attr whose expr is to be printed into the buffer.
 *  @return Returns a malloc'd buffer.
 */
char* sPrintExpr(const classad::ClassAd &ad, const char* name);

/** Basically just calls an Unparser so we can escape strings
 *  @param val The string we're escaping stuff in. 
 *  @return The escaped string.
 */
char const *EscapeAdStringValue(char const *val, std::string &buf);

	/** Set the MyType attribute */
void SetMyTypeName(classad::ClassAd &ad, const char *);
	/** Get the value of the MyType attribute */
const char*	GetMyTypeName(const classad::ClassAd &ad);
	/** Set the value of the TargetType attribute */
void SetTargetTypeName(classad::ClassAd &ad, const char *);
	/** Get the value of the TargetType attribtute */
const char*	GetTargetTypeName(const classad::ClassAd& ad);


void getTheMyRef( classad::ClassAd *ad );
void releaseTheMyRef( classad::ClassAd *ad );

classad::MatchClassAd *getTheMatchAd( classad::ClassAd *source,
									  classad::ClassAd *target );
void releaseTheMatchAd();


// Modify all expressions in the given ad, such that if they refer
// to attributes that are not in the current classad and they are not
// scoped, then they are renamed "target.attribute"
void AddExplicitTargetRefs( classad::ClassAd &ad );

// Return a new ExprTree identical to the given one, except that for any
// attributes referred to which are not scoped and don't appear in the
// given set are renamed "target.attribute".
classad::ExprTree *AddExplicitTargetRefs(classad::ExprTree *,
						std::set < std::string, classad::CaseIgnLTStr > & );
						
// Modify all expressions in the given ad, removing the "target" scope
// from all attributes references.
void RemoveExplicitTargetRefs( classad::ClassAd &ad );

// Return a new ExprTree identical to the given one, but removing any
// "target" scope from all attribute references.
// For example, Target.Foo will become Foo.
classad::ExprTree *RemoveExplicitTargetRefs( classad::ExprTree * );


classad::ExprTree *AddTargetRefs( classad::ExprTree *tree,
								  TargetAdType target_type );

const char *ConvertEscapingOldToNew( const char *str );

// appends converted representation of str to buffer
void ConvertEscapingOldToNew( const char *str, std::string &buffer );

typedef ClassAd AttrList;
typedef classad::ExprTree ExprTree;

} // namespace compat_classad

#endif
