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

		/** Create an absolute time literal.
		 * 	@param now The time in UNIX epoch.  If a value of -1 is passed in
		 * 	the system's current time will be used.
		 * 	@return The literal expression.
		 */
		static Literal* MakeAbsTime( time_t now=-1 );

		/** Create a relative time literal.
		 * @param secs The number of seconds.  If a value of -1 is passed in
		 * the time since midnight (i.e., the daytime) is used.
		 * @return The literal expression.
		 */
		static Literal* MakeRelTime( time_t secs=-1 );

		/** Create a relative time interval by subtracting two absolute times.
		 * @param t1 The end time of the interval.  If -1 is passed in, the
		 * system's current time will be used.
		 * @param t2 the start time of the interval  If -1 is passed in, the
		 * system's current time will be used.
		 * @return The literal expression of the relative time (t1 - t2).
		 */
		static Literal* MakeRelTime( time_t t1=-1, time_t t2=-1 );

		virtual Literal* Copy( ) const;

		static Literal*MakeLiteral( const Value&, NumberFactor f=NO_FACTOR );
		void GetComponents( Value&, NumberFactor &f ) const;

  	private:
		friend FunctionCall;
		friend ClassAd;
		friend ExprList;
		friend Operation;

		virtual void _SetParentScope( const ClassAd* ){ }
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, OpKind* ) const;
 		virtual bool _Evaluate (EvalState &, Value &) const;
 		virtual bool _Evaluate (EvalState &, Value &, ExprTree *&) const;

		// literal specific information
    	Value   		value;
		NumberFactor	factor;
};

END_NAMESPACE // classad

#endif//__LITERALS_H__
