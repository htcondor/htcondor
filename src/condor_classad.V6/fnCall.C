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

#include "condor_common.h"
#include "exprTree.h"
#include <regex.h>

BEGIN_NAMESPACE( classad )

FunctionCall::FuncTable FunctionCall::functionTable;
bool FunctionCall::initialized = false;

// start up with an argument list of size 4
FunctionCall::
FunctionCall( )
{
	nodeKind = FN_CALL_NODE;
	function = NULL;

	if( !initialized ) {
		// load up the function dispatch table
			// type predicates
		functionTable["isundefined"	] = isType;
		functionTable["iserror"		] =	isType;
		functionTable["isstring"	] =	isType;
		functionTable["isinteger"	] =	isType;
		functionTable["isreal"		] =	isType;
		functionTable["islist"		] =	isType;
		functionTable["isclassad"	] =	isType;
		functionTable["isboolean"	] =	isType;
		functionTable["isabstime"	] =	isType;
		functionTable["isreltime"	] =	isType;

			// list membership
		functionTable["member"		] =	testMember;
		functionTable["ismember"	] =	testMember;

			// basic apply-like functions
		/*
		functionTable["sumfrom"		sumAvgFrom );
		functionTable["avgfrom"		sumAvgFrom );
		functionTable["maxfrom"		boundFrom );
		functionTable["minfrom"		boundFrom );
		*/

			// time management
		functionTable["currenttime"	] =	currentTime;
		functionTable["timezoneoffset"] =timeZoneOffset;
		functionTable["daytime"		] =	dayTime;
		functionTable["makedate"	] =	makeDate;
		functionTable["makeabstime"	] =	makeTime;
		functionTable["makereltime"	] =	makeTime;
		functionTable["getyear"		] =	getField;
		functionTable["getmonth"	] =	getField;
		functionTable["getdayofyear"] =	getField;
		functionTable["getdayofmonth"] =getField;
		functionTable["getdayofweek"] =	getField;
		functionTable["getdays"		] =	getField;
		functionTable["gethours"	] =	getField;
		functionTable["getminutes"	] =	getField;
		functionTable["getseconds"	] =	getField;
		functionTable["indays"		] =	inTimeUnits;
		functionTable["inhours"		] =	inTimeUnits;
		functionTable["inminutes"	] =	inTimeUnits;
		functionTable["inseconds"	] =	inTimeUnits;

			// string manipulation
		functionTable["strcat"		] =	strCat;
		functionTable["toupper"		] =	changeCase;
		functionTable["tolower"		] =	changeCase;
		functionTable["substr"		] =	subString;

			// pattern matching (regular expressions) 
		functionTable["regexp"		] =	matchPattern;

			// conversion functions
		functionTable["int"			] =	convInt;
		functionTable["real"		] =	convReal;
		functionTable["string"		] =	convString;
		functionTable["bool"		] =	convBool;
		functionTable["abstime"		] =	convTime;
		functionTable["reltime"		] = convTime;

			// mathematical functions
		functionTable["floor"		] =	doMath;
		functionTable["ceil"		] =	doMath;
		functionTable["round"		] =	doMath;

		initialized = true;
	}
}

FunctionCall::
~FunctionCall ()
{
	for( ArgumentList::iterator i=arguments.begin(); i!=arguments.end(); i++ ) {
		delete (*i);
	}
}


FunctionCall *FunctionCall::
Copy( ) const
{
	FunctionCall *newTree = new FunctionCall;
	ExprTree *newArg;

	if (!newTree) return NULL;

	newTree->functionName = functionName;
	newTree->parentScope = parentScope;
	newTree->function    = function;

	for( ArgumentList::const_iterator i=arguments.begin(); i!=arguments.end();
			i++ ) {
		newArg = (*i)->Copy( );
		if( newArg ) {
			newTree->arguments.push_back( newArg );
		} else {
			delete newTree;
			return NULL;
		}
	}
	return newTree;
}


void FunctionCall::
_SetParentScope( const ClassAd* parent )
{
	for( ArgumentList::iterator i=arguments.begin(); i!=arguments.end(); i++ ) {
		(*i)->SetParentScope( parent );
	}
}
	

FunctionCall *FunctionCall::
MakeFunctionCall( const string &str, vector<ExprTree*> &args )
{
	FunctionCall *fc = new FunctionCall;
	if( !fc ) {
		vector<ExprTree*>::iterator i = args.begin( );
		while( *i ) {
			delete *i;
			i++;
		}
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( NULL );
	}

	FuncTable::iterator	itr = functionTable.find( str );
	if( itr != functionTable.end( ) ) {
		fc->function = itr->second;
	} else {
		fc->function = NULL;
	}

	fc->functionName = str;

	for( ArgumentList::iterator i = args.begin(); i != args.end( ); i++ ) {
		fc->arguments.push_back( *i );
	}
	return( fc );
}


void FunctionCall::
GetComponents( string &fn, vector<ExprTree*> &args ) const
{
	fn = functionName;
	for( ArgumentList::const_iterator i = arguments.begin(); 
			i != arguments.end( ); i++ ) {
		args.push_back( *i );
	}
}


