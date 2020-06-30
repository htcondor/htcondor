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


#ifndef __CLASSAD_SINK_H__
#define __CLASSAD_SINK_H__

#include "classad/common.h"
#include "classad/exprTree.h"
#include <vector>
#include <utility>	// for pair template
#include <string>

namespace classad {

/// This converts a ClassAd into a string representing the %ClassAd
class ClassAdUnParser
{
	public:
		/// Constructor
		ClassAdUnParser() : oldClassAd(false), xmlUnparse(false), delimiter('\"'), oldClassAdValue(false) {}

		/// Destructor
		virtual ~ClassAdUnParser() {}

		/** Function to be called by the ClassAdXMLUnParser with a true
		 *     flag before doing an XMLUnparse
		 */
		void setXMLUnparse(bool doXMLUnparse);

		// The default delimiter for strings is '\"'
		// This can be changed to '\'' to unparse quoted attributes, with this function
		void setDelimiter(char delim);

		/** Unparse a value 
		 * 	@param buffer The string to unparse to
		 * 	@param val The value to unparse
		 */
		void Unparse( std::string &buffer, const Value &val );

		/** Unparse an expression 
		 * 	@param buffer The string to unparse to
		 * 	@param expr The expression to unparse
		 */
		void Unparse( std::string &buffer, const ExprTree *expr );
		void Unparse( std::string &buffer, const ClassAd *ad, const References &whitelist );

			//	for backcompatibility only - NAC
			// In old ClassAd syntax, nested ads should be delimited in
			// the new syntax (ad enclosed by square brackets and
			// attributes separated by semicolons), but the outer-most ad
			// is delimited in the old syntax (no brackets and newlines
			// instead of semicolons).
			// If you want to unparse an attribute value (and not a
			// standalone ad) in the old style, use the second form of
			// SetOldClassAd() and pass true for attr_value. That will
			// cause the outermost ClassAd to be delimited in the new
			// syntax (as well as any nested ads).
		void SetOldClassAd( bool old_syntax );
		void SetOldClassAd( bool old_syntax, bool attr_value );
		bool GetOldClassAd() const;

		virtual void UnparseAux( std::string &buffer,
								 const Value&,Value::NumberFactor );
		virtual void UnparseAux( std::string &buffer, 
								 const ExprTree *tree, 
								 std::string &ref, bool absolute=false );
		virtual void UnparseAux( std::string &buffer, Operation::OpKind op, 
					ExprTree *op1, ExprTree *op2, ExprTree *op3 );
		virtual void UnparseAux(std::string &buffer, std::string &fnName, 
					std::vector<ExprTree*>& args);
		virtual void UnparseAux( std::string &buffer, 
					std::vector< std::pair< std::string, ExprTree*> >& attrlist );
		virtual void UnparseAux( std::string &buffer, std::vector<ExprTree*>& );

		// to unparse attribute names (quoted & unquoted attributes)
		virtual void UnparseAux( std::string &buffer, const std::string &identifier);

		// table of string representation of operators
		static const char *opString[];

 protected:
		bool oldClassAd;
		bool xmlUnparse;
		char delimiter; // string delimiter - initialized to '\"' in the constructor
		bool oldClassAdValue;
};


/// This is a special case of the ClassAdParser which prints the ClassAds more nicely.
class PrettyPrint : public ClassAdUnParser
{
    public:
		/// Constructor
        PrettyPrint( );
		///Destructor
        virtual ~PrettyPrint( );

		/// Set the indentation width for displaying classads
        void SetClassAdIndentation( int=4 );
		/// Get the indentation width for displaying classads
        int  GetClassAdIndentation( );

		/// Set the indentation width for displaying lists
        void SetListIndentation( int=4 );
		/// Get the indentation width for displaying lists
        int  GetListIndentation( );

        void SetWantStringQuotes( bool );
        bool GetWantStringQuotes( );

		/// Set minimal parentheses mode
        void SetMinimalParentheses( bool );
		/// Get minimal parentheses mode
        bool GetMinimalParentheses( );

        virtual void UnparseAux( std::string &buffer, Operation::OpKind op, 
					ExprTree *op1, ExprTree *op2, ExprTree *op3 );
        virtual void UnparseAux( std::string &buffer,
                    std::vector< std::pair< std::string, ExprTree*> >& attrlist );
        virtual void UnparseAux( std::string &buffer, std::vector<ExprTree*>& );

    private:
        int  classadIndent;
        int  listIndent;
        bool wantStringQuotes;
        bool minimalParens;
        int  indentLevel;
};

} // classad

#endif//__CLASSAD_SINK_H__
