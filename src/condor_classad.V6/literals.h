#ifndef __LITERALS_H__
#define __LITERALS_H__

class Literal : public ExprTree 
{
  	public:
		// ctor/dtor
    	Literal ();
    	~Literal ();

    	// override methods
    	virtual ExprTree *copy (CopyMode = EXPR_DEEP_COPY);
		virtual bool toSink (Sink &);

    	// specific methods
    	void setIntegerValue    (int, NumberFactor = NO_FACTOR);
    	void setRealValue       (double, NumberFactor = NO_FACTOR);
    	void adoptStringValue   (char *);
    	void setStringValue     (char *);
    	void setUndefinedValue  (void);
    	void setErrorValue      (void);

	protected:
		friend FunctionCall;
		friend ClassAd;
		friend ExprList;
		friend Operation;

		static Literal* makeLiteral( EvalValue& );
		virtual void setParentScope( ClassAd* ) {};

  	private:
		virtual bool _flatten( EvalState&, EvalValue&, ExprTree*&, OpKind* );
 		virtual void _evaluate (EvalState &, EvalValue &);

		// literal specific information
    	Value   		value;
		NumberFactor	factor;
};

#endif//__LITERALS_H__
