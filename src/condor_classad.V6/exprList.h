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
		virtual ExprTree *copy (CopyMode);
		virtual bool toSink (Sink &);

		// specific methods
		void appendExpression(ExprTree *);
		int  getLength();

	protected:
		virtual void setParentScope( ClassAd* );

		void clear (void);
		List<ExprTree> exprList;

	private:
		virtual void _evaluate (EvalState &, EvalValue &);
		virtual bool _flatten( EvalState&, EvalValue&, ExprTree*&, OpKind* );
};


#endif//__EXPR_LIST_H__