bool FunctionCall::
_Evaluate (EvalState &state, Value &value) const
{
	if( function ) {
		return( (*function)( functionName.c_str( ), arguments, state, value ) );
	} else {
		value.SetErrorValue();
		return( true );
	}
}

bool FunctionCall::
_Evaluate( EvalState &state, Value &value, ExprTree *& tree ) const
{
	FunctionCall *tmpSig = new FunctionCall;
	Value		tmpVal;
	ExprTree	*argSig;
	bool		rval;

	if( !tmpSig ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( false );
	}
	
	if( !_Evaluate( state, value ) ) {
		return false;
	}

	tmpSig->functionName = functionName;
	rval = true;
	for(ArgumentList::const_iterator i=arguments.begin();i!=arguments.end();
			i++) {
		rval = (*i)->Evaluate( state, tmpVal, argSig );
		if( rval ) tmpSig->arguments.push_back( argSig );
	}
	tree = tmpSig;

	if( !rval ) delete tree;
	return( rval );
}

bool FunctionCall::
_Flatten( EvalState &state, Value &value, ExprTree*&tree, OpKind* ) const
{
	FunctionCall *newCall;
	ExprTree	*argTree;
	Value		argValue;
	bool		fold = true;

	// if the function cannot be resolved, the value is "error"
	if( !function ) {
		value.SetErrorValue();
		tree = NULL;
		return true;
	}

	// create a residuated function call with flattened args
	if( ( newCall = new FunctionCall() ) == NULL ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return false;
	}
	newCall->functionName = functionName;

	// flatten the arguments
	for(ArgumentList::const_iterator i=arguments.begin();i!=arguments.end();
			i++ ) {
		if( (*i)->Flatten( state, argValue, argTree ) ) {
			if( argTree ) {
				newCall->arguments.push_back( argTree );
				fold = false;
				continue;
			} else {
				// Assert: argTree == NULL
				argTree = Literal::MakeLiteral( argValue );
				if( argTree ) {
					newCall->arguments.push_back( argTree );
					continue;
				}
			}
		} 

		// we get here only when something bad happens
		delete newCall;
		value.SetErrorValue();
		tree = NULL;
		return false;
	} 
	
	// assume all functions are "pure" (i.e., side-affect free)
	if( fold ) {
			// flattened to a value
		if(!(*function)(functionName.c_str(),arguments,state,value)) {
			return false;
		}
		tree = NULL;
		delete newCall;
	} else {
		tree = newCall;
	}

	return true;
}


bool FunctionCall::
isType (const char *name, const ArgumentList &argList, EvalState &state, 
	Value &val)
{
    Value   arg;

    // need a single argument
    if (argList.size() != 1) {
        val.SetErrorValue ();
        return( true );
    }

    // Evaluate the argument
    if( !argList[0]->Evaluate( state, arg ) ) {
		val.SetErrorValue( );
		return false;
	}

    // check if the value was of the required type
	if( strcasecmp( name, "isundefined" ) == 0 ) {
		val.SetBooleanValue( arg.IsUndefinedValue( ) );
	} else if( strcasecmp( name, "iserror" ) == 0 ) {
		val.SetBooleanValue( arg.IsErrorValue( ) );
	} else if( strcasecmp( name, "isinteger" ) == 0 ) {
		val.SetBooleanValue( arg.IsIntegerValue( ) );
	} else if( strcasecmp( name, "isstring" ) == 0 ) {
		val.SetBooleanValue( arg.IsStringValue( ) );
	} else if( strcasecmp( name, "isreal" ) == 0 ) {
		val.SetBooleanValue( arg.IsRealValue( ) );
	} else if( strcasecmp( name, "isboolean" ) == 0 ) {
		val.SetBooleanValue( arg.IsBooleanValue( ) );
	} else if( strcasecmp( name, "isclassad" ) == 0 ) {
		val.SetBooleanValue( arg.IsClassAdValue( ) );
	} else if( strcasecmp( name, "islist" ) == 0 ) {
		val.SetBooleanValue( arg.IsListValue( ) );
	} else if( strcasecmp( name, "isabstime" ) == 0 ) {
		val.SetBooleanValue( arg.IsAbsoluteTimeValue( ) );
	} else if( strcasecmp( name, "isreltime" ) == 0 ) {
		val.SetBooleanValue( arg.IsRelativeTimeValue( ) );
	} else {
		val.SetErrorValue( );
	}
	return( true );
}


