#ifndef __FN_CALL_H__
#define __FN_CALL_H__

#include "HashTable.h"

typedef ExtArray<ExprTree*> ArgumentList;

class FunctionCall : public ExprTree
{
	public:
		// ctor/dtor
		FunctionCall ();
		~FunctionCall ();

		// override methods
		virtual bool toSink (Sink &);
		virtual void setParentScope( ClassAd* );

		// specific methods
		void setFunctionName (char *fnName);
		void appendArgument  (ExprTree *arg);

		typedef	void (*ClassAdFunc)(char*,ArgumentList&,EvalState&,Value&);
		typedef HashTable<MyString, ClassAdFunc> FuncTable;

	private:
		virtual ExprTree* _copy( CopyMode );
		virtual void _evaluate( EvalState &, Value & );
		virtual void _evaluate( EvalState &, Value &, ExprTree *& );
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
		static void isUndefined(char*,ArgumentList&,EvalState&,Value&);
		static void isError(char*,ArgumentList&,EvalState&,Value&);
		static void isInteger(char*,ArgumentList&,EvalState&,Value&);
		static void isReal(char*,ArgumentList&,EvalState&,Value&);
		static void isString(char*,ArgumentList&,EvalState&,Value&);
		static void isList(char*,ArgumentList&,EvalState&,Value&);
		static void isClassAd(char*,ArgumentList&,EvalState&,Value&);
		static void isMember(char*,ArgumentList&,EvalState&,Value&);
		static void isExactMember(char*,ArgumentList&,EvalState&,Value&);
		static void sumFrom(char*,ArgumentList&,EvalState&,Value&);
		static void avgFrom(char*,ArgumentList&,EvalState&,Value&);
		static void boundFrom(char*,ArgumentList&,EvalState&,Value&);
};


#endif//__FN_CALL_H__
