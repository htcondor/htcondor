#ifndef __ATTRREFS_H__
#define __ATTRREFS_H__

#include "stringSpace.h"

class AttributeReference : public ExprTree 
{
  	public:
		// ctor/dtor	
    	AttributeReference ();
    	~AttributeReference ();

    	// override methods
		virtual bool toSink (Sink &);
		virtual void setParentScope( ClassAd* );

    	// specific methods
    	void setReference (ExprTree *, char *, bool = false);

  	private:
		// private ctor for internal use
		AttributeReference( ExprTree*, char*, bool );

		virtual ExprTree* _copy( CopyMode );
    	virtual void _evaluate( EvalState & , Value & );
    	virtual void _evaluate( EvalState & , Value &, ExprTree*& );
    	virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );

		int	findExpr( EvalState&, ExprTree*&, ExprTree*&, bool );

		ExprTree	*expr;
		bool		absolute;
    	char        *attributeStr;
    	SSString    attributeName;
};

#endif//__ATTRREFS_H__
