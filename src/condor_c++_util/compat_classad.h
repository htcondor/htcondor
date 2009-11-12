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

#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif

#include "condor_common.h"
#include "classad/classad_distribution.h"
#include "condor_io.h"

namespace compat_classad {

// This enum is lifted directly from old ClassAds.
typedef enum
{
  // Literals
  LX_VARIABLE,
  LX_INTEGER,
  LX_FLOAT,
  LX_STRING,
  LX_BOOL,
  LX_NULL,
  LX_UNDEFINED,
  LX_ERROR,

  // Operators
  LX_ASSIGN,
  LX_AGGADD,
  LX_AGGEQ,
  LX_AND,
  LX_OR,
  LX_LPAREN,
  LX_RPAREN,
  LX_MACRO,
  LX_META_EQ,
  LX_META_NEQ,
  LX_EQ,
  LX_NEQ,
  LX_LT,
  LX_LE,
  LX_GT,
  LX_GE,
  LX_ADD,
  LX_SUB,
  LX_MULT,
  LX_DIV,
  LX_EOF,

  LX_EXPR,

  LX_TIME,

  LX_FUNCTION,
  LX_SEMICOLON,
  LX_COMMA,

  NOT_KEYWORD
} LexemeType;

// This class is lifted directly from old ClassAds for EvalExprTree()
class EvalResult
{
    public :

    EvalResult();
  	~EvalResult();

		/// copy constructor
	EvalResult(const EvalResult & copyfrom);
		/// assignment operator
	EvalResult & operator=(const EvalResult & rhs);

	void fPrintResult(FILE *); // for debugging

		/// convert to LX_STRING
		/// if value is ERROR or UNDEFINED, do not convert unless force=true
	void toString(bool force=false);

   	union
    	{
   	    int i;
   	    float f;
   	    char* s;
        };
  	LexemeType type;

	bool debug;

	private :
	void deepcopy(const EvalResult & copyfrom);
};

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
	ClassAd(FILE*,char*,int&,int&,int&);	// Constructor, read from file.

		/* This is a pass-through to ClassAd::Insert(). Because we define
		 * our own Insert() below, our parent's Insert() won't be found
		 * by users of this class.
		 */
	bool Insert( const std::string &attrName, classad::ExprTree *expr );

	int Insert(const char *name, classad::ExprTree *expr );

		/** Insert an attribute/value into the ClassAd 
		 *  @param str A string of the form "Attribute = Value"
		 */
	int Insert(const char *str);

		/** Insert an attribute/value into the ClassAd 
		 *  @param expr A string of the form "Attribute = Value"
		 */
	int InsertOrUpdate(const char *expr) { return Insert(expr); }

		/* Insert an attribute/value into the Classad
		 */
	int AssignExpr(char const *name,char const *value);

		/* Insert an attribute/value into the Classad with the
		 * appropriate type.
		 */
	int Assign(char const *name, MyString const &value)
	{ return InsertAttr( name, value.Value()) ? TRUE : FALSE; }

	int Assign(char const *name,char const *value)
	{ return InsertAttr( name, value) ? TRUE : FALSE; }

	int Assign(char const *name,int value)
	{ return InsertAttr( name, value) ? TRUE : FALSE; }

	int Assign(char const *name,unsigned int value)
	{ return InsertAttr( name, (int)value) ? TRUE : FALSE; }

	int Assign(char const *name,long value)
	{ return InsertAttr( name, (int)value) ? TRUE : FALSE; }

	int Assign(char const *name,unsigned long value)
	{ return InsertAttr( name, (int)value) ? TRUE : FALSE; }

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
	int LookupString(const char *name, char *value) const; 

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

		/** Lookup (don't evaluate) an attribute that is an integer.
		 *  @param name The attribute
		 *  @param value The integer
		 *  @return true if the attribute exists and is an integer, false otherwise
		 */

	int LookupInteger(const char *name, int &value) const;
		/** Lookup (don't evaluate) an attribute that is a float.
		 *  @param name The attribute
		 *  @param value The integer
		 *  @return true if the attribute exists and is a float, false otherwise
		 */

	int LookupFloat(const char *name, float &value) const;

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
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a string.
         */
    int EvalString (const char *name, classad::ClassAd *target, char **value);
        /** MyString version of EvalString()
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value A MyString where we the copy the string. We ensure there is enough space. 
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a string.
         */
    int EvalString (const char *name, classad::ClassAd *target, MyString & value);

		/** Lookup and evaluate an attribute in the ClassAd that is an integer
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we the copy the value.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not an integer
		 */
	int EvalInteger (const char *name, classad::ClassAd *target, int &value);

		/** Lookup and evaluate an attribute in the ClassAd that is a float
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we the copy the value. Danger: we just use strcpy.
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a float.
		 */

