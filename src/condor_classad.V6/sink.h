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
#include <vector>
#include <utility>	// for pair template
#include <string>
#include "formatOptions.h"

BEGIN_NAMESPACE( classad )

class ClassAdCollection;
class View;

class ClassAdUnParser
{
	public:
		/// Constructor
		ClassAdUnParser( );

		/// Destructor
		virtual ~ClassAdUnParser( );

		void Unparse( string &buffer, Value &val );
		void Unparse( string &buffer, ExprTree *expr );

		virtual void Unparse( string &buffer, Value&, NumberFactor );	
		virtual void Unparse( string &buffer, ExprTree *tree, const string &ref,
					bool absolute=false );
		virtual void Unparse( string &buffer, OpKind op, ExprTree *op1, 
					ExprTree *op2, ExprTree *op3 );
		virtual void Unparse(string &buffer, const string &fnName, 
					vector<ExprTree*>& args);
		virtual void Unparse( string &buffer, vector< pair<string, 
					ExprTree*> >& attrlist );
		virtual void Unparse( string &buffer, vector<ExprTree*>& exprs );

		// table of string representation of operators
		static char *opString[];

		/** Set the format options for the sink
		 	@param fo Pointer to a FormatOptions object
			@see FormatOptions
		*/
		void SetFormatOptions( FormatOptions *fo ) { pp = fo; }

		/** Get the FormatOptions for this sink
			@return Pointer to the current FormatOptions object.  This pointer
				should not be deleted.
		*/
		FormatOptions *GetFormatOptions( ) { return pp; }


    private:
		friend class ClassAdList;
		friend class ClassAd;
		friend class ExprTree;
		friend class Value;
		friend class Literal;
		friend class AttributeReference;
		friend class Operation;	
		friend class FunctionCall;
		friend class ExprList;
		friend class FormatOptions;
		friend class ClassAdCollection;
		friend class View;

		FormatOptions *pp;
};

END_NAMESPACE // classad

#endif//__DUMPER_H__