bool FunctionCall::
testMember(const char *name,const ArgumentList &argList, EvalState &state, 
	Value &val)
{
    Value     		arg0, arg1, cArg;
    const ExprTree 	*tree;
	const ExprList	*el;
	bool			b;
	bool			useIS = ( strcasecmp( "ismember", name ) == 0 );

    // need two arguments
    if (argList.size() != 2) {
        val.SetErrorValue();
        return( true );
    }

    // Evaluate the arg list
    if( !argList[0]->Evaluate(state,arg0) || !argList[1]->Evaluate(state,arg1)){
		val.SetErrorValue( );
		return false;
	}

		// if the first arg (a list) is undefined, or the second arg is
		// undefined and we're supposed to test for strict comparison, the 
		// result is 'undefined'
    if( arg0.IsUndefinedValue() || ( !useIS && arg1.IsUndefinedValue() ) ) {
        val.SetUndefinedValue();
        return true;
    }

		// arg0 must be a list; arg1 must be comparable
    if( !arg0.IsListValue() || arg1.IsListValue() || arg1.IsClassAdValue() ) {
        val.SetErrorValue();
        return true;
    }

		// if we're using strict comparison, arg1 can't be 'error'
	if( !useIS && arg1.IsErrorValue( ) ) {
		val.SetErrorValue( );
		return( true );
	}

    // check for membership
	arg0.IsListValue( el );
	ExprListIterator itr( el );
	while( ( tree = itr.NextExpr( ) ) ) {
		if( !tree->Evaluate( state, cArg ) ) {
			val.SetErrorValue( );
			return( false );
		}
		Operation::Operate( useIS?IS_OP:EQUAL_OP, cArg, arg1, val );
		if( val.IsBooleanValue( b ) && b ) {
			return true;
		}
	}
	val.SetBooleanValue( false );	

    return true;
}

/*
bool FunctionCall::
sumAvgFrom (char *name, const ArgumentList &argList, EvalState &state, Value &val)
{
	Value		caVal, listVal;
	ExprTree	*ca;
	Value		tmp, result;
	ExprList	*el;
	ClassAd		*ad;
	bool		first = true;
	int			len;
	bool		onlySum = ( strcasecmp( "sumfrom", name ) == 0 );

	// need two arguments
	if( argList.getlast() != 1 ) {
		val.SetErrorValue();
		return true;
	}

	// first argument must Evaluate to a list
	if( !argList[0]->Evaluate( state, listVal ) ) {
		val.SetErrorValue( );
		return( false );
	} else if( listVal.IsUndefinedValue() ) {
		val.SetUndefinedValue( );
		return( true );
	} else if( !listVal.IsListValue( el ) ) {
		val.SetErrorValue();
		return( true );
	}

	el->Rewind();
	result.SetUndefinedValue();
	while( ( ca = el->Next() ) ) {
		if( !ca->Evaluate( state, caVal ) ) {
			val.SetErrorValue( );
			return( false );
		} else if( !caVal.IsClassAdValue( ad ) ) {
			val.SetErrorValue();
			return( true );
		} else if( !ad->EvaluateExpr( argList[1], tmp ) ) {
			val.SetErrorValue( );
			return( false );
		}
		if( first ) {
			result.CopyFrom( tmp );
			first = false;
		} else {
			Operation::Operate( ADDITION_OP, result, tmp, result );
		}
		tmp.Clear();
	}

		// if the sumFrom( ) function was called, don't need to find average
	if( onlySum ) {
		val.CopyFrom( result );
		return( true );
	}


	len = el->Number();
	if( len > 0 ) {
		tmp.SetRealValue( len );
		Operation::Operate( DIVISION_OP, result, tmp, result );
	} else {
		val.SetUndefinedValue();
	}

	val.CopyFrom( result );
	return true;
}


bool FunctionCall::
boundFrom (char *fn, const ArgumentList &argList, EvalState &state, Value &val)
{
	Value		caVal, listVal, cmp;
	ExprTree	*ca;
	Value		tmp, result;
	ExprList	*el;
	ClassAd		*ad;
	bool		first = true, b=false, min;

	// need two arguments
	if( argList.getlast() != 1 ) {
		val.SetErrorValue();
		return( true );
	}

	// first argument must Evaluate to a list
	if( !argList[0]->Evaluate( state, listVal ) ) {
		val.SetErrorValue( );
		return( false );
	} else if( listVal.IsUndefinedValue( ) ) {
		val.SetUndefinedValue( );
		return( true );
	} else if( !listVal.IsListValue( el ) ) {
		val.SetErrorValue();
		return( true );
	}

	// fn is either "min..." or "max..."
	min = ( tolower( fn[1] ) == 'i' );

	el->Rewind();
	result.SetUndefinedValue();
	while( ( ca = el->Next() ) ) {
		if( !ca->Evaluate( state, caVal ) ) {
			val.SetErrorValue( );
			return false;
		} else if( !caVal.IsClassAdValue( ad ) ) {
			val.SetErrorValue();
			return( true );
		} else if( !ad->EvaluateExpr( argList[1], tmp ) ) {
			val.SetErrorValue( );
			return( true );
		}
		if( first ) {
			result.CopyFrom( tmp );
			first = false;
		} else {
			Operation::Operate(min?LESS_THAN_OP:GREATER_THAN_OP,tmp,result,cmp);
			if( cmp.IsBooleanValue( b ) && b ) {
				result.CopyFrom( tmp );
			}
		}
	}

	val.CopyFrom( result );
	return true;
}
*/

