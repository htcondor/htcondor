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

#ifndef __FN_CALL_H__
#define __FN_CALL_H__

#include <map>
#include <vector>
#include "classad.h"

BEGIN_NAMESPACE( classad )

typedef vector<ExprTree*> ArgumentList;

/// Node of the expression which represents a call to an inbuilt function
class FunctionCall : public ExprTree
{
	public:
		/// Constructor
		FunctionCall ();

		/// Destructor
		~FunctionCall ();

		static FunctionCall *MakeFunctionCall( const string &fnName, 
					vector<ExprTree*> &argList );
		void GetComponents( string &fnName, vector<ExprTree*> &argList ) const;

		virtual FunctionCall* Copy( ) const;

		typedef	bool(*ClassAdFunc)(const char*, const ArgumentList&, EvalState&,
					Value&);
		typedef map<string, ClassAdFunc, CaseIgnLTStr > FuncTable;

	private:
		virtual void _SetParentScope( const ClassAd* );
		virtual bool _Evaluate( EvalState &, Value & ) const;
		virtual bool _Evaluate( EvalState &, Value &, ExprTree *& ) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, OpKind* ) const;

		// information common to all function calls
		// a mapping from function names (char*) to static methods
		static FuncTable functionTable;
		static bool		 initialized;

		// function call specific information
		string			functionName;
		ClassAdFunc		function;
		ArgumentList	arguments;

		// function implementations
			 // type predicates
		static bool isType(const char*,const ArgumentList&,EvalState&,Value&);

			// list membership predicates
		static bool testMember(const char*,const ArgumentList&,EvalState&,
					Value&);

			// sum, average and bounds (max and min)
		static bool sumAvgFrom(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool boundFrom(const char*,const ArgumentList&,EvalState&,
					Value&);

			// time management (constructors)
		static bool currentTime(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool timeZoneOffset(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool dayTime(const char*,const ArgumentList&,EvalState&,Value&);
		static bool makeTime(const char*,const ArgumentList&,EvalState&,Value&);
		static bool makeDate(const char*,const ArgumentList&,EvalState&,Value&);
			// time management (selectors)
		static bool getField(const char*,const ArgumentList&,EvalState&,Value&);
			// time management (conversions)
		static bool inTimeUnits(const char*,const ArgumentList&,EvalState&,
					Value&);

			// string management
		static bool strCat(const char*,const ArgumentList&,EvalState&,Value&);
		static bool changeCase(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool subString(const char*,const ArgumentList&,EvalState&,
					Value&);

			// pattern matching
		static bool matchPattern(const char*,const ArgumentList&,EvalState&,
					Value&);

			// type conversion
		static bool convInt(const char*,const ArgumentList&,EvalState&,Value&);
		static bool convReal(const char*,const ArgumentList&,EvalState&,Value&);
		static bool convString(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool convBool(const char*,const ArgumentList&,EvalState&,Value&);
		static bool convTime(const char*,const ArgumentList&,EvalState&,Value&);

			// math (floor, ceil, round)
		static bool doMath(const char*,const ArgumentList&,EvalState&,Value&);
};

END_NAMESPACE // classad

#endif//__FN_CALL_H__
