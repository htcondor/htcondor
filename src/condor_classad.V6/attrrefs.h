#ifndef __ATTRREFS_H__
#define __ATTRREFS_H__

#include "stringSpace.h"

/** Represents a attribute reference node in the expression tree
*/
class AttributeReference : public ExprTree 
{
  	public:
		/** Constructor
		*/
    	AttributeReference ();

		/**  Destructor
		*/
    	~AttributeReference ();

        /** Sends the attribute reference object to a Sink.
            @param s The Sink object.
            @return false if the expression could not be successfully
                unparsed into the sink.
            @see Sink
        */
		virtual bool toSink (Sink &s);

		/** Set the attribute reference.
			@param expr The expression part of the reference (i.e., in
				case of expr.attr).  This parameter is NULL if the reference
				is absolute (i.e., .attr) or simple (i.e., attr).
			@param attrName The name of the reference.  This string is
				duplicated internally.
			@param absolute True if the reference is an absolute reference
				(i.e., in case of .attr).  This parameter cannot be true if
				expr is not NULL
		*/
    	void setReference (ExprTree *expr, char *attrName, bool absolute=false);

  	private:
		// private ctor for internal use
		AttributeReference( ExprTree*, char*, bool );

		virtual ExprTree* _copy( CopyMode );
		virtual void _setParentScope( ClassAd* p );
    	virtual bool _evaluate( EvalState & , Value & );
    	virtual bool _evaluate( EvalState & , Value &, ExprTree*& );
    	virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );

		int	findExpr( EvalState&, ExprTree*&, ExprTree*&, bool );

		ExprTree	*expr;
		bool		absolute;
    	char        *attributeStr;
    	SSString    attributeName;
};

#endif//__ATTRREFS_H__
