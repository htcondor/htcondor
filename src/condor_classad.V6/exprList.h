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
		virtual bool toSink (Sink &);
		virtual void setParentScope( ClassAd* );

		// specific methods
		void appendExpression(ExprTree *);
		int  number();
		void rewind();
		ExprTree* next();

	protected:
		void clear (void);
		List<ExprTree> exprList;

	private:
		virtual ExprTree* _copy( CopyMode );
		virtual void _evaluate (EvalState &, Value &);
		virtual void _evaluate (EvalState &, Value &, ExprTree *&);
		virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );
};


#endif//__EXPR_LIST_H__
