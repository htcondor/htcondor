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
#include <limits>
#include "exprTree.h"

namespace classad {

class ErrorLiteral;
class UndefinedLiteral;
class BooleanLiteral;
class IntegerLiteral;
class RealLiteral;
class ReltimeLiteral;
class AbstimeLiteral;
class StringLiteral;

// abstract base class for the common code in all the
// concrete literal sub classes

class Literal: public ExprTree {
	public:
		virtual NodeKind GetKind (void) const = 0;
        virtual bool SameAs(const ExprTree *tree) const = 0;
		virtual const ClassAd *GetParentScope( ) const { return nullptr; }

		static AbstimeLiteral   *MakeAbsTime( abstime_t *now=NULL ); 
		static AbstimeLiteral   *MakeAbsTime( std::string timestr);
		static ReltimeLiteral   *MakeRelTime( time_t secs=-1 );
		static ReltimeLiteral   *MakeRelTime( time_t t1, time_t t2 );
		static ReltimeLiteral   *MakeRelTime(const std::string &str);
		static UndefinedLiteral *MakeUndefined();
		static ErrorLiteral     *MakeError();
		static BooleanLiteral   *MakeBool(bool val);
		static IntegerLiteral   *MakeInteger(int64_t i);
		static RealLiteral      *MakeReal(double d);
		static StringLiteral    *MakeString(const std::string &str);
		static StringLiteral    *MakeString(const char *);
		static StringLiteral    *MakeString(const char *, size_t size);
		static Literal          *MakeLiteral( const Value& v, Value::NumberFactor f=Value::NO_FACTOR);

		static int findOffset(time_t epochsecs);

		void GetValue(Value& val) const {
			EvalState dummy;
			this->_Evaluate(dummy, val);
		}

	protected:
		// no-op for all literals
		virtual void _SetParentScope(const ClassAd*) {}
		Literal(): ExprTree() {}
};
	// An empty instance.  Could be a singleton, if we cared enough
class ErrorLiteral: public Literal {
	public:
		virtual ~ErrorLiteral() {}
		ErrorLiteral(const ErrorLiteral &) : Literal() {}
		ErrorLiteral &operator=(const ErrorLiteral &) {return *this;}

		virtual NodeKind GetKind (void) const { return ERROR_LITERAL; }
		virtual ExprTree* Copy() const { return new ErrorLiteral();}
        virtual bool SameAs(const ExprTree *tree) const {return dynamic_cast<const ErrorLiteral *>(tree);} 
	protected:
		ErrorLiteral() {}

		// Always evaluates to error
 		virtual bool _Evaluate(EvalState &, Value &v) const {
			v.SetErrorValue();
			return true;
		};

		virtual bool _Flatten(EvalState &state, Value &val, ExprTree*& tree, int*) const {
			tree = nullptr;
			return(_Evaluate(state, val));
		}

 		virtual bool _Evaluate(EvalState &state, Value &value, ExprTree *&tree) const {
			_Evaluate(state, value);
			return ((tree = Copy()));
		};
		friend class Literal;
};
	
class UndefinedLiteral: public Literal {
	public:
		virtual ~UndefinedLiteral() {}
		UndefinedLiteral(const UndefinedLiteral &) : Literal() {}
		UndefinedLiteral &operator=(const UndefinedLiteral &) {return *this;}

		virtual NodeKind GetKind (void) const { return UNDEFINED_LITERAL; }
		virtual ExprTree* Copy() const { return new UndefinedLiteral();}
        virtual bool SameAs(const ExprTree *tree) const {return dynamic_cast<const UndefinedLiteral *>(tree);} 
	protected:
		UndefinedLiteral() {}

		// Always evaluates to Undefined
 		virtual bool _Evaluate(EvalState &, Value &v) const {
			v.SetUndefinedValue();
			return true;
		};

		virtual bool _Flatten(EvalState &state, Value &val, ExprTree*& tree, int*) const {
			tree = nullptr;
			return(_Evaluate(state, val));
		}

