#ifndef __EXPR_LIST_H__
#define __EXPR_LIST_H__

#include "list.h"

class ExprList : public ExprTree
{
	public:
		// ctor/dtor
		ExprList();
		~ExprList();

		// override methods
		virtual ExprTree *copy (void);
		virtual bool toSink (Sink &);

		// specific methods
		void appendExpression(ExprTree *);
		bool isMember (const Value &);

	protected:
		void clear (void);
		List<ExprTree> exprList;

	private:
		virtual void _evaluate (EvalState &, Value &);
};


#endif//__EXPR_LIST_H__

