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

#ifndef __DUMPER_H__
#define __DUMPER_H__

#include "common.h"
#include "exprTree.h"
#include <vector>
#include <utility>	// for pair template
#include <string>

BEGIN_NAMESPACE( classad );

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
		void Unparse( string &buffer, Value &val );

		/** Unparse an expression 
		 * 	@param buffer The string to unparse to
		 * 	@param val The expression to unparse
		 */
		void Unparse( string &buffer, ExprTree *expr );

		virtual void UnparseAux( string &buffer,Value&,Value::NumberFactor );	
		virtual void UnparseAux( string &buffer, ExprTree *tree, 
					string &ref, bool absolute=false );
		virtual void UnparseAux( string &buffer, Operation::OpKind op, 
					ExprTree *op1, ExprTree *op2, ExprTree *op3 );
		virtual void UnparseAux(string &buffer, string &fnName, 
					vector<ExprTree*>& args);
		virtual void UnparseAux( string &buffer, 
					vector< pair< string, ExprTree*> >& attrlist );
		virtual void UnparseAux( string &buffer, vector<ExprTree*>& );

		// table of string representation of operators
		static char *opString[];
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

        virtual void UnparseAux( string &buffer, Operation::OpKind op, 
					ExprTree *op1, ExprTree *op2, ExprTree *op3 );
        virtual void UnparseAux( string &buffer,
                    vector< pair< string, ExprTree*> >& attrlist );
        virtual void UnparseAux( string &buffer, vector<ExprTree*>& );

    private:
        int  classadIndent;
        int  listIndent;
        bool wantStringQuotes;
        bool minimalParens;
        int  indentLevel;
};

END_NAMESPACE // classad

#endif//__DUMPER_H__