 		virtual bool _Evaluate(EvalState &state, Value &value, ExprTree *&tree) const {
			_Evaluate(state, value);
			return ((tree = Copy()));
		};
		friend class Literal;
};

class BooleanLiteral: public Literal {
	public:
		virtual ~BooleanLiteral() {}
		BooleanLiteral(const BooleanLiteral &rhs) : Literal(), _theBoolean(rhs._theBoolean) {}
		BooleanLiteral &operator=(const BooleanLiteral &rhs) {_theBoolean = rhs._theBoolean ; return *this;}

		virtual NodeKind GetKind (void) const { return BOOLEAN_LITERAL; }
		virtual ExprTree* Copy() const { return new BooleanLiteral(_theBoolean);}
        virtual bool SameAs(const ExprTree *tree) const {
			const BooleanLiteral *bl = dynamic_cast<const BooleanLiteral *>(tree); 
			if (bl && (bl->_theBoolean == _theBoolean)) {
				return true;
			}
			return false;
		}
		bool getBool() const {
			return _theBoolean;
		}

	protected:
		BooleanLiteral(bool b): _theBoolean(b) {}
		virtual void _SetParentScope(const ClassAd*) {}

		// Always evaluates to true/false
 		virtual bool _Evaluate(EvalState &, Value &v) const {
			v.SetBooleanValue(_theBoolean);
			return true;
		};

		virtual bool _Flatten(EvalState &state, Value &val, ExprTree*& tree, int*) const {
			tree = nullptr;
			return(_Evaluate(state, val));
		}

 		virtual bool _Evaluate(EvalState &state, Value &value, ExprTree *&tree) const {
			_Evaluate(state, value);
			return ((tree = Copy()));
		};

	private:
		bool _theBoolean;
		friend class Literal;
};

class IntegerLiteral: public Literal {
	public:
		virtual ~IntegerLiteral() {}
		IntegerLiteral(const IntegerLiteral &rhs) : Literal(), _theInteger(rhs._theInteger) {}
		IntegerLiteral &operator=(const IntegerLiteral &rhs) {_theInteger = rhs._theInteger; return *this;}

		virtual NodeKind GetKind (void) const { return INTEGER_LITERAL; }
		virtual ExprTree* Copy() const { return new IntegerLiteral(_theInteger);}
        virtual bool SameAs(const ExprTree *tree) const {
			const IntegerLiteral *bl = dynamic_cast<const IntegerLiteral *>(tree); 
			if (bl && (bl->_theInteger == _theInteger)) {
				return true;
			}
			return false;
		}

		int64_t getInteger() const {
			return _theInteger;
		}

	protected:
		IntegerLiteral(int64_t i): _theInteger(i) {}

		// Always evaluates to the integer
 		virtual bool _Evaluate(EvalState &, Value &v) const {
			v.SetIntegerValue(_theInteger);
			return true;
		};

		virtual bool _Flatten(EvalState &state, Value &val, ExprTree*& tree, int*) const {
			tree = nullptr;
			return(_Evaluate(state, val));
		}

 		virtual bool _Evaluate(EvalState &state, Value &value, ExprTree *&tree) const {
			_Evaluate(state, value);
			return ((tree = Copy()));
		};

	private:
		int64_t _theInteger;
		friend class Literal;
};

class RealLiteral: public Literal {
	public:
		virtual ~RealLiteral() {}
		RealLiteral(const RealLiteral &rhs) : Literal(), _theReal(rhs._theReal) {}
		RealLiteral &operator=(const RealLiteral &rhs) {_theReal = rhs._theReal; return *this;}

		virtual NodeKind GetKind (void) const { return REAL_LITERAL; }
		virtual ExprTree* Copy() const { return new RealLiteral(_theReal);}
        virtual bool SameAs(const ExprTree *tree) const {
			const RealLiteral *rl = dynamic_cast<const RealLiteral *>(tree); 

			// The old code did a strict == here, but avoid warning and NaNsense this way
			if (rl && (fabs(rl->_theReal - _theReal) <= std::numeric_limits<double>::epsilon())) {
				return true;
			}
			return false;
		}

		double getReal() const {
			return _theReal;
		}

	protected:
		RealLiteral(double d): _theReal(d) {}

