#ifndef __FN_CALL_H__
#define __FN_CALL_H__

#include "HashTable.h"

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
		virtual bool toSink (Sink &);

		/** Set the name of the function call
			@param fnName The name of the function.  The string is duplicated
				internally.
		*/
		void setFunctionName(char *fnName);

		/** Append an actual argument to the call.
			@param arg The argument to the call.  The expression is adopted
				by the node (i.e., does not duplicate the structure, but 
				assumes responsibility for further storage management of the
				expression).
		*/
		void appendArgument(ExprTree *arg);

		typedef	bool (*ClassAdFunc)(char*,ArgumentList&,EvalState&,Value&);
		typedef HashTable<MyString, ClassAdFunc> FuncTable;

	private:
		virtual ExprTree* _copy( CopyMode );
		virtual void _setParentScope( ClassAd* );
		virtual bool _evaluate( EvalState &, Value & );
		virtual bool _evaluate( EvalState &, Value &, ExprTree *& );
		virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );

		// information common to all function calls
		// a mapping from function names (char*) to static methods
		static FuncTable functionTable;
		static bool		 initialized;

		// function call specific information
		char 	 		*functionName;
		ClassAdFunc		function;
		ArgumentList	arguments;
		int				numArgs;

		// the functions themselves 
		static bool isUndefined(char*,ArgumentList&,EvalState&,Value&);
		static bool isError(char*,ArgumentList&,EvalState&,Value&);
		static bool isInteger(char*,ArgumentList&,EvalState&,Value&);
		static bool isReal(char*,ArgumentList&,EvalState&,Value&);
		static bool isString(char*,ArgumentList&,EvalState&,Value&);
		static bool isList(char*,ArgumentList&,EvalState&,Value&);
		static bool isClassAd(char*,ArgumentList&,EvalState&,Value&);
		static bool isMember(char*,ArgumentList&,EvalState&,Value&);
		static bool isExactMember(char*,ArgumentList&,EvalState&,Value&);
		static bool sumFrom(char*,ArgumentList&,EvalState&,Value&);
		static bool avgFrom(char*,ArgumentList&,EvalState&,Value&);
		static bool boundFrom(char*,ArgumentList&,EvalState&,Value&);
};


#endif//__FN_CALL_H__
