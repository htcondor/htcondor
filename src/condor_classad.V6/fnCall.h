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
		virtual ExprTree *copy (CopyMode);
		virtual bool toSink (Sink &);

		// specific methods
		void setFunctionName (char *fnName);
		void appendArgument  (ExprTree *arg);

		typedef	void (*ClassAdFunc)(char*,ArgumentList&,EvalState&,EvalValue&);
		typedef HashTable<MyString, ClassAdFunc> FuncTable;

	protected:
		virtual void setParentScope( ClassAd* );

	private:
		virtual void _evaluate (EvalState &, EvalValue &);

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
		static void isUndefined(char*,ArgumentList&,EvalState&,EvalValue&);
		static void isError(char*,ArgumentList&,EvalState&,EvalValue&);
		static void isInteger(char*,ArgumentList&,EvalState&,EvalValue&);
		static void isReal(char*,ArgumentList&,EvalState&,EvalValue&);
		static void isString(char*,ArgumentList&,EvalState&,EvalValue&);
		static void isList(char*,ArgumentList&,EvalState&,EvalValue&);
		static void isClassAd(char*,ArgumentList&,EvalState&,EvalValue&);
		static void isMember(char*,ArgumentList&,EvalState&,EvalValue&);
		static void isExactMember(char*,ArgumentList&,EvalState&,EvalValue&);
		
};


#endif//__FN_CALL_H__