		// Always evaluates to the double
 		virtual bool _Evaluate(EvalState &, Value &v) const {
			v.SetRealValue(_theReal);
			return true;
		};

		virtual bool _Flatten(EvalState &state, Value &val, ExprTree*& tree, int*) const {
			tree = nullptr;
			return(_Evaluate(state, val));
		}

 		virtual bool _Evaluate(EvalState &state, Value &value, ExprTree *&tree) const {
			_Evaluate(state, value);
			return ((tree = Copy()));
		};

	private:
		double _theReal;
		friend class Literal;
};

class ReltimeLiteral: public Literal {
	public:
		virtual ~ReltimeLiteral() {}
		ReltimeLiteral(const ReltimeLiteral &rhs) : Literal(), _theReltime(rhs._theReltime) {}
		ReltimeLiteral &operator=(const ReltimeLiteral &rhs) {_theReltime = rhs._theReltime; return *this;}

		virtual NodeKind GetKind (void) const { return RELTIME_LITERAL; }
		virtual ExprTree* Copy() const { return new ReltimeLiteral(_theReltime);}
        virtual bool SameAs(const ExprTree *tree) const {
			const ReltimeLiteral *rl = dynamic_cast<const ReltimeLiteral *>(tree); 
			if (rl && (fabs(rl->_theReltime - _theReltime) <= std::numeric_limits<double>::epsilon())) {
				return true;
			}
			return false;
		}
		double getReltime() const {
			return _theReltime;
		}

	protected:
		ReltimeLiteral(double d): _theReltime(d) {}

		// Always evaluates to the double
 		virtual bool _Evaluate(EvalState &, Value &v) const {
			v.SetRealValue(_theReltime);
			return true;
		};

		virtual bool _Flatten(EvalState &state, Value &val, ExprTree*& tree, int*) const {
			tree = nullptr;
			return(_Evaluate(state, val));
		}

 		virtual bool _Evaluate(EvalState &state, Value &value, ExprTree *&tree) const {
			_Evaluate(state, value);
			return ((tree = Copy()));
		};

	private:
		double _theReltime;
		friend class Literal;
};

class AbstimeLiteral: public Literal {
	public:
		virtual ~AbstimeLiteral() {}
		AbstimeLiteral(const AbstimeLiteral &rhs ) : Literal(), _theAbstime(rhs._theAbstime) {}
		AbstimeLiteral &operator=(const AbstimeLiteral &rhs) {_theAbstime = rhs._theAbstime; return *this;}

		virtual NodeKind GetKind (void) const { return ABSTIME_LITERAL; }
		virtual ExprTree* Copy() const { return new AbstimeLiteral(_theAbstime);}
        virtual bool SameAs(const ExprTree *tree) const {
			const AbstimeLiteral *al = dynamic_cast<const AbstimeLiteral *>(tree); 
			if (al && (al->_theAbstime.secs == _theAbstime.secs) && (al->_theAbstime.offset == _theAbstime.offset)) {
				return true;
			}
			return false;
		}
		abstime_t getAbstime() const {
			return _theAbstime;
		}

	protected:
		AbstimeLiteral(abstime_t a): _theAbstime(a) {}

		// Always evaluates to the abstime
 		virtual bool _Evaluate(EvalState &, Value &v) const {
			v.SetAbsoluteTimeValue(_theAbstime);
			return true;
		};

		virtual bool _Flatten(EvalState &state, Value &val, ExprTree*& tree, int*) const {
			tree = nullptr;
			return(_Evaluate(state, val));
		}

 		virtual bool _Evaluate(EvalState &state, Value &value, ExprTree *&tree) const {
			_Evaluate(state, value);
			return ((tree = Copy()));
		};

	private:
		abstime_t _theAbstime;
		friend class Literal;
};

class StringLiteral: public Literal {
	public:
		virtual ~StringLiteral() {}
		StringLiteral(const StringLiteral &rhs) : Literal(), _theString(rhs._theString) {}
		StringLiteral &operator=(const StringLiteral &rhs) {_theString = rhs._theString; return *this;}

		const char *getCString() const { 
			return _theString.c_str();;
		}

		const std::string &getString() const {
			return _theString;
		}

