/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef __CLASSAD_LITERALS_H__
#define __CLASSAD_LITERALS_H__

#include <vector>
#include "exprTree.h"

namespace classad {

typedef std::vector<ExprTree*> ArgumentList;

/** Represents the literals of the ClassAd language, such as integers,
		reals, booleans, strings, undefined and real.
*/
class Literal : public ExprTree 
{
	public:
		/// Destructor
		virtual ~Literal () {};

		/// Copy constructor
		Literal(const Literal &lit)
			: ExprTree()
			, value(lit.value)
			{}

		/// Assignment operator
		Literal &operator=(const Literal &lit) {
			if (this != &lit) {
				CopyFrom(lit);
			}
			return *this;
		}

		/// node type
		virtual NodeKind GetKind (void) const { return LITERAL_NODE; }

		/** Create an absolute time literal.
		 * 	@param now The time in UNIX epoch.  If a value of NULL is passed in
		 * 	the system's current time will be used.
		 * 	@return The literal expression.
		 */
		static Literal* MakeAbsTime( abstime_t *now=NULL );

		/* Creates an absolute time literal, from the string timestr, 
		 *parsing it as the regular expression:
		 D* dddd [D* dd [D* dd [D* dd [D* dd [D* dd D*]]]]] [-dddd | +dddd | z | Z]
		 D => non-digit, d=> digit
		 Ex - 2003-01-25T09:00:00-0600
		*/
		static Literal* MakeAbsTime( std::string timestr);

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
		static Literal* MakeRelTime( time_t t1, time_t t2 );

		/* Creates a relative time literal, from the string timestr, 
		 *parsing it as [[[days+]hh:]mm:]ss
		 * Ex - 1+00:02:00
		*/		
		static Literal* MakeRelTime(std::string str);

		/** Create a real literal from the given string.
		 * Use of a scaling factor in the string is not supported.
		 * @param realstr String representation of a floating-point value.
		 * @return The literal expression.
		 */
		static Literal* MakeReal(std::string realstr);

		/// optimized literal makers for common cases.
		// these skip the error setting reporting on purpose - leaving that to the caller if it is desired.
		static Literal* MakeBool(bool val) {
			Literal* lit = new Literal();
			if (lit) { lit->value.SetBooleanValue(val); }
			return lit;
		}
		static Literal* MakeLong(long long val) {
			Literal* lit = new Literal();
			if (lit) { lit->value.SetIntegerValue(val); }
			return lit;
		}
		static Literal* MakeReal(double real) {
			Literal* lit = new Literal();
			if (lit) { lit->value.SetRealValue(real); }
			return lit;
		}
		static Literal* MakeString(const std::string & str) {
			Literal* lit = new Literal();
			if (lit) { lit->value.SetStringValue(str); }
			return lit;
		}
		static Literal* MakeString(const char* str) {
			Literal* lit = new Literal();
			if (lit) { lit->value.SetStringValue(str); }
			return lit;
		}
		static Literal* MakeString(const char* str, size_t cch) {
			Literal* lit = new Literal();
			if (lit) { lit->value.SetStringValue(str, cch); }
			return lit;
		}
		static Literal* MakeError() {
			Literal* lit = new Literal();
			if (lit) { lit->value.SetErrorValue(); }
			return lit;
		}
		static Literal* MakeUndefined() {
			Literal* lit = new Literal();
			if (lit) { lit->value.SetUndefinedValue(); }
			return lit;
		}
		static Literal* MakeUndefined( Value * & v ) {
			Literal* lit = new Literal();
			// lit->value is already the undefined value.
			if (lit) { v = & lit->value; }
			return lit;
		}

		/// optimized value replacers
		void SetBool(bool val) { value.SetBooleanValue(val); }
		void SetReal(double val) { value.SetRealValue(val); }
		void SetLong(long long val) { value.SetIntegerValue(val); }
		void SetString(const char * str) { value.SetStringValue(str); }
		void SetString(const char * str, size_t cch) { value.SetStringValue(str, cch); }
		void SetString(const std::string & str) { value.SetStringValue(str); }
		void SetUndefined() { value.SetUndefinedValue(); }

		/// Make a deep copy
		virtual ExprTree* Copy( ) const {
			Literal *tree = new Literal(*this);
			if ( ! tree) setError(ERR_MEM_ALLOC_FAILED);
			return tree;
		}

		void CopyFrom(const Literal &lit) {
			ExprTree::CopyFrom(lit);
			value.CopyFrom(lit.value);
		}

		/** Factory method to construct a Literal
		 * @param v The value to convert to a literal. (Cannot be a classad or
		 * 			list value.)
		 * @param f The number factor (B, K, M, G, T) --- ignored for
		 * 			non-numeric values.
		 * @return The constructed literal expression
		 */
		static Literal*MakeLiteral( const Value& v, Value::NumberFactor f=
					Value::NO_FACTOR );

		/** Deconstructor to get the components of the literal 
		 * 	@param v The encapsulated value
		 * 	@param f The number factor (invalid if v is non-numeric)
		 */
		void GetComponents( Value& v, Value::NumberFactor &f ) const {
			v = value;
			// TJ: This is wrong, but necessary to preserve the fiction that factor lives in Literal.
			v.factor = Value::NO_FACTOR;
			f = value.factor;
		}

		const Value& getValue(Value::NumberFactor &f) const {
			// TJ: This is wrong, but necessary to preserve the fiction that factor lives in Literal.
			f = value.factor;
			return value;
		}

		/** Get the encapsulated value (with the factor applied)
		 * 	@param v The value encapsulated by the literal
		 */
		void GetValue( Value& val ) const {
			val.CopyFrom( value );
			val.ApplyFactor();
		}

		/** Special case fetch of the c_str() within a literal string
		 *  to avoid copying it into a new literal
		 */
		bool GetStringValue( const char * & cstr ) const { return value.IsStringValue(cstr); };

		/* Takes the number of seconds since the epoch as argument - epochsecs, 
		 *and returns the timezone offset(relative to GMT) in the currect locality
		 */
		static int findOffset(time_t epochsecs);

        virtual bool SameAs(const ExprTree *tree) const;

        friend bool operator==(Literal &literal1, Literal &literal2);

		virtual const ClassAd *GetParentScope( ) const { return NULL; }

	protected:
		/// Constructor
		Literal () {}
		static void setError(int err, const char *msg=NULL);

  	private:
		friend class FunctionCall;
		friend class ClassAd;
		friend class ExprList;
		friend class Operation;

		virtual void _SetParentScope( const ClassAd* ){ }
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
 		virtual bool _Evaluate (EvalState &, Value &) const;
 		virtual bool _Evaluate (EvalState &, Value &, ExprTree *&) const;

		// literal specific information
		Value value;
};

} // classad

#endif//__CLASSAD_LITERALS_H__
