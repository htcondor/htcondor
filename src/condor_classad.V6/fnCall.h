#ifndef __FN_CALL_H__
#define __FN_CALL_H__

#include "arguments.h"

class FunctionCall : public ExprTree
{
	public:
		// ctor/dtor
		FunctionCall ();
		~FunctionCall ();

		// override methods
		virtual ExprTree *copy (void);
		virtual bool toSink (Sink &);

		// specific methods; fnName is *not* strdup()d internally
		void setCall (char *fnName, ArgumentList *argList);

	private:
		virtual void _evaluate (EvalState &, Value &);

		// function call specific information
		char 	 *functionName;
		ExprTree *argumentList;
};


#endif//__FN_CALL_H__
