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

BEGIN_NAMESPACE( classad )

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
		virtual bool ToSink( Sink &s );

		/** Set this literal as a boolean.
			@param b The boolean value.
		*/
		void SetBooleanValue(bool b);

		/** Set this literal as an integer.
			@param i The integer value.
			@param f The multiplication factor.
		*/
    	void SetIntegerValue(int i, NumberFactor f = NO_FACTOR);

		/** Set this literal as a real.
			@param r The real value.
			@param f The multiplication factor.
		*/
    	void SetRealValue(double r, NumberFactor f = NO_FACTOR);

		/** Set the literal to a string value.  The string is adopted by the
		  		object, and is assumed to have been created with new char[].
			@param str The string value
		*/
    	void SetStringValue( const char *str );

		/** Set the literal to an absolute time value
			@param secs Number of seconds since the UNIX epoch
		*/
		void SetAbsTimeValue( int secs );

		/** Set the literal to a relative time value
			@param secs Number of seconds (could be positive or negative)
		*/
		void SetRelTimeValue( int secs );

		/** Set the literal as an undefined value
		*/
    	void SetUndefinedValue(void);

		/** Set the literal as an error value
		*/
    	void SetErrorValue(void);

		/** Create an absolute time literal.
		 * 	@param now The time in UNIX epoch.  If a value of -1 is passed in
		 * 	the system's current time will be used.
		 * 	@return The literal expression.
		 */
		static Literal* MakeAbsTime( time_t now=-1 );

		/** Create a relative time literal.
		 * @param secs The number of seconds.
		 * @return The literal expression.
		 */
		static Literal* MakeRelTime( int secs );

		/** Create a relative time interval by subtracting two absolute times.
		 * @param t1 The end time of the interval.  If -1 is passed in, the
		 * system's current time will be used.
		 * @param t2 the start time of the interval  If -1 is passed in, the
		 * system's current time will be used.
		 * @return The literal expression of the relative time (t1 - t2).
		 */
		static Literal* MakeRelTime( time_t t1=-1, time_t t2=-1 );

		virtual Literal* Copy( );

  	private:
		friend FunctionCall;
		friend ClassAd;
		friend ExprList;
		friend Operation;

		static Literal*MakeLiteral(Value&);
		virtual void _SetParentScope( ClassAd* ){ }
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, OpKind* );
 		virtual bool _Evaluate (EvalState &, Value &);
 		virtual bool _Evaluate (EvalState &, Value &, ExprTree *&);

		// literal specific information
    	Value   		value;
		NumberFactor	factor;
};

END_NAMESPACE // classad

#endif//__LITERALS_H__
