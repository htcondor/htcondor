/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __SINK_H__
#define __SINK_H__

#include "common.h"
#include "exprTree.h"
#include <vector>
#include <utility>	// for pair template
#include <string>

BEGIN_NAMESPACE( classad )

/// The unparser object
class ClassAdUnParser
{
	public:
		/// Constructor
		ClassAdUnParser( );

		/// Destructor
		virtual ~ClassAdUnParser( );

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

			//	for backcompatibility only - NAC
		void SetOldClassAd( bool );
		bool GetOldClassAd();

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

		// table of string representation of operators
		static char *opString[];

 protected:
		bool oldClassAd;
};


/// The pretty print object --- unparsing with format
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

END_NAMESPACE // classad

#endif//__DUMPER_H__