bool FunctionCall::
currentTime (const char *, const ArgumentList &argList, EvalState &, Value &val)
{
	time_t clock;

		// no arguments
	if( argList.size( ) > 0 ) {
		val.SetErrorValue( );
		return( true );
	}

	if( time( &clock ) < 0 ) {
		val.SetErrorValue( );
		return( false );
	}

	val.SetAbsoluteTimeValue( (int) clock );
	return( true );
}


bool FunctionCall::
timeZoneOffset (const char *, const ArgumentList &argList, EvalState &, 
	Value &val)
{
	extern long timezone;

		// no arguments
	if( argList.size( ) > 0 ) {
		val.SetErrorValue( );
		return( true );
	}

		// POSIX says that timezone is positive west of GMT;  we reverse it
		// here to make it more intuitive
	val.SetRelativeTimeValue( (int) -timezone );
	return( true );
}

bool FunctionCall::
dayTime (const char *, const ArgumentList &argList, EvalState &, Value &val)
{
	time_t 		now;
	struct tm 	lt;
	if( argList.size( ) > 0 ) {
		val.SetErrorValue( );
		return( true );
	}
	time( &now );
	if( now == -1 ) {
		val.SetErrorValue( );
		return( false );
	}
	localtime_r( &now, &lt );
	val.SetRelativeTimeValue( lt.tm_hour*3600 + lt.tm_min*60 + lt.tm_sec );
	return( true );
}

bool FunctionCall::
makeDate( const char*, const ArgumentList &argList, EvalState &state, 
	Value &val )
{
	Value 	arg0, arg1, arg2;
	int		dd, mm, yy;
	time_t	clock;
	struct	tm	tms;
	char	buffer[64];
	string	month;

		// two or three arguments
	if( argList.size( ) < 2 || argList.size( ) > 3 ) {
		val.SetErrorValue( );
		return( true );
	}

		// evaluate arguments
	if( !argList[0]->Evaluate( state, arg0 ) || 
		!argList[1]->Evaluate( state, arg1 ) ) {
		val.SetErrorValue( );
		return( false );
	}

		// get current time in tm struct
	if( time( &clock ) == -1 ) {
		val.SetErrorValue( );
		return( false );
	}
	gmtime_r( &clock, &tms );
	
		// evaluate optional third argument
	if( argList.size( ) == 3 ) {
		if( !argList[2]->Evaluate( state, arg2 ) ) {
			val.SetErrorValue( );
			return( false );
		}
	} else {
			// use the current year (tm_year is years since 1900)
		arg2.SetIntegerValue( tms.tm_year + 1900 );
	}
		
		// check if any of the arguments are undefined
	if( arg0.IsUndefinedValue( ) || arg1.IsUndefinedValue( ) || 
		arg2.IsUndefinedValue( ) ) {
		val.SetUndefinedValue( );
		return( true );
	}

		// first and third arguments must be integers (year should be >= 1970)
	if( !arg0.IsIntegerValue( dd ) || !arg2.IsIntegerValue( yy ) || yy < 1970 ){
		val.SetErrorValue( );
		return( true );
	}

		// the second argument must be integer or string
	if( arg1.IsIntegerValue( mm ) ) {
		if( sprintf( buffer, "%d %d %d", dd, mm, yy ) > 63 ||
			strptime( buffer, "%d %m %Y", &tms ) == NULL ) {
			val.SetErrorValue( );
			return( true );
		}
	} else if( arg1.IsStringValue( month ) ) {
		if( sprintf( buffer, "%d %s %d", dd, month.c_str( ), yy ) > 63 ||
				strptime( buffer, "%d %b %Y", &tms ) == NULL ) {
			val.SetErrorValue( );
			return( true );
		}
	} else {
		val.SetErrorValue( );
		return( true );
	}

		// convert the struct tm->time_t->absolute time
	clock = mktime( &tms );
	val.SetAbsoluteTimeValue( (int) clock );
	return( true );
}


bool FunctionCall::
makeTime( const char*name, const ArgumentList &argList, EvalState &state, 
	Value &val )
{
	Value 	arg;
	int		i;

	if( argList.size( ) != 1 ) {
		val.SetErrorValue( );
		return( true );
	}

	if( !argList[0]->Evaluate( state, arg ) ) {
		val.SetErrorValue( );
		return false;	
	}

	if( arg.IsUndefinedValue( ) ) {
		val.SetUndefinedValue( );
		return( true );
	}

	if( !arg.IsNumber( i ) ) {
		val.SetErrorValue( );
		return( true );
	}

	if( strcasecmp( name, "makeabstime" ) == 0 ) {
		val.SetAbsoluteTimeValue( i );
	} else {
		val.SetRelativeTimeValue( i );
	}
	return( true );
}

