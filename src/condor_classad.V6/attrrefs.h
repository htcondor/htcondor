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
    	virtual ExprTree *copy (CopyMode);
		virtual bool toSink (Sink &);

    	// specific methods
    	void setReference (ExprTree *, char *, bool = false);

	protected:
		virtual void setParentScope( ClassAd* );

  	private:
    	virtual void _evaluate( EvalState & , EvalValue & );
		int	findExpr( EvalState , ExprTree* , ExprTree*& , EvalState& );

		ExprTree	*expr;
		bool		absolute;
    	char        *attributeStr;
    	SSString    attributeName;
};

#endif//__ATTRREFS_H__
