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

#include "HashTable.h"

BEGIN_NAMESPACE( classad )

typedef ExtArray<ExprTree*> ArgumentList;

/// Node of the expression which represents a call to an inbuilt function
class FunctionCall : public ExprTree
{
	public:
		/// Constructor
		FunctionCall ();

		/// Destructor
		~FunctionCall ();

        /** Sends the function call object to a Sink.
            @param s The Sink object.
            @return false if the expression could not be successfully
                unparsed into the sink.
            @see Sink 
        */
		virtual bool ToSink( Sink & );

		/** Set the name of the function call
			@param fnName The name of the function.  The string is duplicated
				internally.
		*/
		void SetFunctionName(char *fnName);

		/** Append an actual argument to the call.
			@param arg The argument to the call.  The expression is adopted
				by the node (i.e., does not duplicate the structure, but 
				assumes responsibility for further storage management of the
				expression).
		*/
		void AppendArgument(ExprTree *arg);

		virtual FunctionCall* Copy( );

		typedef	bool (*ClassAdFunc)(char*,ArgumentList&,EvalState&,Value&);
		typedef HashTable<MyString, ClassAdFunc> FuncTable;

	private:
		virtual void _SetParentScope( ClassAd* );
		virtual bool _Evaluate( EvalState &, Value & );
		virtual bool _Evaluate( EvalState &, Value &, ExprTree *& );
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, OpKind* );

		// information common to all function calls
		// a mapping from function names (char*) to static methods
		static FuncTable functionTable;
		static bool		 initialized;

		// function call specific information
		char 	 		*functionName;
		ClassAdFunc		function;
		ArgumentList	arguments;
		int				numArgs;

		// function implementations
			 // type predicates
		static bool isType(char*,ArgumentList&,EvalState&,Value&);

			// list membership predicates
		static bool testMember(char*,ArgumentList&,EvalState&,Value&);

			// sum, average and bounds (max and min)
		static bool sumAvgFrom(char*,ArgumentList&,EvalState&,Value&);
		static bool boundFrom(char*,ArgumentList&,EvalState&,Value&);

			// time management (constructors)
		static bool currentTime(char*,ArgumentList&,EvalState&,Value&);
		static bool timeZoneOffset(char*,ArgumentList&,EvalState&,Value&);
		static bool dayTime(char*,ArgumentList&,EvalState&,Value&);
		static bool makeTime(char*,ArgumentList&,EvalState&,Value&);
		static bool makeDate(char*,ArgumentList&,EvalState&,Value&);
			// time management (selectors)
		static bool getField(char*,ArgumentList&,EvalState&,Value&);
			// time management (conversions)
		static bool inTimeUnits(char*,ArgumentList&,EvalState&,Value&);

			// string management
		static bool strCat(char*,ArgumentList&,EvalState&,Value&);
		static bool changeCase(char*,ArgumentList&,EvalState&,Value&);
		static bool subString(char*,ArgumentList&,EvalState&,Value&);

			// pattern matching
		static bool matchPattern(char*,ArgumentList&,EvalState&,Value&);

			// type conversion
		static bool convInt(char*,ArgumentList&,EvalState&,Value&);
		static bool convReal(char*,ArgumentList&,EvalState&,Value&);
		static bool convString(char*,ArgumentList&,EvalState&,Value&);
		static bool convBool(char*,ArgumentList&,EvalState&,Value&);
		static bool convTime(char*,ArgumentList&,EvalState&,Value&);

			// math (floor, ceil, round)
		static bool doMath(char*,ArgumentList&,EvalState&,Value&);
};

END_NAMESPACE // classad

#endif//__FN_CALL_H__