bool FunctionCall::
getField(const char* name, const ArgumentList &argList, EvalState &state, 
	Value &val )
{
	Value 	arg;
	time_t 	secs;
	time_t	clock;
	struct  tm tms;

	if( argList.size( ) != 1 ) {
		val.SetErrorValue( );
		return( true );
	}

	if( !argList[0]->Evaluate( state, arg ) ) {
		val.SetErrorValue( );
		return false;	
	}

	if( arg.IsAbsoluteTimeValue( secs ) ) {
		clock = secs;
		gmtime_r( &clock, &tms );
		if( strcasecmp( name, "getyear" ) == 0 ) {
				// tm_year is years since 1900 --- make it y2k compliant :-)
			val.SetIntegerValue( tms.tm_year + 1900 );
		} else if( strcasecmp( name, "getmonth" ) == 0 ) {
			val.SetIntegerValue( tms.tm_mon );
		} else if( strcasecmp( name, "getdayofyear" ) == 0 ) {
			val.SetIntegerValue( tms.tm_yday );
		} else if( strcasecmp( name, "getdayofmonth" ) == 0 ) {
			val.SetIntegerValue( tms.tm_mday );
		} else if( strcasecmp( name, "getdayofweek" ) == 0 ) {
			val.SetIntegerValue( tms.tm_wday );
		} else if( strcasecmp( name, "gethours" ) == 0 ) {
			val.SetIntegerValue( tms.tm_hour );
		} else if( strcasecmp( name, "getminutes" ) == 0 ) {
			val.SetIntegerValue( tms.tm_min );
		} else if( strcasecmp( name, "getseconds" ) == 0 ) {
			val.SetIntegerValue( tms.tm_sec );
		} else if( strcasecmp( name, "getdays" ) == 0 ||
			strcasecmp( name, "getuseconds" ) == 0 ) {
				// not meaningful for abstimes
			val.SetErrorValue( );
			return( true );
		} else {
			EXCEPT( "Should not reach here" );
			val.SetErrorValue( );
			return( false );
		}
		return( true );
	} else if( arg.IsRelativeTimeValue( secs ) ) {
		if( strcasecmp( name, "getyear" ) == 0  	||
			strcasecmp( name, "getmonth" ) == 0  	||
			strcasecmp( name, "getdayofmonth" )== 0 ||
			strcasecmp( name, "getdayofweek" ) == 0 ||
			strcasecmp( name, "getdayofyear" ) == 0 ) {
				// not meaningful for reltimes
			val.SetErrorValue( );
			return( true );
		} else if( strcasecmp( name, "getdays" ) == 0 ) {
			val.SetIntegerValue( secs / 86400 );
		} else if( strcasecmp( name, "gethours" ) == 0 ) {
			val.SetIntegerValue( ( secs % 86400 ) / 3600 );
		} else if( strcasecmp( name, "getminutes" ) == 0 ) {
			val.SetIntegerValue( ( secs % 3600 ) / 60 );
		} else if( strcasecmp( name, "getseconds" ) == 0 ) {
			val.SetIntegerValue( secs % 60 );
		} else {
			EXCEPT( "Should not reach here" );
			val.SetErrorValue( );
			return( false );
		}
		return( true );
	}

	val.SetErrorValue( );
	return( true );
}

bool FunctionCall::
inTimeUnits(const char*name,const ArgumentList &argList,EvalState &state, 
	Value &val )
{
	Value 	arg;
	time_t	asecs=0, rsecs=0;
	double	secs=0.0;

    if( argList.size( ) != 1 ) {
        val.SetErrorValue( );
        return( true );
    }

    if( !argList[0]->Evaluate( state, arg ) ) {
        val.SetErrorValue( );
        return false;
    }

		// only handle times
	if( !arg.IsAbsoluteTimeValue(asecs) && 
		!arg.IsRelativeTimeValue(rsecs) ) {
		val.SetErrorValue( );
		return( true );
	}

	if( arg.IsAbsoluteTimeValue( ) ) {
		secs = asecs;
	} else if( arg.IsRelativeTimeValue( ) ) {	
		secs = rsecs;
	}

	if (strcasecmp( name, "indays" ) == 0 ) {
		val.SetRealValue( secs / 86400.0 );
		return( true );
	} else if( strcasecmp( name, "inhours" ) == 0 ) {
		val.SetRealValue( secs / 3600.0 );
		return( true );
	} else if( strcasecmp( name, "inminutes" ) == 0 ) {
		val.SetRealValue( secs / 60.0 );
	} else if( strcasecmp( name, "inseconds" ) == 0 ) {
		val.SetRealValue( secs );
		return( true );
	}

	val.SetErrorValue( );
	return( true );
}

	// concatenate all arguments (expected to be strings)
bool FunctionCall::
strCat( const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	ClassAdUnParser	unp;
	string			buf, s;
	Value			val;
	bool			errorFlag=false, undefFlag=false, rval=true;

	for( int i = 0 ; (unsigned)i < argList.size() ; i++ ) {
		s = "";
		if( !( rval = argList[i]->Evaluate( state, val ) ) ) {
			break;
		}
		if( val.IsUndefinedValue( ) ) {
			 undefFlag = true;
		} else 
		if( val.IsErrorValue() || val.IsClassAdValue() || val.IsListValue() ) {
			errorFlag = true;
			break;
		} else {
			if( val.IsStringValue( s ) ) {
				buf += s;
			} else {
				unp.Unparse( s, val );
				if( val.IsAbsoluteTimeValue() || val.IsRelativeTimeValue() ) {
					buf += s.substr(1, s.size()-2 );
				} else {
					buf += s;
				}
			}
		}
	}
	
		// failed evaluating some argument
	if( !rval ) {
		result.SetErrorValue( );
		return( false );
	}
		// type error
	if( errorFlag ) {
		result.SetErrorValue( );
		return( true );
	} 
		// some argument was undefined
	if( undefFlag ) {
		result.SetUndefinedValue( );
		return( true );
	}

	result.SetStringValue( buf );
	return( true );
}

