/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