		virtual NodeKind GetKind (void) const { return STRING_LITERAL; }
		virtual ExprTree* Copy() const { return new StringLiteral(_theString);}
        virtual bool SameAs(const ExprTree *tree) const {
			const StringLiteral *sl = dynamic_cast<const StringLiteral *>(tree); 
			if (sl && (sl->_theString == _theString)) {
				return true;
			}
			return false;
		}

	protected:
		StringLiteral(const std::string &s): _theString(s) {}

		// Always evaluates to the abstime
 		virtual bool _Evaluate(EvalState &, Value &v) const {
			v.SetStringValue(_theString);
			return true;
		};

		virtual bool _Flatten(EvalState &state, Value &val, ExprTree*& tree, int*) const {
			tree = nullptr;
			return(_Evaluate(state, val));
		}

 		virtual bool _Evaluate(EvalState &state, Value &value, ExprTree *&tree) const {
			_Evaluate(state, value);
			return ((tree = Copy()));
		};

	private:
		std::string _theString;
		friend class Literal;
};

typedef std::vector<ExprTree*> ArgumentList;

/*
ERROR_VALUE         = 1<<0,
UNDEFINED_VALUE     = 1<<1,
BOOLEAN_VALUE 		= 1<<2,
INTEGER_VALUE       = 1<<3,
REAL_VALUE          = 1<<4,
RELATIVE_TIME_VALUE = 1<<5,
ABSOLUTE_TIME_VALUE = 1<<6,
STRING_VALUE        = 1<<7,
CLASSAD_VALUE       = 1<<8,
LIST_VALUE 	= 1<<9,
SLIST_VALUE	= 1<<10,
*/

inline BooleanLiteral* 
Literal::MakeBool(bool val) {
	return new BooleanLiteral(val);
}

inline IntegerLiteral* 
Literal::MakeInteger(int64_t val) {
	return new IntegerLiteral(val);
}

inline RealLiteral* 
Literal::MakeReal(double real) {
	return new RealLiteral(real);
}
inline StringLiteral* 
Literal::MakeString(const std::string & str) {
	return new StringLiteral(str);
}
inline StringLiteral* 
Literal::MakeString(const char* str) {
	return new StringLiteral(str ? str : "");
}

inline StringLiteral* 
Literal::MakeString(const char* str, size_t cch) {
	if (str == nullptr) {
		return new StringLiteral("");
	}
	return new StringLiteral(std::string(str, cch));
}
inline ErrorLiteral* 
Literal::MakeError() {
	return new ErrorLiteral();
}
inline UndefinedLiteral *
Literal::MakeUndefined() {
	return new UndefinedLiteral();
}

inline Literal*
Literal::MakeLiteral(const Value &v, Value::NumberFactor) {
	switch (v.valueType) {
		case Value::UNDEFINED_VALUE:
			return new UndefinedLiteral;
			break;
		case Value::ERROR_VALUE:
			return new ErrorLiteral;
			break;
		case Value::STRING_VALUE: {
			std::string s;
			std::ignore = v.IsStringValue(s);
			return new StringLiteral(s);
			break;
		}
		case Value::REAL_VALUE: {
			double d = 0;
			std::ignore = v.IsRealValue(d);
			return new RealLiteral(d);
			break;
		}
		case Value::INTEGER_VALUE: {
			int64_t i = 0;
			std::ignore = v.IsIntegerValue(i);
			return new IntegerLiteral(i);
			break;
		}
		case Value::BOOLEAN_VALUE: {
			bool b = true;
			std::ignore = v.IsBooleanValue(b);
			return new BooleanLiteral(b);
			break;
		}
		case Value::ABSOLUTE_TIME_VALUE: {
			classad::abstime_t atime {};
			std::ignore = v.IsAbsoluteTimeValue(atime);
			return new AbstimeLiteral(atime);
			break;
		}
		case Value::RELATIVE_TIME_VALUE: {
			time_t seconds = 0;
			std::ignore = v.IsRelativeTimeValue(seconds);
			return new ReltimeLiteral(seconds);
			break;
		}
		default:
			return nullptr;
	}
	return nullptr;
}


} // classad

#endif//__CLASSAD_LITERALS_H__