bool FunctionCall::
changeCase(const char*name,const ArgumentList &argList,EvalState &state,
	Value &result)
{
	Value 	val;
	char	*str=0, *newstr;
	bool	lower = ( strcasecmp( name, "tolower" ) == 0 );
	int		len;

		// only one argument 
	if( argList.size() != 1 ) {
		result.SetErrorValue( );
		return( true );
	}

		// check for evaluation failure
	if( !argList[0]->Evaluate( state, val ) ) {
		result.SetErrorValue( );
		return( false );
	}

		// only strings allowed; if so handover string from the object	
	if( val.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return( true );
	} else if( !val.IsStringValue( str ) ) {
		result.SetErrorValue( );
		return( true );
	}

	if( !str || ( ( newstr = strnewp( str ) ) == NULL ) ) {
		result.SetErrorValue( );
		return( false );
	}

	len = strlen( newstr );
	for( int i=0; i <= len; i++ ) {
		newstr[i] = lower ? tolower( newstr[i] ) : toupper( newstr[i] );
	}

	result.SetStringValue( newstr );
	return( true );
}

bool FunctionCall::
subString( const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value 	arg0, arg1, arg2;
	string	buf;
	int		offset, len=0, alen;

		// two or three arguments
	if( argList.size() < 2 || argList.size() > 3 ) {
		result.SetErrorValue( );
		return( false );
	}

		// Evaluate all arguments
	if( !argList[0]->Evaluate( state, arg0 ) ||
		!argList[1]->Evaluate( state, arg1 ) ||
		( argList.size( ) > 2 && !argList[2]->Evaluate( state, arg2 ) ) ) {
		result.SetErrorValue( );
		return( false );
	}

		// strict on undefined
	if( arg0.IsUndefinedValue( ) || arg1.IsUndefinedValue( ) ||
		(argList.size() > 2 && arg2.IsUndefinedValue( ) ) ) {
		result.SetUndefinedValue( );
		return( false );
	}

		// arg0 must be string, arg1 must be int, arg2 (if given) must be int
	if( !arg0.IsStringValue( buf ) || !arg1.IsIntegerValue( offset )||
		(argList.size( ) > 2 && !arg2.IsIntegerValue( len ) ) ) {
		result.SetErrorValue( );
		return( false );
	}

		// perl-like substr; negative offsets and lengths count from the end
		// of the string
	alen = buf.size( );
	if( offset < 0 ) { 
		offset = alen + offset; 
	} else if( offset >= alen ) {
		offset = alen;
	}
	if( len <= 0 ) {
		len = alen - offset + len;
	} else if( len > alen - offset ) {
		len = alen - offset;
	}

		// allocate storage for the string
	string str;

	str.assign( buf, offset, len );
	result.SetStringValue( str );
	return( true );
}

