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

class CompatClassAd : public classad::ClassAd
{
 public:
	CompatClassAd();

	CompatClassAd( const CompatClassAd &ad );

	virtual ~CompatClassAd();

		/**@name Deprecated functions (only for use within Condor) */
		//@{

		/** A constructor that reads old ClassAds from a FILE */
	CompatClassAd(FILE*,char*,int&,int&,int&);	// Constructor, read from file.

		/* This is a pass-through to ClassAd::Insert(). Because we define
		 * our own Insert() below, our parent's Insert() won't be found
		 * by users of this class.
		 */
	bool Insert( const std::string &attrName, classad::ExprTree *expr );

		/** Insert an attribute/value into the ClassAd 
		 *  @param str A string of the form "Attribute = Value"
		 */
	int Insert(const char *str);

		/** Insert an attribute/value into the ClassAd 
		 *  @param expr A string of the form "Attribute = Value"
		 */
	int InsertOrUpdate(const char *expr) { return Insert(expr); }

		// for iteration through expressions
//		void		ResetExpr();
//		classad::ExprTree*	NextExpr();

		// for iteration through names (i.e., lhs of the expressions)
//		void		ResetName() { this->ptrName = exprList; }
//		const char* NextNameOriginal();

		// lookup values in classads  (for simple assignments)
//		classad::ExprTree*   Lookup(char *) const;  		// for convenience
//      classad::ExprTree*	Lookup(const char*) const;	// look up an expression

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
	const char*	GetMyTypeName();
		/** Set the value of the TargetType attribute */
	void 		SetTargetTypeName(const char *);
		/** Get the value of the TargetType attribtute */
	const char*	GetTargetTypeName();

		/** Print the ClassAd as an old ClassAd to the stream
		 * @param s the stream
		 */
	int put(Stream& s);

		/** Read the old ClassAd from the stream, and fill in this ClassAd.
		 * @param s the stream
		 */
	int initFromStream(Stream& s);

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

	bool AddExplicitConditionals( classad::ExprTree *expr, classad::ExprTree *&newExpr );
	classad::ClassAd *AddExplicitTargetRefs( );
		//@}

	static bool ClassAdAttributeIsPrivate( char const *name );

 private:
	void evalFromEnvironment( const char *name, classad::Value val );
	classad::ExprTree *AddExplicitConditionals( classad::ExprTree * );
	classad::ExprTree *AddExplicitTargetRefs( classad::ExprTree *,
						std::set < std::string, classad::CaseIgnLTStr > & );
};

#endif
