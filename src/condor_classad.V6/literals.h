#ifndef __LITERALS_H__
#define __LITERALS_H__

class Literal : public ExprTree 
{
  	public:
		// ctor/dtor
    	Literal ();
    	~Literal ();

    	// override methods
		virtual void setParentScope( ClassAd* ){ }
		virtual bool toSink (Sink &);

    	// specific methods
		void setBooleanValue	(bool);
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

		static Literal* makeLiteral( Value& );
  	private:
		virtual ExprTree* _copy( CopyMode );
		virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );
 		virtual void _evaluate (EvalState &, Value &);
 		virtual void _evaluate (EvalState &, Value &, ExprTree *&);

		// literal specific information
    	Value   		value;
		NumberFactor	factor;
};

#endif//__LITERALS_H__