bool FunctionCall::
convInt( const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value	arg;
	string	buf;
	char	*end;
	int		ivalue;
	time_t	tvalue;
	bool	bvalue;
	double	rvalue;
	NumberFactor nf = NO_FACTOR;

		// takes exactly one argument
	if( argList.size() > 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

	switch( arg.GetType( ) ) {
		case UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return( true );

		case ERROR_VALUE:
		case CLASSAD_VALUE:
		case LIST_VALUE:
			result.SetErrorValue( );
			return( true );

		case STRING_VALUE:
			arg.IsStringValue( buf );
			ivalue = strtol( buf.c_str( ), (char**) &end, 0 );
			if( end == buf && ivalue == 0 ) {
				// strtol() returned an error
				result.SetErrorValue( );
				return( true );
			}
			switch( toupper( *end ) ) {
				case 'K': nf = K_FACTOR; break;
				case 'M': nf = M_FACTOR; break;
				case 'G': nf = G_FACTOR; break;
				case '\0': nf = NO_FACTOR; break;
				default:  
					result.SetErrorValue( );
					return( true );
			}
			result.SetIntegerValue( ivalue * nf );
			return( true );

		case BOOLEAN_VALUE:
			arg.IsBooleanValue( bvalue );
			result.SetIntegerValue( bvalue ? 1 : 0 );
			return( true );

		case INTEGER_VALUE:
			result.CopyFrom( arg );
			return( true );

		case REAL_VALUE:
			arg.IsRealValue( rvalue );
			result.SetIntegerValue( (int) rvalue );
			return( true );

		case ABSOLUTE_TIME_VALUE:
			arg.IsAbsoluteTimeValue( tvalue );
			result.SetIntegerValue( (int)tvalue );
			return( true );

		case RELATIVE_TIME_VALUE:
			arg.IsRelativeTimeValue( tvalue );
			result.SetIntegerValue( (int)tvalue );
			return( true );

		default:
			EXCEPT( "Should not reach here" );
	}
	return( false );
}


bool FunctionCall::
convReal( const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value	arg;
	string	buf;
	char	*end;
	int		ivalue;
	time_t	tvalue;
	bool	bvalue;
	double	rvalue;
	NumberFactor nf;

		// takes exactly one argument
	if( argList.size() > 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

	switch( arg.GetType( ) ) {
		case UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return( true );

		case ERROR_VALUE:
		case CLASSAD_VALUE:
		case LIST_VALUE:
			result.SetErrorValue( );
			return( true );

		case STRING_VALUE:
			arg.IsStringValue( buf );
			rvalue = strtod( buf.c_str( ), (char**) &end );
			if( end == buf && rvalue == 0.0 ) {
				// strtod() returned an error
				result.SetErrorValue( );
				return( true );
			}
			switch( toupper( *end ) ) {
				case 'K': nf = K_FACTOR; break;
				case 'M': nf = M_FACTOR; break;
				case 'G': nf = G_FACTOR; break;
				case '\0': nf = NO_FACTOR; break;
				default:
					result.SetErrorValue( );
					return( true );
			}
			result.SetRealValue( rvalue * nf );
			return( true );

		case BOOLEAN_VALUE:
			arg.IsBooleanValue( bvalue );
			result.SetRealValue( bvalue ? 1.0 : 0.0 );
			return( true );

		case INTEGER_VALUE:
			arg.IsIntegerValue( ivalue );
			result.SetRealValue( (double)ivalue );
			return( true );

		case REAL_VALUE:
			result.CopyFrom( arg );
			return( true );

		case ABSOLUTE_TIME_VALUE:
			arg.IsAbsoluteTimeValue( tvalue );
			result.SetRealValue( tvalue );
			return( true );

		case RELATIVE_TIME_VALUE:
			arg.IsRelativeTimeValue( tvalue );
			result.SetRealValue( tvalue );
			return( true );

		default:
			EXCEPT( "Should not reach here" );
	}
	return( false );
}

bool FunctionCall::
convString(const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value	arg;

		// takes exactly one argument
	if( argList.size() > 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

	switch( arg.GetType( ) ) {
		case UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return( true );

		case ERROR_VALUE:
		case CLASSAD_VALUE:
		case LIST_VALUE:
			result.SetErrorValue( );
			return( true );

		case STRING_VALUE:
			result.CopyFrom( arg );
			return( true );

		case INTEGER_VALUE:
		case REAL_VALUE:
		case BOOLEAN_VALUE:
			{
				ClassAdUnParser	unp;
				string			svalue;
				unp.Unparse( svalue, arg );
				result.SetStringValue( svalue );
				return( true );
			}

		case ABSOLUTE_TIME_VALUE:
		case RELATIVE_TIME_VALUE:
			{
				ClassAdUnParser	unp;
				string			svalue;
				unp.Unparse( svalue, arg );
				result.SetStringValue( svalue.substr(1,svalue.size()-2) );
				return( true );
			}

		default:
			EXCEPT( "Should not reach here" );
	}
	return( false );
}

bool FunctionCall::
convBool( const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value	arg;

		// takes exactly one argument
	if( argList.size() != 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

	switch( arg.GetType( ) ) {
		case UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return( true );

		case ERROR_VALUE:
		case CLASSAD_VALUE:
		case LIST_VALUE:
		case ABSOLUTE_TIME_VALUE:
			result.SetErrorValue( );
			return( true );

		case BOOLEAN_VALUE:
			result.CopyFrom( arg );
			return( true );

		case INTEGER_VALUE:
			{
				int ival;
				arg.IsIntegerValue( ival );
				result.SetBooleanValue( ival != 0 );
				return( true );
			}

		case REAL_VALUE:
			{
				double rval;
				arg.IsRealValue( rval );
				result.SetBooleanValue( rval != 0.0 );
				return( true );
			}

		case STRING_VALUE:
			{
				string buf;
				arg.IsStringValue( buf );
				if( strcasecmp( "false", buf.c_str( ) ) || buf == "" ) {
					result.SetBooleanValue( false );
				} else {
					result.SetBooleanValue( true );
				}
				return( true );
			}

		case RELATIVE_TIME_VALUE:
			{
				time_t rsecs;
				arg.IsRelativeTimeValue( rsecs );
				result.SetBooleanValue( rsecs != 0 );
				return( true );
			}

		default:
			EXCEPT( "Should not reach here" );
	}
	return( false );
}


bool FunctionCall::
convTime(const char* name,const ArgumentList &argList,EvalState &state,
	Value &result)
{
	Value	arg;
	bool	relative = ( strcasecmp( "reltime", name ) == 0 );

		// takes exactly one argument
	if( argList.size() > 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

	switch( arg.GetType( ) ) {
		case UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return( true );

		case ERROR_VALUE:
		case CLASSAD_VALUE:
		case LIST_VALUE:
		case BOOLEAN_VALUE:
			result.SetErrorValue( );
			return( true );

		case INTEGER_VALUE:
			{
				int ivalue;
				arg.IsIntegerValue( ivalue );
				if( relative ) {
					result.SetRelativeTimeValue( ivalue );
				} else {
					result.SetAbsoluteTimeValue( ivalue );
				}
				return( true );
			}

		case REAL_VALUE:
			{
				double	rvalue;

				arg.IsRealValue( rvalue );
				if( relative ) {
					result.SetRelativeTimeValue( (int)rvalue );
				} else {
					result.SetAbsoluteTimeValue( (int)rvalue );
				}
				return( true );
			}

		case STRING_VALUE:
			{
				string 	buf;
				time_t 	secs;
				arg.IsStringValue( buf );
				if( relative ) {
					if( !Lexer::tokenizeRelativeTime((char*)buf.c_str(),secs)) {
						result.SetErrorValue( );
						return( true );
					}
					result.SetRelativeTimeValue( secs );
				} else {
					if( !Lexer::tokenizeAbsoluteTime((char*)buf.c_str(),secs)) {
						result.SetErrorValue( );
						return( true );
					}
					result.SetAbsoluteTimeValue( secs );
				}
				return( true );
			}

		case ABSOLUTE_TIME_VALUE:
			{
				time_t secs;
				arg.IsAbsoluteTimeValue( secs );
				if( relative ) {
					result.SetRelativeTimeValue( secs );
				} else {
					result.SetAbsoluteTimeValue( secs );
				}
				return( true );
			}

		case RELATIVE_TIME_VALUE:
			{
				if( relative ) {
					result.CopyFrom( arg );
				} else {
					time_t secs;
					arg.IsRelativeTimeValue( secs );
					result.SetAbsoluteTimeValue( secs );
				}
				return( true );
			}

		default:
			EXCEPT( "Should not reach here" );
			return( false );
	}
}

bool FunctionCall::
doMath( const char* name,const ArgumentList &argList,EvalState &state,
	Value &result )
{
	Value	arg;

		// takes exactly one argument
	if( argList.size() > 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

	switch( arg.GetType( ) ) {
		case UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return( true );

		case ERROR_VALUE:
		case CLASSAD_VALUE:
		case LIST_VALUE:
		case STRING_VALUE:
		case ABSOLUTE_TIME_VALUE:
		case RELATIVE_TIME_VALUE:
		case BOOLEAN_VALUE:
			result.SetErrorValue( );
			return( true );

		case INTEGER_VALUE:
				// floor, ceil and round are identity ops for integers
			result.CopyFrom( arg );
			return( true );

		case REAL_VALUE:
			{
				double rvalue;
				arg.IsRealValue( rvalue );
				if( strcasecmp( "floor", name ) == 0 ) {
					result.SetRealValue( floor( rvalue ) );
				} else if( strcasecmp( "ceil", name ) == 0 ) {
					result.SetRealValue( ceil( rvalue ) );
				} else if( strcasecmp( "round", name ) == 0 ) {
					result.SetRealValue( rint( rvalue ) );
				} else {
					result.SetErrorValue( );
					return( true );
				}
				return( true );
			}
		
		default:
			EXCEPT( "Should not get here" );
			return( false );
	}
	return( false );
}

bool FunctionCall::
matchPattern( const char*,const ArgumentList &argList,EvalState &state,
	Value &result )
{
	Value 	arg0, arg1;
	char	*pattern=NULL, *target=NULL;
	regex_t	re;
	int		status;

		// need two arguments; first is pattern, second is string
	if( argList.size() != 2 ) {
		result.SetErrorValue( );
		return( true );
	}

		// Evaluate args
	if( !argList[0]->Evaluate( state, arg0 ) || 
		!argList[1]->Evaluate( state, arg1 ) ) {
		result.SetErrorValue( );
		return( false );
	}

		// if either arg is error, the result is error
	if( arg0.IsErrorValue( ) || arg1.IsErrorValue( ) ) {
		result.SetErrorValue( );
		return( true );
	}

		// if either arg is undefined, the result is undefined
	if( arg0.IsUndefinedValue( ) || arg1.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return( true );
	}

		// if either argument is not a string, the result is undefined
	if( !arg0.IsStringValue( pattern ) || !arg1.IsStringValue( target ) ) {
		result.SetErrorValue( );
		return( true );
	}

		// compile the patern
	if( regcomp( &re, pattern, REG_EXTENDED|REG_NOSUB ) != 0 ) {
			// error in pattern
		result.SetErrorValue( );
		return( true );
	}

		// test the match
	status = regexec( &re, target, (size_t)0, NULL, 0 );

		// dispose memory created by regcomp()
	regfree( &re );

		// check for success/failure
	if( status == 0 ) {
		result.SetBooleanValue( true );
		return( true );
	} else if( status == REG_NOMATCH ) {
		result.SetBooleanValue( false );
		return( true );
	} else {
			// some error; we could possibly return 'false' here ...
		result.SetErrorValue( );
		return( true );
	}
}

END_NAMESPACE // classad
