#ifndef __LITERALS_H__
#define __LITERALS_H__

/** Represents the literals of the ClassAd language, such as integers,
		reals, booleans, strings, undefined and real.
*/
class Literal : public ExprTree 
{
  	public:
		/// Constructor
    	Literal ();
		/// Destructor
    	~Literal ();

        /** Sends the literal expression object to a Sink.
            @param s The Sink object.
            @return false if the expression could not be successfully
                unparsed into the sink.
            @see Sink
        */
		virtual bool toSink (Sink &s);

		/** Set this literal as a boolean.
			@param b The boolean value.
		*/
		void setBooleanValue(bool b);
		/** Set this literal as an integer.
			@param i The integer value.
			@param f The multiplication factor.
		*/
    	void setIntegerValue(int i, NumberFactor f = NO_FACTOR);

		/** Set this literal as a real.
			@param r The real value.
			@param f The multiplication factor.
		*/
    	void setRealValue(double r, NumberFactor f = NO_FACTOR);

		// leave this undocumented for now
    	void adoptStringValue(char *str);

		/** Set the literal to a string value
			@param str The string value, which is duplicated internally.
		*/
    	void setStringValue(char *str);

		/** Set the literal as an undefined value
		*/
    	void setUndefinedValue(void);

		/** Set the literal as an error value
		*/
    	void setErrorValue(void);

	protected:
		friend FunctionCall;
		friend ClassAd;
		friend ExprList;
		friend Operation;

		static Literal* makeLiteral( Value& );
  	private:
		virtual ExprTree* _copy( CopyMode );
		virtual void _setParentScope( ClassAd* ){ }
		virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );
 		virtual bool _evaluate (EvalState &, Value &);
 		virtual bool _evaluate (EvalState &, Value &, ExprTree *&);

		// literal specific information
		char			*string;
    	Value   		value;
		NumberFactor	factor;
};

#endif//__LITERALS_H__
