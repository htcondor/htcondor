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
    	virtual ExprTree *copy (void);
		virtual bool toSink (Sink &);

    	// specific methods; the strings are *not* strdup()d internally
    	void setReference (char *, char *);

  	private:
    	virtual void _evaluate (EvalState &, Value &);

		// attr reference specific information
    	char        *attributeStr;
    	SSString    attributeName;
		char		*scopeStr;
		SSString	scopeName;		// which string space??  --RR
};

#endif//__ATTRREFS_H__
