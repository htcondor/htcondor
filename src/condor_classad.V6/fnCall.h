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

#ifndef __FN_CALL_H__
#define __FN_CALL_H__

#include <map>
#include <vector>
#include "classad.h"

BEGIN_NAMESPACE( classad );

typedef vector<ExprTree*> ArgumentList;

/// Node of the expression which represents a call to an inbuilt function
class FunctionCall : public ExprTree
{
	public:
		/// Destructor
		~FunctionCall ();

		/** Factory method to make a function call expression
		 * 	@param fnName	The name of the function to be called
		 * 	@param argList	A vector representing the argument list
		 * 	@return The constructed function call expression
		 */
		static FunctionCall *MakeFunctionCall( const string &fnName, 
					vector<ExprTree*> &argList );

		/** Deconstructor to get the components of a function call
		 * 	@param fnName	The name of the function being called
		 * 	@param argList  The argument list
		 */
		void GetComponents( string &, vector<ExprTree*> &) const;

		/// Make a deep copy of the expression
		virtual FunctionCall* Copy( ) const;

	protected:
		/// Constructor
		FunctionCall ();

		typedef	bool(*ClassAdFunc)(const char*, const ArgumentList&, EvalState&,
					Value&);
		typedef map<string, void*, CaseIgnLTStr > FuncTable;

	private:
		virtual void _SetParentScope( const ClassAd* );
		virtual bool _Evaluate( EvalState &, Value & ) const;
		virtual bool _Evaluate( EvalState &, Value &, ExprTree *& ) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;

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
		/*
		static bool sumAvgFrom(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool boundFrom(const char*,const ArgumentList&,EvalState&,
					Value&);
		*/
		static bool size(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool sumAvg(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool minMax(const char*,const ArgumentList&,EvalState&,
					Value&);
		static bool listCompare(const char*,const ArgumentList&,EvalState&,
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
