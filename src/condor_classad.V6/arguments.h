#ifndef __ARGUMENTS_H__
#define __ARGUMENTS_H__

#include "exprTree.h"
#include "exprList.h"

// leverage exprList code
class ArgumentList : public ExprList
{
	public:
		// ctor/dtor
		ArgumentList ();
		~ArgumentList ();

		// override methods
		virtual ExprTree *copy (void);
		virtual bool toSink (Sink &);

		// specific methods
		void appendArgument (ExprTree *);
		int	 getNumberOfArguments (void) { return numberOfArguments; }
		void evalArgument(EvalState &, int, Value &);

	private:
		// dummy --- never gets called
		virtual void _evaluate (EvalState &, Value &) { }

		int		numberOfArguments;
};

#endif/ __ARGUMENTS_H__