	int EvalFloat (const char *name, classad::ClassAd *target, float &value);

		/** Lookup and evaluate an attribute in the ClassAd that is a boolean
		 *  @param name The name of the attribute
		 *  @param target A ClassAd to resolve MY or other references
		 *  @param value Where we a 1 (if the value is non-zero) or a 1. 
		 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist 
		 *  but is not a number.
		 */
	int EvalBool  (const char *name, classad::ClassAd *target, int &value);

		/** Set the MyType attribute */
	void		SetMyTypeName(const char *);
		/** Get the value of the MyType attribute */
	const char*	GetMyTypeName() const ;
		/** Set the value of the TargetType attribute */
	void 		SetTargetTypeName(const char *);
		/** Get the value of the TargetType attribtute */
	const char*	GetTargetTypeName() const;

		/** Print the ClassAd as an old ClassAd to the stream
		 * @param s the stream
		 */
	int put(Stream& s);

		/** Read the old ClassAd from the stream, and fill in this ClassAd.
		 * @param s the stream
		 */
	int initFromStream(Stream& s);

		/** Print the ClassAd as an old AttrList to the stream
		 * @param s the stream
		 */
	int putAttrList(Stream& s);

		/** Read the old ClassAd as an old AttrList from the stream,
		 * and fill in this ClassAd.
		 * @param s the stream
		 */
	int initAttrListFromStream(Stream& s);

		/** Print the ClassAd as an old ClassAd to the FILE
			@param file The file handle to print to.
			@return TRUE
		*/
	int	fPrint(FILE *file);

		/** Print the ClassAd as an old ClasAd with dprintf
			@param level The dprintf level.
		*/
	void dPrint( int level);

		/** Format the ClassAd as an old ClassAd into the MyString.
			@param output The MyString to write into
			@return TRUE
		*/
	int sPrint( MyString &output );
        /** Prints an expression with a certain name into a buffer. 
         *
         *  @param buffer The buffer to place the named expression into. NOTE: if this is NULL, then the function returns some malloc'd memory. Free it.
         *  @param buffersize The size of the buffer.
         *  @param name The attr whose expr is to be printed into the buffer.
         *  @return Returns the now-filled buffer.
         */
    char* sPrintExpr(char* buffer, unsigned int buffersize, const char* name);

    /* Prints out the classad as xml to a file.
     * @param fp The file to be printed to.
     * @return TRUE as long as the file existed.
     */
    int fPrintAsXML(FILE *fp);

    /* Prints the current classad as XML to a string. fPrintAsXML calls this.
     * @param output The MyString to have filled with the XML-ified classad.
     * @return TRUE
     */
    int sPrintAsXML(MyString &output);

    void ResetExpr();

	void ResetName();
	const char *NextNameOriginal();

	bool AddExplicitConditionals( classad::ExprTree *expr, classad::ExprTree *&newExpr );
	classad::ClassAd *AddExplicitTargetRefs( );
		//@}

	static bool ClassAdAttributeIsPrivate( char const *name );

    /** Is this value valid for being written to the log? The value is a RHS of an expression. Only '\n' or '\r' are invalid.
     *
     * @param value The thing we check to see if valid.
     * @return True, unless value had '\n' or '\r'.
     */
    bool IsValidAttrValue(const char *value);

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

    /** Basically just calls an Unparser so we can escape strings
     *  @param val The string we're escaping stuff in. 
     *  @return The escaped string.
     */
    char const *EscapeStringValue(char const *val);

    /** Takes the ad this is chained to, copies over all the 
     *  attributes from the parent ad that aren't in this classad
     *  (so attributes in both the parent ad and this ad retain the 
     *  values from this ad), and then makes this ad not chained to
     *  the parent.
     */
    void ChainCollapse();


    /*
    bool GetInternalReferences( const ExprTree *tree, classad::References &refs, bool fullNames);


    bool _GetInternalReferences( const ExprTree *expr, ClassAd *ad,
            classad::EvalState &state, classad::References& refs, bool fullNames);

    */

 private:
	void evalFromEnvironment( const char *name, classad::Value val );
	classad::ExprTree *AddExplicitConditionals( classad::ExprTree * );
	classad::ExprTree *AddExplicitTargetRefs( classad::ExprTree *,
						std::set < std::string, classad::CaseIgnLTStr > & );

	classad::ClassAd::iterator m_nameItr;
	bool m_nameItrInChain;

	classad::ClassAd::iterator m_exprItr;
	bool m_exprItrInChain;

    classad::DirtyAttrList::iterator m_dirtyItr;
    bool m_dirtyItrInit;
};

typedef ClassAd AttrList;

} // namespace compat_classad

#endif
