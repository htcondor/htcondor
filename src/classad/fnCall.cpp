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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "classad/common.h"
#include "classad/exprTree.h"
#include "classad/source.h"
#include "classad/sink.h"
#include "classad/util.h"
#include "classad/natural_cmp.h"

#ifdef WIN32
 #if _MSC_VER < 1900
 double rint(double rval) { return floor(rval + .5); }
 #endif
#else
#include <sys/time.h>
#endif

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#ifdef UNIX
#include <dlfcn.h>
#endif

using std::string;
using std::vector;
using std::pair;
using std::set;


namespace classad {

bool FunctionCall::initialized = false;

static bool doSplitTime(
    const Value &time, ClassAd * &splitClassAd);
static void absTimeToClassAd(
    const abstime_t &asecs, ClassAd * &splitClassAd);
static void relTimeToClassAd(
    double rsecs, ClassAd * &splitClassAd);
static void make_formatted_time(
    const struct tm &time_components, string &format, Value &result);
static bool
stringListsIntersect(const char*,const ArgumentList &argList,EvalState &state,Value &result);

// start up with an argument list of size 4
FunctionCall::
FunctionCall( )
{
	parentScope = NULL;

	function = NULL;

	if( !initialized ) {
		FuncTable &functionTable = getFunctionTable();

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
		functionTable["identicalmember"	] =	testMember;

		// Some list functions, useful for lists as sets
		functionTable["size"        ] = size;
		functionTable["sum"         ] = sumAvg;
		functionTable["avg"         ] = sumAvg;
		functionTable["min"         ] = minMax;
		functionTable["max"         ] = minMax;
		functionTable["anycompare"  ] = listCompare;
		functionTable["allcompare"  ] = listCompare;

			// time management
		functionTable["time"        ] = epochTime;
		functionTable["currenttime"	] =	currentTime;
		functionTable["timezoneoffset"] =timeZoneOffset;
		functionTable["daytime"		] =	dayTime;
		functionTable["getyear"		] =	getField;
		functionTable["getmonth"	] =	getField;
		functionTable["getdayofyear"] =	getField;
		functionTable["getdayofmonth"] =getField;
		functionTable["getdayofweek"] =	getField;
		functionTable["getdays"		] =	getField;
		functionTable["gethours"	] =	getField;
		functionTable["getminutes"	] =	getField;
		functionTable["getseconds"	] =	getField;
		functionTable["splittime"   ] = splitTime;
		functionTable["formattime"  ] = formatTime;

			// string manipulation
		functionTable["strcat"		] =	strCat;
		functionTable["join"		] =	strCat;
		functionTable["toupper"		] =	changeCase;
		functionTable["tolower"		] =	changeCase;
		functionTable["substr"		] =	subString;
		functionTable["strcmp"      ] = compareString;
		functionTable["stricmp"     ] = compareString;

			// version comparison
		functionTable["versioncmp"  ] = compareVersion;
		functionTable["versionLE"   ] = compareVersion;
		functionTable["versionLT"   ] = compareVersion;
		functionTable["versionGE"   ] = compareVersion;
		functionTable["versionGT"   ] = compareVersion;
		// Not identical to str1 =?= str2 because it won't eat undefined.
		functionTable["versionEQ"   ] = compareVersion;
		functionTable["version_in_range"] = versionInRange;

			// pattern matching (regular expressions)
		functionTable["regexp"		] =	matchPattern;
		functionTable["regexpmember"] =	matchPatternMember;
		functionTable["regexps"     ] = substPattern;
		functionTable["replace"     ] = substPattern;
		functionTable["replaceall"  ] = substPattern;

			// conversion functions
		functionTable["int"			] =	convInt;
		functionTable["real"		] =	convReal;
		functionTable["string"		] =	convString;
		functionTable["bool"		] =	convBool;
		functionTable["absTime"		] =	convTime;
		functionTable["relTime"		] = convTime;

		// turn the contents of an expression into a string
		// but *do not* evaluate it
		functionTable["unparse"		] =	unparse;
		functionTable["unresolved"	] = hasRefs;

			// mathematical functions
		functionTable["floor"		] =	doRound;
		functionTable["ceil"		] =	doRound;
		functionTable["ceiling"		] =	doRound;
		functionTable["round"		] =	doRound;
		functionTable["pow" 		] =	doMath2;
		//functionTable["log" 		] =	doMath2;
		functionTable["quantize"	] =	doMath2;
		functionTable["random"      ] = random;

			// for compatibility with old classads:
		functionTable["ifThenElse"  ] = ifThenElse;
		functionTable["interval" ] = interval;
		functionTable["eval"] = eval;

			// string list functions:
			// Note that many other string list functions are defined
			// externally in the Condor classad compatibility layer.
		functionTable["stringListsIntersect" ] = stringListsIntersect;
		functionTable["debug"      ] = debug;

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

FunctionCall::
FunctionCall(FunctionCall &functioncall)
{
    CopyFrom(functioncall);
    return;
}

FunctionCall & FunctionCall::
operator=(FunctionCall &functioncall)
{
    if (this != &functioncall) {
        CopyFrom(functioncall);
    }
    return *this;
}

ExprTree *FunctionCall::
Copy( ) const
{
	FunctionCall *newTree = new FunctionCall;

	if (!newTree) return NULL;

    if (!newTree->CopyFrom(*this)) {
        delete newTree;
        newTree = NULL;
    }
	return newTree;
}

bool FunctionCall::
CopyFrom(const FunctionCall &functioncall)
{
    bool     success;
	ExprTree *newArg;

    success      = true;
	parentScope = functioncall.parentScope;

    ExprTree::CopyFrom(functioncall);
    functionName = functioncall.functionName;
	function     = functioncall.function;

	for (ArgumentList::const_iterator i = functioncall.arguments.begin(); 
         i != functioncall.arguments.end();
         i++ ) {
		newArg = (*i)->Copy( );
		if( newArg ) {
			arguments.push_back( newArg );
		} else {
            success = false;
            break;
		}
	}
    return success;
}

bool FunctionCall::
SameAs(const ExprTree *tree) const
{
    bool is_same;
    const FunctionCall *other_fn;
    const ExprTree * pSelfTree = tree->self();
    
    if (this == pSelfTree) {
        is_same = true;
    } else if (pSelfTree->GetKind() != FN_CALL_NODE) {
        is_same = false;
    } else {
        other_fn = (const FunctionCall *) pSelfTree;
        
        if (functionName == other_fn->functionName
            && function == other_fn->function
            && arguments.size() == other_fn->arguments.size()) {
            
            is_same = true;
            ArgumentList::const_iterator a1 = arguments.begin();
            ArgumentList::const_iterator a2 = other_fn->arguments.begin();
            while (a1 != arguments.end()) {
                if (a2 == other_fn->arguments.end()) {
                    is_same = false;
                    break;
                } else if (!(*a1)->SameAs((*a2))) {
                    is_same = false;
                    break;
                }
				a1++;
				a2++;
            }
        } else {
            is_same = false;
        }
    }

    return is_same;
}

bool operator==(const FunctionCall &fn1, const FunctionCall &fn2)
{
    return fn1.SameAs(&fn2);
}

FunctionCall::FuncTable& FunctionCall::
getFunctionTable(void)
{
    static FuncTable functionTable;
    return functionTable;
}

void FunctionCall::RegisterFunction(
	string &functionName, 
	ClassAdFunc function)
{
    FuncTable &functionTable = getFunctionTable();

	if (functionTable.find(functionName) == functionTable.end()) {
		functionTable[functionName] = function;
	}
	return;
}

void FunctionCall::RegisterFunctions(
	ClassAdFunctionMapping *functions)
{
	if (functions != NULL) {
		while (functions->function != NULL) {
			RegisterFunction(functions->functionName, 
							 functions->function);
			functions++;
		}
	}
	return;
}

bool FunctionCall::RegisterSharedLibraryFunctions(
	const char *shared_library_path)
{
#ifdef UNIX
	bool success;
	void *dynamic_library_handle;
		
	success = false;
	if (shared_library_path) {
		// We use "deep bind" here to make sure the library uses its own version of the
		// symbol in case of duplicates.
		//
		// In particular, we were observing crashes in condor_shadow (which statically
		// links libcondor_utils) and libclassad_python_user.so (which dynamically links
		// libcondor_utils).  This causes the finalizers for global objects to be called
		// twice - once for the dynamic version (which bound against the global one) and
		// again at exit().
		//
		// With DEEPBIND, the finalizer for the dynamic library points at its own copy
		// of the global and doesn't touch the shadow's.  See #4998
		//
		int flags = RTLD_LAZY;
#ifdef LINUX
		flags |= RTLD_DEEPBIND;
#endif
		dynamic_library_handle = dlopen(shared_library_path, flags);
		if (dynamic_library_handle) {
			ClassAdSharedLibraryInit init_function;

			init_function = (ClassAdSharedLibraryInit) dlsym(dynamic_library_handle, 
															 "Init");
			if (init_function != NULL) {
				ClassAdFunctionMapping *functions;
				
				functions = init_function();
				if (functions != NULL) {
					RegisterFunctions(functions);
					success = true;
					/*
					while (functions->apparentFunctionName != NULL) {
						ClassAdFunc function;
						string functionName = functions->apparentFunctionName;
						function = dlsym(dynamic_library_handle, 
										 functions->actualFunctionName);
						RegisterFunction(functionName,
										 function);
						success = true;
						functions++;
					}
					*/
				} else {
					CondorErrno  = ERR_CANT_LOAD_DYNAMIC_LIBRARY;
					CondorErrMsg = "Init function returned NULL.";
				}
			} else {
				CondorErrno  = ERR_CANT_LOAD_DYNAMIC_LIBRARY;
				CondorErrMsg = "Couldn't find Init() function.";
			}
		} else {
			CondorErrno  = ERR_CANT_LOAD_DYNAMIC_LIBRARY;
			CondorErrMsg = "Couldn't open shared library with dlopen.";
		}
	} else {
		CondorErrno  = ERR_CANT_LOAD_DYNAMIC_LIBRARY;
		CondorErrMsg = "No shared library was specified.";
	}

	return success;
#else /* end UNIX */
    CondorErrno = ERR_CANT_LOAD_DYNAMIC_LIBRARY;
    CondorErrMsg = "Shared library support not available.";
    return false;
#endif /* end !UNIX */
}

void FunctionCall::
_SetParentScope( const ClassAd* parent )
{
	parentScope = parent;

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
		while(i != args.end()) {
			delete *i;
			i++;
		}
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( NULL );
	}

    FuncTable &functionTable = getFunctionTable();
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

#ifdef TJ_PICKLE

FunctionCall * FunctionCall::Make(ExprStream & stm)
{
	ExprStream stm2;
	FunctionCall *fc = new FunctionCall;
	if ( ! fc) return NULL;

	unsigned int argc = 0;
	unsigned char ct = 0;
	if ( ! stm.readByte(ct)) { goto bail; }

	if (ct == ExprStream::EsCall) {
		unsigned char count = 0;
		if ( ! stm.readSmallString(fc->functionName) || ! stm.readByte(count)) { goto bail; }
		argc = count;
	} else if (ct == ExprStream::EsBigCall) {
		if ( ! stm.readString(fc->functionName) || ! stm.readInteger(argc)) { goto bail; }
	} else {
		goto bail;
	}

	fc->arguments.resize(argc);
	for (unsigned int ii = 0; ii < argc; ++ii) {
		if ( ! stm.readNullableExpr(fc->arguments[ii]))
			goto bail;
	}

	// fill in function
	{
		FuncTable &functionTable = getFunctionTable();
		FuncTable::iterator found = functionTable.find(fc->functionName);
		if (found != functionTable.end( )) {
			fc->function = (ClassAdFunc)found->second;
		} else {
			fc->function = NULL;
		}
	}

	return fc;
bail:
	delete fc;
	return NULL;
}

/*static*/ unsigned int FunctionCall::Scan(ExprStream & stm)
{
	ExprStream::Mark mk = stm.mark();

	unsigned int argc = 0;
	unsigned char ct = 0;
	if ( ! stm.readByte(ct)) { goto bail; }

	if (ct == ExprStream::EsCall) {
		unsigned char count = 0;
		if ( ! stm.skipSmall() || ! stm.readByte(count)) { goto bail; }
		argc = count;
	} else if (ct == ExprStream::EsBigCall) {
		if ( ! stm.skipStream() || ! stm.readInteger(argc)) { goto bail; }
	} else {
		goto bail;
	}

	for (unsigned int ii = 0; ii < argc; ++ii) {
		ExprTree::NodeKind kind;
		int err = 0;
		ExprTree::Scan(stm, kind, true, &err);
		if (err) goto bail;
	}

	return stm.size(mk);
bail:
	stm.unwind(mk);
	return 0;
}


unsigned int FunctionCall::Pickle(ExprStreamMaker & stm) const
{
	ExprStreamMaker::Mark mk = stm.mark();

	unsigned int len = (unsigned int)functionName.length();
	unsigned int argc = (unsigned int)arguments.size();
	if (len > 255 || argc > 255) {
		stm.putByte(ExprStream::EsBigCall);
		stm.putString(functionName);
		stm.putInteger(argc);
	} else {
		stm.putByte(ExprStream::EsCall);
		stm.putSmallString(functionName);
		stm.putByte((unsigned char)argc);
	}

	for (unsigned int ii = 0; ii < argc; ++ii) {
		stm.putNullableExpr(arguments[ii]);
	}
	return stm.added(mk);
}

#endif


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
		delete tmpSig;
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
_Flatten( EvalState &state, Value &value, ExprTree*&tree, int* ) const
{
	FunctionCall *newCall;
	ExprTree	*argTree;
	Value		argValue;
	bool		fold = true;

	tree = NULL; // Just to be safe...  wenger 2003-12-11.

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
	newCall->function = function;

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
			delete newCall;
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
	const ExprList	*el = nullptr;
	bool			b;
	bool			useIS = ( strcasecmp( "identicalmember", name ) == 0 );

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

		// if the second arg (a list) is undefined, or the first arg is
		// undefined and we're supposed to test for strict comparison, the 
		// result is 'undefined'
    if( arg1.IsUndefinedValue() || ( !useIS && arg0.IsUndefinedValue() ) ) {
        val.SetUndefinedValue();
        return true;
    }

#ifdef FLEXIBLE_MEMBER
    if (arg0.IsListValue() && !arg1.IsListValue()) {
        Value swap;

        swap.CopyFrom(arg0);
        arg0.CopyFrom(arg1);
        arg1.CopyFrom(swap);
    }
#endif

		// arg1 must be a list; arg0 must be comparable
    if( !arg1.IsListValue() || arg0.IsListValue() || arg0.IsClassAdValue() ) {
        val.SetErrorValue();
        return true;
    }

		// if we're using strict comparison, arg0 can't be 'error'
	if( !useIS && arg0.IsErrorValue( ) ) {
		val.SetErrorValue( );
		return( true );
	}

    // check for membership
	arg1.IsListValue( el );
	for (const auto *tree: *el) {
		if( !tree->Evaluate( state, cArg ) ) {
			val.SetErrorValue( );
			return( false );
		}
		Operation::Operate(useIS ? Operation::IS_OP : Operation::EQUAL_OP, 
			cArg, arg0, val );
		if( val.IsBooleanValue( b ) && b ) {
			return true;
		}
	}
	val.SetBooleanValue( false );	

    return true;
}

// The size of a list, ClassAd, etc. 
bool FunctionCall::
size(const char *, const ArgumentList &argList, 
	 EvalState &state, Value &val)
{
	Value             arg;
	const ExprList    *listToSize;
    ClassAd           *classadToSize;
	int			      length;

	// we accept only one argument
	if (argList.size() != 1) {
		val.SetErrorValue();
		return( true );
	}
	
	if (!argList[0]->Evaluate(state, arg)) {
		val.SetErrorValue();
		return false;
	} else if (arg.IsUndefinedValue()) {
		val.SetUndefinedValue();
		return true;
	} else if (arg.IsListValue(listToSize)) {
        length = listToSize->size();
        val.SetIntegerValue(length);
        return true;
	} else if (arg.IsClassAdValue(classadToSize)) {
        length = classadToSize->size();
        val.SetIntegerValue(length);
        return true;
    } else if (arg.IsStringValue(length)) {
        val.SetIntegerValue(length);
        return true;
    } else {
        val.SetErrorValue();
        return true;
    }
}

bool FunctionCall::
sumAvg(const char *name, const ArgumentList &argList, 
	   EvalState &state, Value &val)
{
	Value             listElementValue, listVal;
	Value             numElements, result;
	const ExprList    *listToSum;
	bool		      first;
	int			      len;
	bool              onlySum = (strcasecmp("sum", name) == 0 );

	// we accept only one argument
	if (argList.size() != 1) {
		val.SetErrorValue();
		return( true );
	}
	
	// argument must Evaluate to a list
	if (!argList[0]->Evaluate(state, listVal)) {
		val.SetErrorValue();
		return false;
	} else if (listVal.IsUndefinedValue()) {
		val.SetUndefinedValue();
		return true;
	} else if (!listVal.IsListValue((const ExprList *&)listToSum)) {
		val.SetErrorValue();
		return( true );
	}

	onlySum = (strcasecmp("sum", name) == 0 );
	result.SetIntegerValue(0); // sum({}) should be 0
	len = 0;
	first = true;

	// Walk over each element in the list, and sum.
	for (const auto *listElement: *listToSum) {
		if (listElement != nullptr) {
			len++;
			// Make sure this element is a number.
			if (!listElement->Evaluate(state, listElementValue)) {
				val.SetErrorValue();
				return false;
			} else if (listElementValue.IsUndefinedValue()) {
				--len;
				continue;
			} else if (!listElementValue.IsNumber()) {
				val.SetErrorValue();
				return true;
			}

			// Either take the number if it's the first, 
			// or add to the running sum.
			if (first) {
				result.CopyFrom(listElementValue);
				first = false;
			} else {
				Operation::Operate(Operation::ADDITION_OP, result, 
								   listElementValue, result);
			}
		}
	}

    // if the sum() function was called, we don't need to find the average
    if (onlySum) {
		val.CopyFrom(result);
		return true;
	}

	if (len > 0) {
		numElements.SetRealValue(len);
		Operation::Operate(Operation::DIVISION_OP, result, 
						   numElements, result);
	} else {
		val.SetUndefinedValue();
	}

	val.CopyFrom( result );
	return true;
}


bool FunctionCall::
minMax(const char *fn, const ArgumentList &argList, 
	   EvalState &state, Value &val)
{
	Value		       listElementValue, listVal, cmp;
	Value              result;
	const ExprList     *listToBound;
    bool		       first = true, b = false;
	Operation::OpKind  comparisonOperator;

	// we accept only one argument
	if (argList.size() != 1) {
		val.SetErrorValue();
		return true;
	}

	// first argument must Evaluate to a list
	if(!argList[0]->Evaluate(state, listVal)) {
		val.SetErrorValue();
		return false;
	} else if (listVal.IsUndefinedValue()) {
		val.SetUndefinedValue();
		return true;
	} else if (!listVal.IsListValue((const ExprList *&)listToBound)) {
		val.SetErrorValue();
		return true;
	}

	// fn is either "min..." or "max..."
	if (tolower(fn[1]) == 'i') {
		comparisonOperator = Operation::LESS_THAN_OP;
	} else {
		comparisonOperator = Operation::GREATER_THAN_OP;
	}

	result.SetUndefinedValue();

	// Walk over the list, calculating the bound the whole way.
	for (const auto *listElement: *listToBound) {
		if (listElement != nullptr) {

			// For this element of the list, make sure it is 
			// acceptable.
			if(!listElement->Evaluate(state, listElementValue)) {
				val.SetErrorValue();
				return false;
			} else if (listElementValue.IsUndefinedValue()) {
				continue;
			} else if (!listElementValue.IsNumber()) {
				val.SetErrorValue();
				return true;
			}
			
			// If it's the first element, copy it to the bound,
			// otherwise compare to decide what to do.
			if (first) {
				result.CopyFrom(listElementValue);
				first = false;
			} else {
				Operation::Operate(comparisonOperator, listElementValue, 
								   result, cmp);
				if (cmp.IsBooleanValue(b) && b) {
					result.CopyFrom(listElementValue);
				}
			}
		}
	}

	val.CopyFrom(result);
	return true;
}

bool FunctionCall::
listCompare(
	const char         *fn, 
	const ArgumentList &argList, 
	EvalState          &state, 
	Value              &val)
{
	Value		       listElementValue, listVal, compareVal;
	Value              stringValue;
	const ExprList     *listToCompare;
    bool		       needAllMatch;
	string             comparison_string;
	Operation::OpKind  comparisonOperator;

	// We take three arguments:
	// The operator to use, as a string.
	// The list
	// The thing we are comparing against.
	if (argList.size() != 3) {
		val.SetErrorValue();
		return true;
	}

	// The first argument must be a string
	if (!argList[0]->Evaluate(state, stringValue)) {
		val.SetErrorValue();
		return false;
	} else if (stringValue.IsUndefinedValue()) {
		val.SetUndefinedValue();
		return true;
	} else if (!stringValue.IsStringValue(comparison_string)) {
		val.SetErrorValue();
		return true;
	}
	
	// Decide which comparison to do, or give an error
	if (comparison_string == "<") {
		comparisonOperator = Operation::LESS_THAN_OP;
	} else if (comparison_string == "<=") {
		comparisonOperator = Operation::LESS_OR_EQUAL_OP;
	} else if (comparison_string == "!=") {
		comparisonOperator = Operation::NOT_EQUAL_OP;
	} else if (comparison_string == "==") {
		comparisonOperator = Operation::EQUAL_OP;
	} else if (comparison_string == ">") {
		comparisonOperator = Operation::GREATER_THAN_OP;
	} else if (comparison_string == ">=") {
		comparisonOperator = Operation::GREATER_OR_EQUAL_OP;
	} else if (comparison_string == "is") {
		comparisonOperator = Operation::META_EQUAL_OP;
	} else if (comparison_string == "isnt") {
		comparisonOperator = Operation::META_NOT_EQUAL_OP;
	} else {
		val.SetErrorValue();
		return true;
	}

	// The second argument must Evaluate to a list
	if (!argList[1]->Evaluate(state, listVal)) {
		val.SetErrorValue();
		return false;
	} else if (listVal.IsUndefinedValue()) {
		val.SetUndefinedValue();
		return true;
	} else if (!listVal.IsListValue((const ExprList *&)listToCompare)) {
		val.SetErrorValue();
		return true;
	}

	// The third argument is something to compare against.
	if (!argList[2]->Evaluate(state, compareVal)) {
		val.SetErrorValue();
		return false;
	}

	// Finally, we decide what to do exactly, based on our name.
	if (!strcasecmp(fn, "anycompare")) {
		needAllMatch = false;
		val.SetBooleanValue(false);
	} else {
		needAllMatch = true;
		val.SetBooleanValue(true);
	}

	// Walk over the list
	for (const auto *listElement: *listToCompare) {
		if (listElement != NULL) {

			// For this element of the list, make sure it is 
			// acceptable.
			if(!listElement->Evaluate(state, listElementValue)) {
				val.SetErrorValue();
				return false;
			} else {
				Value  compareResult;
				bool   b;

				Operation::Operate(comparisonOperator, listElementValue,
								   compareVal, compareResult);
				if (!compareResult.IsBooleanValue(b)) {
					if (compareResult.IsUndefinedValue()) {
						if (needAllMatch) {
							val.SetBooleanValue(false);
							return true;
						}
					} else {
						val.SetErrorValue();
						return true;
					}
					return true;
				} else if (b) {
					if (!needAllMatch) {
						val.SetBooleanValue(true);
						return true;
					}
				} else {
					if (needAllMatch) {
						// we failed, because it didn't match
						val.SetBooleanValue(false);
						return true;
					}
				}
			}
		}
	}

	if (needAllMatch) {
		// They must have all matched, because nothing failed,
		// which would have returned.
		val.SetBooleanValue(true);
	} else {
		// Nothing must have matched, since we would have already
		// returned.
		val.SetBooleanValue(false);
	}

	return true;
}

bool FunctionCall::
epochTime (const char *, const ArgumentList &argList, EvalState &, Value &val)
{

		// no arguments
	if( argList.size( ) > 0 ) {
		val.SetErrorValue( );
		return( true );
	}

    val.SetIntegerValue(time(NULL));
	return( true );
}

bool FunctionCall::
currentTime (const char *, const ArgumentList &argList, EvalState &, Value &val)
{

		// no arguments
	if( argList.size( ) > 0 ) {
		val.SetErrorValue( );
		return( true );
	}

	Literal *time_literal = Literal::MakeAbsTime(NULL);
    if (time_literal != NULL) {
        time_literal->GetValue(val);
        delete time_literal;
        return true;
    } else {
        return false;
    }
}


bool FunctionCall::
timeZoneOffset (const char *, const ArgumentList &argList, EvalState &, 
	Value &val)
{
		// no arguments
	if( argList.size( ) > 0 ) {
		val.SetErrorValue( );
		return( true );
	}
	
	val.SetRelativeTimeValue( (time_t) timezone_offset( time(NULL), false ) );
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

	getLocalTime(&now, &lt);

	val.SetRelativeTimeValue((time_t) ( lt.tm_hour*3600 + lt.tm_min*60 + lt.tm_sec) );
	return( true );
}

bool FunctionCall::
getField(const char* name, const ArgumentList &argList, EvalState &state, 
	Value &val )
{
	Value 	arg;
	abstime_t asecs;
	time_t rsecs;
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

	if( arg.IsAbsoluteTimeValue( asecs ) ) {
	 	clock = asecs.secs;
		getLocalTime( &clock, &tms );
		if( strcasecmp( name, "getyear" ) == 0 ) {
			 // tm_year is years since 1900 --- make it y2k compliant :-)
			val.SetIntegerValue( tms.tm_year + 1900 );
		} else if( strcasecmp( name, "getmonth" ) == 0 ) {
			val.SetIntegerValue( tms.tm_mon + 1 );
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
			CLASSAD_EXCEPT( "Should not reach here" );
			val.SetErrorValue( );
			return( false );
		}
		return( true );
	} else if( arg.IsRelativeTimeValue( rsecs ) ) {
		if( strcasecmp( name, "getyear" ) == 0  	||
			strcasecmp( name, "getmonth" ) == 0  	||
			strcasecmp( name, "getdayofmonth" )== 0 ||
			strcasecmp( name, "getdayofweek" ) == 0 ||
			strcasecmp( name, "getdayofyear" ) == 0 ) {
				// not meaningful for reltimes
			val.SetErrorValue( );
			return( true );
		} else if( strcasecmp( name, "getdays" ) == 0 ) {
			val.SetIntegerValue( rsecs / 86400 );
		} else if( strcasecmp( name, "gethours" ) == 0 ) {
			val.SetIntegerValue( (rsecs % 86400 ) / 3600 );
		} else if( strcasecmp( name, "getminutes" ) == 0 ) {
			val.SetIntegerValue( ( rsecs % 3600 ) / 60 );
		} else if( strcasecmp( name, "getseconds" ) == 0 ) {
			val.SetIntegerValue( rsecs % 60 );
		} else {
			CLASSAD_EXCEPT( "Should not reach here" );
			val.SetErrorValue( );
			return( false );
		}
		return( true );
	}

	val.SetErrorValue( );
	return( true );
}

bool FunctionCall::
splitTime(const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value 	arg;
	ClassAd *split = nullptr;

	if( argList.size( ) != 1 ) {
		result.SetErrorValue( );
		return true;
	}

	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return false;	
	}

	if (arg.IsUndefinedValue()) {
		result.SetUndefinedValue();
	} else if (!arg.IsClassAdValue() && doSplitTime(arg, split)) {
		state.AddToDeletionCache(split);
		result.SetClassAdValue(split);
	} else {
		result.SetErrorValue();
	}
	return true;
}

bool FunctionCall::
formatTime(const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value 	   time_arg;
    Value      format_arg;
    time_t     epoch_time;
	struct  tm time_components;
    ClassAd    *splitClassAd;
    string     format;
    size_t     number_of_args;
    bool       did_eval;

    memset(&time_components, 0, sizeof(time_components));

    did_eval = true;
    number_of_args = argList.size();
    if (number_of_args == 0) {
        time(&epoch_time);
        getLocalTime(&epoch_time, &time_components);
        format = "%c";
        make_formatted_time(time_components, format, result);
    } else if (number_of_args < 3) {
        // The first argument should be our time and should
        // not be a relative time.
        if (!argList[0]->Evaluate(state, time_arg)) {
            did_eval = false;
        } else if (time_arg.IsRelativeTimeValue()) {
            result.SetErrorValue();
        } else if (!doSplitTime(time_arg, splitClassAd)) {
            result.SetErrorValue();
        } else {
            if (!splitClassAd->EvaluateAttrInt("Seconds", time_components.tm_sec)) {
                time_components.tm_sec = 0;
            }
            if (!splitClassAd->EvaluateAttrInt("Minutes", time_components.tm_min)) {
                time_components.tm_min = 0;
            }
            if (!splitClassAd->EvaluateAttrInt("Hours", time_components.tm_hour)) {
                time_components.tm_hour = 0;
            }
            if (!splitClassAd->EvaluateAttrInt("Day", time_components.tm_mday)) {
                time_components.tm_mday = 0;
            }
            if (!splitClassAd->EvaluateAttrInt("Month", time_components.tm_mon)) {
                time_components.tm_mon = 0;
            } else {
                time_components.tm_mon--;
            }
            if (!splitClassAd->EvaluateAttrInt("Year", time_components.tm_year)) {
                time_components.tm_year = 0;
            } else {
                time_components.tm_year -= 1900;;
            }

            // Note that we are doing our own calculations to get the weekday and year day.
            // Why are we doing our own calculations instead of using something like gmtime or 
            // localtime? That's because we are dealing with something that might not be in 
            // our timezone or GM time, but some other timezone.
            day_numbers(time_components.tm_year + 1900, time_components.tm_mon+1, time_components.tm_mday,
                        time_components.tm_wday, time_components.tm_yday);

            // The second argument, if provided, must be a string
            if (number_of_args == 1) {
                format = "%c";
                make_formatted_time(time_components, format, result);
            } else {
                if (!argList[1]->Evaluate(state, format_arg)) {
                    did_eval = false;
                } else {
                    if (!format_arg.IsStringValue(format)) {
                        result.SetErrorValue();
                    } else {
                        make_formatted_time(time_components, format, result);
                    }
                }
            }
            delete splitClassAd;
        }
    } else {
        result.SetErrorValue();
    }

    if (!did_eval) {
        result.SetErrorValue();
    }
    return did_eval;
}

	// concatenate all arguments (expected to be strings)
bool FunctionCall::
strCat( const char* name, const ArgumentList &argListIn, EvalState &state,
	Value &result )
{
	ClassAdUnParser	unp;
	string			buf, s, sep;
	bool			errorFlag=false, undefFlag=false, defFlag=false, rval=true;
	bool			is_join = (0 == strcasecmp( name, "join" ));
	int				num_args = (int)argListIn.size();
	ArgumentList	argTemp; // in case we need it
	Value			listVal; // in case we need it
	Literal *		sepLit=NULL; // in case we need it

	const ArgumentList * args = &argListIn;

	// join has a special case for 1 or 2 args when the last argument is a list.
	// for 1 arg, join the list items, for 2 args, join arg1 with arg0 as the separator
	if (is_join && num_args > 0 && num_args <= 2) {
		if( state.depth_remaining <= 0 ) {
			result.SetErrorValue();
			return false;
		}
		state.depth_remaining--;

		rval = argListIn[num_args-1]->Evaluate(state, listVal);
		state.depth_remaining++;
		if (rval) {
			ExprList *listToJoin;
			if (listVal.IsListValue(listToJoin)) {
				// cons up a new temporary argument list using the contents of the list
				// and the (optional) separator arg.
				listToJoin->GetComponents(argTemp);
				if (num_args > 1) {
					argTemp.insert(argTemp.begin(), argListIn[0]);
				} else {
					sepLit = Literal::MakeString("");
					argTemp.insert(argTemp.begin(), sepLit);
				}
				// now use this as the input arguments
				args = &argTemp;
				num_args = (int)argTemp.size();
			}
		}
	}

	for( int i = 0 ; (unsigned)i < args->size() ; i++ ) {
		Value  val;
		Value  stringVal;

		s = "";

		if( state.depth_remaining <= 0 ) {
			result.SetErrorValue();
			return false;
		}
		state.depth_remaining--;

		if( !( rval = (*args)[i]->Evaluate( state, val ) ) ) {
			break;
		}
		state.depth_remaining++;

		if (!val.IsStringValue(s)) {
			convertValueToStringValue(val, stringVal);
			if (stringVal.IsUndefinedValue()) {
				undefFlag = true;
				if (is_join) { continue; }  // join keeps going
				defFlag = false;
				break; // strcat returns undefined
			} else if (stringVal.IsErrorValue()) {
				errorFlag = true;
				result.SetErrorValue();
				break;
			} else if (!stringVal.IsStringValue(s)) {
				errorFlag = true;
				break;
			}
		}
		// if we get here, s is the string value of the item
		if (is_join) {
			if (0 == i) {
				sep = s;
				continue;
			}
			if ( ! buf.empty()) {
				buf += sep;
			}
		}
		defFlag = true;
		buf += s;
	}

	// clean up the temporary arglist
	while (argTemp.size() > 0) { argTemp.pop_back(); }
	delete sepLit; sepLit = NULL;

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
	// some argument was undefined and no arguments were defined
	if( undefFlag && ! defFlag ) {
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
	Value 		val, stringVal;
	string		str;
	bool		lower = ( strcasecmp( name, "tolower" ) == 0 );
	int			len;

    // only one argument 
	if( argList.size() != 1 ) {
		result.SetErrorValue( );
		return true;
	}

    // check for evaluation failure
	if( !argList[0]->Evaluate( state, val ) ) {
		result.SetErrorValue( );
		return false;
	}

    if (!val.IsStringValue(str)) {
        convertValueToStringValue(val, stringVal);
        if (stringVal.IsUndefinedValue()) {
            result.SetUndefinedValue( );
            return true;
        } else if (stringVal.IsErrorValue()) {
            result.SetErrorValue();
            return true;
        } else if (!stringVal.IsStringValue(str)) {
            result.SetErrorValue();
            return true;
        }
	}

	len = (int)str.size( );
	for( int i=0; i <= len; i++ ) {
		str[i] = lower ? tolower( str[i] ) : toupper( str[i] );
	}

	result.SetStringValue( str );
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
		return( true );
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
		return( true );
	}

		// arg0 must be string, arg1 must be int, arg2 (if given) must be int
	if( !arg0.IsStringValue( buf ) || !arg1.IsIntegerValue( offset )||
		(argList.size( ) > 2 && !arg2.IsIntegerValue( len ) ) ) {
		result.SetErrorValue( );
		return( true );
	}

		// perl-like substr; negative offsets and lengths count from the end
		// of the string
	alen = (int)buf.size( );
	if( offset < 0 ) { 
		offset = alen + offset; 
		if( offset < 0 ) {
			offset = 0;
		}
	} else if( offset >= alen ) {
		offset = alen;
	}
	if( len <= 0 ) {
		len = alen - offset + len;
		if( len < 0 ) {
			len = 0;
		}
	} else if( len > alen - offset ) {
		len = alen - offset;
	}

	// to make sure that if length is specified as 0 explicitly
	// then, len is set to 0
	if(argList.size( ) == 3) {
	  int templen;
	  arg2.IsIntegerValue( templen );
	  if(templen == 0)
	    len = 0;
	}

		// allocate storage for the string
	string str;

	str.assign( buf, offset, len );
	result.SetStringValue( str );
	return( true );
}

bool FunctionCall::
versionInRange( const char * name, const ArgumentList & argList,
  EvalState & state, Value & result ) {
	if( argList.size() != 3 ) {
		result.SetErrorValue();
		return true;
	}

	Value test, min, max;
	if(!argList[0]->Evaluate(state, test) ||
	   !argList[1]->Evaluate(state, min) ||
	   !argList[2]->Evaluate(state, max)) {
		result.SetErrorValue();
		return false;
	}

	// It seems like the version*() functions are more useful this way.
	if(min.IsUndefinedValue() || max.IsUndefinedValue()) {
		result.SetUndefinedValue();
		return true;
	}

	Value cTest, cMin, cMax;
	std::string sTest, sMin, sMax;
	if(  convertValueToStringValue( test, cTest )
	  && convertValueToStringValue( min, cMin )
	  && convertValueToStringValue( max, cMax )
	  && cTest.IsStringValue(sTest)
	  && cMin.IsStringValue(sMin)
	  && cMax.IsStringValue(sMax) ) {
		int l = natural_cmp( sMin.c_str(), sTest.c_str() );
		int r = natural_cmp( sTest.c_str(), sMax.c_str() );

		result.SetBooleanValue( l <= 0 && r <= 0 );
		return true;
	} else {
		result.SetErrorValue();
		return true;
	}

	return true;
}

bool FunctionCall::
compareVersion( const char * name, const ArgumentList & argList,
  EvalState & state, Value & result ) {
	if( argList.size() != 2 ) {
		result.SetErrorValue();
		return true;
	}

	Value 	left, right;
	if(!argList[0]->Evaluate(state, left) ||
	   !argList[1]->Evaluate(state, right)) {
		result.SetErrorValue();
		return false;
	}

	// It seems like the version*() functions are more useful this way.
	if(left.IsUndefinedValue() || right.IsUndefinedValue()) {
		result.SetUndefinedValue();
		return true;
	}

	Value cLeft, cRight;
	std::string sLeft, sRight;
	if(  convertValueToStringValue( left, cLeft )
	  && convertValueToStringValue( right, cRight )
	  && cLeft.IsStringValue(sLeft)
	  && cRight.IsStringValue(sRight) ) {
		int r = natural_cmp( sLeft.c_str(), sRight.c_str() );
		if( 0 == strcasecmp( name, "versioncmp" ) ) {
			result.SetIntegerValue(r);
		} else if( 0 == strcmp( name, "versionLE" ) ) {
			result.SetBooleanValue( r <= 0 );
		} else if( 0 == strcmp( name, "versionLT" ) ) {
			result.SetBooleanValue( r < 0 );
		} else if( 0 == strcmp( name, "versionGE" ) ) {
			result.SetBooleanValue( r >= 0 );
		} else if( 0 == strcmp( name, "versionGT" ) ) {
			result.SetBooleanValue( r > 0 );
		} else if( 0 == strcmp( name, "versionEQ" ) ) {
			result.SetBooleanValue( r == 0 );
		} else {
			// This should never happen.
			result.SetErrorValue();
		}
	} else {
		result.SetErrorValue();
		return true;
	}

	return true;
}

bool FunctionCall::
compareString( const char*name, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value 	arg0, arg1;
    Value   arg0_s, arg1_s;

    // Must have two arguments
	if(argList.size() != 2) {
		result.SetErrorValue( );
		return( true );
	}

    // Evaluate both arguments
	if(!argList[0]->Evaluate(state, arg0) ||
       !argList[1]->Evaluate(state, arg1)) {
		result.SetErrorValue();
		return false;
	}

    // If either argument is undefined, then the result is
    // undefined.
	if(arg0.IsUndefinedValue() || arg1.IsUndefinedValue()) {
		result.SetUndefinedValue( );
		return true;
    }

    string  s0, s1;
    if (   convertValueToStringValue(arg0, arg0_s)
        && convertValueToStringValue(arg1, arg1_s)
        && arg0_s.IsStringValue(s0)
        && arg1_s.IsStringValue(s1)) {

        int  order;
        
        if (strcasecmp(name, "strcmp") == 0) {
            order = strcmp(s0.c_str(), s1.c_str());
            if (order < 0) order = -1;
            else if (order > 0) order = 1;
        } else {
            order = strcasecmp(s0.c_str(), s1.c_str());
            if (order < 0) order = -1;
            else if (order > 0) order = 1;
        }
        result.SetIntegerValue(order);
    } else {
        result.SetErrorValue();
    }

	return( true );
}

bool FunctionCall::
convInt( const char*, const ArgumentList &argList, EvalState &state, 
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

    convertValueToIntegerValue(arg, result);
    return true;
}


bool FunctionCall::
convReal( const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	Value  arg;

    // takes exactly one argument
	if( argList.size() != 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

    convertValueToRealValue(arg, result);
    return true;
}

bool FunctionCall::
convString(const char*, const ArgumentList &argList, EvalState &state, 
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

    convertValueToStringValue(arg, result);
    return true;
}

bool FunctionCall::
unparse(const char*, const ArgumentList &argList, EvalState &state, 
	Value &result )
{
	
	if( argList.size() != 1 || argList[0]->GetKind() != ATTRREF_NODE ) {
		result.SetErrorValue( );
	}
	else{
	 
		// use the printpretty on arg0 to spew out 
		PrettyPrint     unp;
		string          szAttribute,szValue;
		ExprTree* 		pTree;
		
		unp.Unparse( szAttribute, argList[0] );
		// look them up argument within context of the ad.
		if ( state.curAd && (pTree = state.curAd->Lookup(szAttribute)) )
		{
			unp.Unparse( szValue, pTree );
		}
		
		result.SetStringValue(szValue);
	}
	
	return (true); 
}

bool FunctionCall::hasRefs(const char*, const ArgumentList& argList, EvalState& state, Value& result)
{
	std::string val;
	Value arg;

	// takes at least one argument which must be an attribute reference
	if (argList.size() < 1 || argList.size() > 2 || argList[0]->GetKind() != ATTRREF_NODE) {
		result.SetErrorValue( );
		return true;
	}

	// de-reference the first argument to the the exprtree that it points to
	ExprTree * expr = nullptr;
	AttributeReference * atref = reinterpret_cast<AttributeReference *>(argList[0]);
	int er = AttributeReference::Deref(*atref, state, expr);
	if (er != EVAL_OK) {
		result.SetUndefinedValue();
		return true;
	}

	// for the 2 arg form, the second argument is a regex pattern to be compared against
	// each of the unresolved references
	pcre2_code * re = nullptr;
	if (argList.size() == 2) {
		const char* pattern = nullptr;
		if ( !argList[1]->Evaluate(state, arg) || ! arg.IsStringValue(pattern)) {
			result.SetErrorValue();
			return false;
		}

		int error_number;
		PCRE2_SIZE error_offset;
		PCRE2_SPTR pattern_pcre2str = reinterpret_cast<const unsigned char *>(pattern);
		re = pcre2_compile(pattern_pcre2str, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &error_number, &error_offset, NULL);
		if ( ! re) {
			// error in pattern
			result.SetErrorValue();
			return true;
		}
		result.SetBooleanValue(false); // assume no match
	}

	if (expr && state.curAd) {
		classad::References attrs;
		if (ClassAd::_GetExternalReferences(expr, state.curAd, state, attrs, true)) {
			for (auto it = attrs.begin(); it != attrs.end(); ++it) {
				const char * attr = it->c_str();
				size_t len = it->length();
				if (0 == strncasecmp(attr, "target.", 7)) {
					attr += 7;
					len -= 7;
				}
				if (re) {
					pcre2_match_data * match_data = pcre2_match_data_create_from_pattern(re, NULL);
					PCRE2_SPTR attr_pcre2str = reinterpret_cast<const unsigned char *>(attr);
					if (pcre2_match(re, attr_pcre2str, len, 0, PCRE2_NOTEMPTY, match_data, NULL) > 0) {
						result.SetBooleanValue(true); // found a match
						pcre2_match_data_free(match_data);
						break;
					} else {
						pcre2_match_data_free(match_data);
					}
				} else {
					if (!val.empty()) val += ",";
					val += attr;
				}
			}
		}
	}

	if ( ! re) {
		result.SetStringValue(val);
	} else {
		pcre2_code_free(re);
	}
	return true;
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
		case Value::UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return( true );

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::SCLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::SLIST_VALUE:
		case Value::ABSOLUTE_TIME_VALUE:
			result.SetErrorValue( );
			return( true );

		case Value::BOOLEAN_VALUE:
			result.CopyFrom( arg );
			return( true );

		case Value::INTEGER_VALUE:
			{
				long long ival;
				arg.IsIntegerValue( ival );
				result.SetBooleanValue( ival != 0 );
				return( true );
			}

		case Value::REAL_VALUE:
			{
				double rval;
				arg.IsRealValue( rval );
				result.SetBooleanValue( rval != 0.0 );
				return( true );
			}

		case Value::STRING_VALUE:
			{
				string buf;
				arg.IsStringValue( buf );
				if (strcasecmp("false", buf.c_str()) == 0) { 
					result.SetBooleanValue( false );
					return( true );
				} 
				if (strcasecmp("true", buf.c_str()) == 0) { 
					result.SetBooleanValue( true );
					return( true );
				} 
				result.SetUndefinedValue();
				return( true );
			}

		case Value::RELATIVE_TIME_VALUE:
			{
				time_t rsecs;
				arg.IsRelativeTimeValue( rsecs );
				result.SetBooleanValue( rsecs != 0 );
				return( true );
			}

		default:
			CLASSAD_EXCEPT( "Should not reach here" );
	}
	return( false );
}




bool FunctionCall::
convTime(const char* name,const ArgumentList &argList,EvalState &state,
	Value &result)
{
	Value	arg, arg2;
	bool	relative = ( strcasecmp( "reltime", name ) == 0 );
	bool    secondarg = false; // says whether a 2nd argument exists
	int     arg2num;

    if (argList.size() == 0 && !relative) {
        // absTime with no arguments returns the current time. 
        return currentTime(name, argList, state, result);
    }
	if(( argList.size() < 1 )  || (argList.size() > 2)) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}
	if(argList.size() == 2) { // we have a 2nd argument
		secondarg = true;
		if( !argList[1]->Evaluate( state, arg2 ) ) {
			result.SetErrorValue( );
			return( false );
		}
		int ivalue2 = 0;
		double rvalue2 = 0;
		time_t rsecs = 0;
		if(relative) {// 2nd argument is N/A for reltime
			result.SetErrorValue( );
			return( true );
		}
		// 2nd arg should be integer, real or reltime
		else if (arg2.IsIntegerValue(ivalue2)) {
			arg2num = ivalue2;
		}
		else if (arg2.IsRealValue(rvalue2)) {
			arg2num = (int)rvalue2;
		}
		else if(arg2.IsRelativeTimeValue(rsecs)) {
			arg2num = rsecs;
		}
		else {
			result.SetErrorValue( );
			return( true );
		}
	} else {
        secondarg = false;
        arg2num = 0;
    }

	switch( arg.GetType( ) ) {
		case Value::UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return( true );

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::SCLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::SLIST_VALUE:
		case Value::BOOLEAN_VALUE:
			result.SetErrorValue( );
			return( true );

		case Value::INTEGER_VALUE:
			{
				int ivalue;
				arg.IsIntegerValue( ivalue );
				if( relative ) {
					result.SetRelativeTimeValue( (time_t) ivalue );
				} else {
				  abstime_t atvalue;
				  atvalue.secs = ivalue;
				  if(secondarg)   //2nd arg is the offset in secs
				    atvalue.offset = arg2num;
				  else   // the default offset is the current timezone
				    atvalue.offset = Literal::findOffset(atvalue.secs);
				  
				  if(atvalue.offset == -1) {
				    result.SetErrorValue( );
				    return( false );
				  }
				  else
				    result.SetAbsoluteTimeValue( atvalue );
				}
				return( true );
			}

		case Value::REAL_VALUE:
			{
				double	rvalue;

				arg.IsRealValue( rvalue );
				if( relative ) {
					result.SetRelativeTimeValue( rvalue );
				} else {
				  	  abstime_t atvalue;
					  atvalue.secs = (int)rvalue;
					  if(secondarg)         //2nd arg is the offset in secs
					    atvalue.offset = arg2num;
					  else    // the default offset is the current timezone
					    atvalue.offset = Literal::findOffset(atvalue.secs);
					  result.SetAbsoluteTimeValue( atvalue );
				}
				return( true );
			}

		case Value::STRING_VALUE:
			{
			  //should'nt come here
			  // a string argument to this function is transformed to a literal directly
			}

		case Value::ABSOLUTE_TIME_VALUE:
			{
				abstime_t secs = { 0, 0 };
				arg.IsAbsoluteTimeValue( secs );
				if( relative ) {
					result.SetRelativeTimeValue( secs.secs );
				} else {
					result.CopyFrom( arg );
				}
				return( true );
			}

		case Value::RELATIVE_TIME_VALUE:
			{
				if( relative ) {
					result.CopyFrom( arg );
				} else {
					time_t secs;
					arg.IsRelativeTimeValue( secs );
					abstime_t atvalue;
					atvalue.secs = secs;
					if(secondarg)    //2nd arg is the offset in secs
						atvalue.offset = arg2num;
					else      // the default offset is the current timezone
						atvalue.offset = Literal::findOffset(atvalue.secs);	
					result.SetAbsoluteTimeValue( atvalue );
				}
				return( true );
			}

		default:
			CLASSAD_EXCEPT( "Should not reach here" );
			return( false );
	}
}


bool FunctionCall::
doRound( const char* name,const ArgumentList &argList,EvalState &state,
	Value &result )
{
	Value	arg;
    Value   realValue;

		// takes exactly one argument
	if( argList.size() != 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

    if (arg.GetType() == Value::INTEGER_VALUE) {
        result.CopyFrom(arg);
    } else {
        if (!convertValueToRealValue(arg, realValue)) {
            result.SetErrorValue();
        } else {
            double rvalue;
            realValue.IsRealValue(rvalue);
            if (strcasecmp("floor", name) == 0) {
                result.SetIntegerValue((long long) floor(rvalue));
            } else if (   strcasecmp("ceil", name)    == 0 
                       || strcasecmp("ceiling", name) == 0) {
                result.SetIntegerValue((long long) ceil(rvalue));
            } else if( strcasecmp("round", name) == 0) {
                result.SetIntegerValue((long long) rint(rvalue));
            } else {
                result.SetErrorValue( );
            }
        }
    }
    return true;
}

// 2 argument math functions.
bool FunctionCall::
doMath2( const char* name,const ArgumentList &argList,EvalState &state,
	Value &result )
{
	Value	arg, arg2;

		// takes 2 arguments  pow(val,base)
	if( argList.size() != 2) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ||
		!argList[1]->Evaluate( state, arg2 )) {
		result.SetErrorValue( );
		return( false );
	}

	if (strcasecmp("pow", name) == 0) {
		// take arg2 to the power of arg2
		long long ival, ibase;
		if (arg.IsIntegerValue(ival) && arg2.IsIntegerValue(ibase) && ibase >= 0) {
			ival = (long long) (pow((double)ival, (double)ibase) + 0.5);
			result.SetIntegerValue(ival);
		} else {
			Value	realValue, realBase;
			if ( ! convertValueToRealValue(arg, realValue) ||
			     ! convertValueToRealValue(arg2, realBase)) {
			result.SetErrorValue();
			} else {
				double rvalue = 0, rbase = 1;
				realValue.IsRealValue(rvalue);
				realBase.IsRealValue(rbase);
				result.SetRealValue( pow(rvalue, rbase) );
			}
		}
	} else if (strcasecmp("quantize", name) == 0) {
		// quantize arg1 to the next integral multiple of arg2
		// if arg2 is a list, choose the first item from the list that is larger than arg1
		// if arg1 is larger than all of the items in the list, the result is an error.

		Value val, base;
		if ( ! convertValueToRealValue(arg, val)) {
			result.SetErrorValue();
		} else {
			// get the value to quantize into rval.
			double rval, rbase;
			val.IsRealValue(rval);

			if (arg2.IsListValue()) {
				const ExprList *list = NULL;
				arg2.IsListValue(list);
				base.SetRealValue(0.0), rbase = 0.0; // treat an empty list as 'don't quantize'
				for (const auto *expr: *list) {
					if ( ! expr->Evaluate(state, base)) {
						result.SetErrorValue();
						return false; // eval should not fail
					}
					if (convertValueToRealValue(base, val)) {
						val.IsRealValue(rbase);
						if (rbase >= rval) {
							result = base;
							return true;
						}
					} else {
						//TJ: should we ignore values that can't be converted?
						result.SetErrorValue();
						return true;
					}
				}
				// at this point base is the value of the last expression in the list.
				// and rbase is the real value of it and rval > rbase.
				// when this happens we want to quantize on multiples of the last
				// list value, as if on a single value were passed rather than a list.
				arg2 = base;
			} else {
				// if arg2 is not a list, then it must evaluate to a real value
				// or we can't use it. (note that if it's an int, we still want
				// to return an int, but we assume that all ints can be converted to real)
				if ( ! convertValueToRealValue(arg2, base)) {
					result.SetErrorValue();
					return true;
				}
				base.IsRealValue(rbase);
			}

			// at this point rbase should contain the real value of either arg2 or the
			// last entry in the list. and rval should contain the value to be quantized.

			long long ival, ibase;
			if (arg2.IsIntegerValue(ibase)) {
				// quantize to an integer base,
				if ( ! ibase)
					result = arg;
				else if (arg.IsIntegerValue(ival)) {
					ival = ((ival + ibase-1) / ibase) * ibase;
					result.SetIntegerValue(ival);
				} else {
					rval = ceil(rval / ibase) * ibase;
					result.SetRealValue(rval);
				}
			} else {
				const double epsilon = 1e-8;
				if (rbase >= -epsilon && rbase <= epsilon) {
					result = arg;
				} else {
					// we already have the real-valued base in rbase so just use it here.
					rval = ceil(rval / rbase) * rbase;
					result.SetRealValue(rval);
				}
			}
		}
	} else {
		// unknown 2 argument math function
		result.SetErrorValue( );
	}
	return true;
}

bool FunctionCall::
random( const char*,const ArgumentList &argList,EvalState &state,
	Value &result )
{
	Value	arg;
    int     int_max;
    double  double_max;
    int     random_int;
    double  random_double;

	// TODO Make this work properly for ranges beyond 2^31
	//   (2^15 on windows)

    // takes exactly one argument
	if( argList.size() > 1 ) {
		result.SetErrorValue( );
		return( true );
	} else if ( argList.size() == 0 ) {
		arg.SetRealValue( 1.0 );
	} else if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}

    if (arg.IsIntegerValue(int_max)) {
		if (int_max <= 0) {
			result.SetErrorValue( );
			return( false );
		}
        random_int = get_random_integer() % int_max;
        result.SetIntegerValue(random_int);
    } else if (arg.IsRealValue(double_max)) {
		if (double_max <= 0) {
			result.SetErrorValue( );
			return( false );
		}
        random_double = double_max * get_random_real();
        result.SetRealValue(random_double);
    } else {
        result.SetErrorValue();
    }

    return true;
}

bool FunctionCall::
ifThenElse( const char* /* name */,const ArgumentList &argList,EvalState &state,
	Value &result )
{
	Value	arg1;
	bool    arg1_bool = false;;

		// takes exactly three arguments
	if( argList.size() != 3 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg1 ) ) {
		result.SetErrorValue( );
		return( false );
	}

	switch( arg1.GetType() ) {
	case Value::BOOLEAN_VALUE:
		if( !arg1.IsBooleanValue(arg1_bool) ) {
			result.SetErrorValue();
			return( false );
		}
		break;
	case Value::INTEGER_VALUE: {
		long long intval;
		if( !arg1.IsIntegerValue(intval) ) {
			result.SetErrorValue();
			return( false );
		}
		arg1_bool = intval != 0;
		break;
	}
	case Value::REAL_VALUE: {
		double realval;
		if( !arg1.IsRealValue(realval) ) {
			result.SetErrorValue();
			return( false );
		}
		arg1_bool = realval != 0.0;
		break;
	}

	case Value::UNDEFINED_VALUE:
		result.SetUndefinedValue();
		return( true );

	case Value::ERROR_VALUE:
	case Value::CLASSAD_VALUE:
	case Value::SCLASSAD_VALUE:
	case Value::LIST_VALUE:
	case Value::SLIST_VALUE:
	case Value::STRING_VALUE:
	case Value::ABSOLUTE_TIME_VALUE:
	case Value::RELATIVE_TIME_VALUE:
	case Value::NULL_VALUE:
	default:
		result.SetErrorValue();
		return( true );
	}

	if( arg1_bool ) {
		if( !argList[1]->Evaluate( state, result ) ) {
			result.SetErrorValue( );
			return( false );
		}
	}
	else {
		if( !argList[2]->Evaluate( state, result ) ) {
			result.SetErrorValue( );
			return( false );
		}
	}

    return true;
}

bool FunctionCall::
eval( const char* /* name */,const ArgumentList &argList,EvalState &state,
	  Value &result )
{
	Value arg,strarg;

		// takes exactly one argument
	if( argList.size() != 1 ) {
		result.SetErrorValue( );
		return true;
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return false;
	}

	string s;
    if( !convertValueToStringValue(arg, strarg) ||
		!strarg.IsStringValue( s ) )
	{
		result.SetErrorValue();
		return true;
	}

	if( state.depth_remaining <= 0 ) {
		result.SetErrorValue();
		return false;
	}

	ClassAdParser parser;
	ExprTree *expr = NULL;
	if( !parser.ParseExpression( s.c_str(), expr, true ) || !expr ) {
		if( expr ) {
			delete expr;
		}
		result.SetErrorValue();
		return true;
	}

	// add the new tree to the evaluation cache for later deletion
	state.AddToDeletionCache(expr);

	state.depth_remaining--;

	expr->SetParentScope( state.curAd );

	bool eval_ok = expr->Evaluate( state, result );

	state.depth_remaining++;

	if( !eval_ok ) {
		result.SetErrorValue();
		return false;
	}
	return true;
}

bool FunctionCall::
interval( const char* /* name */,const ArgumentList &argList,EvalState &state,
	Value &result )
{
	Value	arg,intarg;
	int tot_secs;

		// takes exactly one argument
	if( argList.size() != 1 ) {
		result.SetErrorValue( );
		return( true );
	}
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}
	if( !convertValueToIntegerValue(arg,intarg) ) {
		result.SetErrorValue( );
		return( true );
	}

	if( !intarg.IsIntegerValue(tot_secs) ) {
		result.SetErrorValue( );
		return( true );
	}

	int days,hours,min,secs;
	days = tot_secs / (3600*24);
	tot_secs %= (3600*24);
	hours = tot_secs / 3600;
	tot_secs %= 3600;
	min = tot_secs / 60;
	secs = tot_secs % 60;

	char strval[25];
	if ( days != 0 ) {
		snprintf(strval, sizeof(strval), "%d+%02d:%02d:%02d", days, abs(hours), abs(min), abs(secs) );
	} else if ( hours != 0 ) {
		snprintf(strval, sizeof(strval), "%d:%02d:%02d", hours, abs(min), abs(secs) );
	} else if ( min != 0 ) {
		snprintf(strval, sizeof(strval), "%d:%02d", min, abs(secs) );
	} else {
		snprintf(strval, sizeof(strval), "%d", secs );
	}
	result.SetStringValue(strval);

    return true;
}

bool FunctionCall::
debug( const char* name,const ArgumentList &argList,EvalState &state,
	Value &result )
{
	Value	arg;

	// takes exactly one argument
	if( argList.size() != 1 ) {
		result.SetErrorValue( );
		return( true );
	}

	bool old_debug = state.debug;
	state.debug = true;

	double diff = 0;
#ifndef WIN32
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
#endif
	if( !argList[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return( false );
	}
#ifndef WIN32
	gettimeofday(&end, NULL);
	diff = (end.tv_sec + (end.tv_usec * 0.000001)) -
		(begin.tv_sec + (begin.tv_usec * 0.000001));
#endif
	state.debug = old_debug;
	result = arg;
	argList[0]->debug_format_value(result, diff);
	return true;
}

static bool regexp_helper(const char *pattern, const char *target,
                          const char *replace,
                          bool have_options, string options_string,
                          Value &result);

bool FunctionCall::
substPattern( const char *name,const ArgumentList &argList,EvalState &state,
	Value &result )
{
    bool        have_options;
	Value 		arg0, arg1, arg2, arg3;
	const char	*pattern=NULL, *target=NULL, *replace=NULL;
    string      options_string;

		// need three or four arguments: pattern, string, replace, optional settings
	if( argList.size() != 3 && argList.size() != 4) {
		result.SetErrorValue( );
		return( true );
	}
    if (argList.size() == 3) {
        have_options = false;
    } else {
        have_options = true;
    }

		// Evaluate args
	if( !argList[0]->Evaluate( state, arg0 ) || 
		!argList[1]->Evaluate( state, arg1 ) ||
		!argList[2]->Evaluate( state, arg2 ) ) {
		result.SetErrorValue( );
		return( false );
	}
    if( have_options && !argList[3]->Evaluate( state, arg3 ) ) {
		result.SetErrorValue( );
		return( false );
    }

		// if any arg is error, the result is error
	if( arg0.IsErrorValue( ) || arg1.IsErrorValue( ) || arg2.IsErrorValue() ) {
		result.SetErrorValue( );
		return( true );
	}
    if( have_options && arg3.IsErrorValue( ) ) {
        result.SetErrorValue( );
        return( true );
    }

		// if any arg is undefined, the result is undefined
	if( arg0.IsUndefinedValue( ) || arg1.IsUndefinedValue( ) || arg2.IsUndefinedValue() ) {
		result.SetUndefinedValue( );
		return( true );
	}
    if( have_options && arg3.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return( true );
    } else if ( have_options && !arg3.IsStringValue( options_string ) ) {
        result.SetErrorValue( );
        return( true );
    }

	if( strcasecmp( name, "replace" ) == 0 ) {
		have_options = true;
		options_string += "f";
	} else if( strcasecmp( name, "replaceall" ) == 0 ) {
		have_options = true;
		options_string += "fg";
	}

		// if either argument is not a string, the result is an error
	if( !arg0.IsStringValue( pattern ) || !arg1.IsStringValue( target ) || !arg2.IsStringValue( replace ) ) {
		result.SetErrorValue( );
		return( true );
	}
    return regexp_helper(pattern, target, replace, have_options, options_string, result);
}

bool FunctionCall::
matchPattern( const char*,const ArgumentList &argList,EvalState &state,
	Value &result )
{
    bool        have_options;
	Value 		arg0, arg1, arg2;
	const char	*pattern=NULL, *target=NULL;
    string      options_string;

		// need two or three arguments: pattern, string, optional settings
	if( argList.size() != 2 && argList.size() != 3) {
		result.SetErrorValue( );
		return( true );
	}
    if (argList.size() == 2) {
        have_options = false;
    } else {
        have_options = true;
    }

		// Evaluate args
	if( !argList[0]->Evaluate( state, arg0 ) || 
		!argList[1]->Evaluate( state, arg1 ) ) {
		result.SetErrorValue( );
		return( false );
	}
    if( have_options && !argList[2]->Evaluate( state, arg2 ) ) {
		result.SetErrorValue( );
		return( false );
    }

		// if either arg is error, the result is error
	if( arg0.IsErrorValue( ) || arg1.IsErrorValue( ) ) {
		result.SetErrorValue( );
		return( true );
	}
    if( have_options && arg2.IsErrorValue( ) ) {
        result.SetErrorValue( );
        return( true );
    }

		// if either arg is undefined, the result is undefined
	if( arg0.IsUndefinedValue( ) || arg1.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return( true );
	}
    if( have_options && arg2.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return( true );
    } else if ( have_options && !arg2.IsStringValue( options_string ) ) {
        result.SetErrorValue( );
        return( true );
    }

		// if either argument is not a string, the result is an error
	if( !arg0.IsStringValue( pattern ) || !arg1.IsStringValue( target ) ) {
		result.SetErrorValue( );
		return( true );
	}
    return regexp_helper(pattern, target, NULL, have_options, options_string, result);
}

bool FunctionCall::
matchPatternMember( const char*,const ArgumentList &argList,EvalState &state,
	Value &result )
{
    bool            have_options;
	Value 		    arg0, arg1, arg2;
	const char	    *pattern=NULL, *target=NULL;
    const ExprList	*target_list;
    string          options_string;

    // need two or three arguments: pattern, list, optional settings
	if( argList.size() != 2 && argList.size() != 3) {
		result.SetErrorValue( );
		return( true );
	}
    if (argList.size() == 2) {
        have_options = false;
    } else {
        have_options = true;
    }

		// Evaluate args
	if( !argList[0]->Evaluate( state, arg0 ) || 
		!argList[1]->Evaluate( state, arg1 ) ) {
		result.SetErrorValue( );
		return( false );
	}
    if( have_options && !argList[2]->Evaluate( state, arg2 ) ) {
		result.SetErrorValue( );
		return( false );
    }

		// if either arg is error, the result is error
	if( arg0.IsErrorValue( ) || arg1.IsErrorValue( ) ) {
		result.SetErrorValue( );
		return( true );
	}
    if( have_options && arg2.IsErrorValue( ) ) {
        result.SetErrorValue( );
        return( true );
    }

		// if either arg is undefined, the result is undefined
	if( arg0.IsUndefinedValue( ) || arg1.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return( true );
	}
    if( have_options && arg2.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return( true );
    } else if ( have_options && !arg2.IsStringValue( options_string ) ) {
        result.SetErrorValue( );
        return( true );
    }

		// if the arguments are not of the correct types, the result is an error
	if( !arg0.IsStringValue( pattern ) || !arg1.IsListValue( target_list ) ) {
		result.SetErrorValue( );
		return( true );
	}
    result.SetBooleanValue(false);
    
    ExprTree *target_expr;
	bool couldBeUndefined = false;
    ExprList::const_iterator list_iter = target_list->begin();
    while (list_iter != target_list->end()) {
        Value target_value;
        Value have_match_value;
        target_expr = *list_iter;
        if (target_expr != NULL) {
            if( !target_expr->Evaluate(state, target_value)) {
                result.SetErrorValue();
                return true;
            }

			// If the list element is not a string, result is error
			// unless it is undefined, then total result is either
			// true (if one matches) or undefined
            if (!target_value.IsStringValue(target)) {
				if (target_value.IsUndefinedValue()) {
					couldBeUndefined = true;
				} else {
					// Some non-string, not-undefined value, error and give up
                	result.SetErrorValue();
                	return true;
				}
            } else {
                bool have_match;
                bool success = regexp_helper(pattern, target, NULL, have_options, options_string, have_match_value);
                if (!success) {
                    result.SetErrorValue();
                    return true;
                } else {
                    if (have_match_value.IsBooleanValue(have_match) && have_match) {
                        result.SetBooleanValue(true);
                        return true;
                    }
                }
            }
        } else {
            result.SetErrorValue();
            return(false);
        }
        list_iter++;
    }
	if (couldBeUndefined) {
		result.SetUndefinedValue();
	}
    return true;
}

static bool regexp_helper(
    const char *pattern,
    const char *target,
	const char *replace,
    bool       have_options,
    string     options_string,
    Value      &result)
{
    int         options = 0;
	int			status;
	bool		full_target = false;
	bool		find_all = false;

	PCRE2_SIZE error_offset;
	int error_code;
	pcre2_code * re = NULL;
	PCRE2_SPTR pattern_pcre2 = reinterpret_cast<const unsigned char *>(pattern);
	PCRE2_SIZE *ovector = NULL;
	bool empty_match = false;
	uint32_t addl_opts = 0;
	PCRE2_SIZE target_len = (PCRE2_SIZE) strlen(target);
	PCRE2_SPTR target_pcre2str = reinterpret_cast<const unsigned char *>(target);

	size_t target_idx = 0;
	std::string output;

    if( have_options ){
        // We look for the options we understand, and ignore
        // any others that we might find, hopefully allowing
        // forwards compatibility.
        if ( options_string.find( 'i' ) != string::npos ) {
            options |= PCRE2_CASELESS;
        } 
        if ( options_string.find( 'm' ) != string::npos ) {
            options |= PCRE2_MULTILINE;
        }
        if ( options_string.find( 's' ) != string::npos ) {
            options |= PCRE2_DOTALL;
        }
        if ( options_string.find( 'x' ) != string::npos ) {
            options |= PCRE2_EXTENDED;
        }
		if ( replace ) {
			// The 'f' option means that the result should consist of
			// the full target string with any replacement(s) applied
			// in-place.
			if ( options_string.find( 'f' ) != string::npos ) {
				full_target = true;
			}
			// The 'g' option means that all matches to the pattern
			// should be found in the target string (without overlaps).
			if ( options_string.find( 'g' ) != string::npos ) {
				find_all = true;
			}
		}
    }

    re = pcre2_compile(pattern_pcre2, PCRE2_ZERO_TERMINATED, options, &error_code, &error_offset, NULL);
    if ( re == NULL ){
			// error in pattern
		result.SetErrorValue( );
		goto cleanup;
	}

	// NOTE: For global replacement option 'g', we don't properly
	//   handle situations where a single character of the target
	//   consists of multiple bytes. These are UTF-8 strings and
	//   environments where a newline is CRLF. These options can be
	//   enabled within the search pattern in PCRE.

	do {

		if ( empty_match ) {
			addl_opts = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
		} else {
			addl_opts = 0;
		}

		pcre2_match_data * match_data = pcre2_match_data_create_from_pattern(re, NULL);
		status = pcre2_match(re, target_pcre2str, target_len, target_idx, addl_opts, match_data, NULL);
		ovector = pcre2_get_ovector_pointer(match_data);
		if (empty_match && status == PCRE2_ERROR_NOMATCH) {
			output += target[target_idx];
			target_idx++;
			empty_match = false;
			status = 0;
			continue;
		}

		if( status >= 0 && replace ) {
			PCRE2_UCHAR **groups_pcre2 = nullptr;
			int ngroups = status;
			const char *replace_ptr = replace;

			if ( full_target ) {
				output.append(&target[target_idx], (ovector[0]) - target_idx);
			}

			if (pcre2_substring_list_get(match_data, &groups_pcre2, nullptr)) {
				result.SetErrorValue( );
				goto cleanup;
			}
			else while (*replace_ptr) {
				if (*replace_ptr == '\\') {
					if (isdigit(replace_ptr[1])) {
						int offset = replace_ptr[1] - '0';
						replace_ptr++;
						if( offset >= ngroups ) {
							result.SetErrorValue();
							goto cleanup;
						}
						output += reinterpret_cast<const char *>(groups_pcre2[offset]);
					} else {
						output += '\\';
					}
				} else {
					output += *replace_ptr;
				}
				replace_ptr++;
			}

#if PCRE2_MAJOR == 10 && PCRE2_MINOR < 43
			pcre2_substring_list_free((PCRE2_SPTR *)groups_pcre2);
#else
			pcre2_substring_list_free(groups_pcre2);
#endif

			target_idx = ovector[1];
			if ( ovector[0] == ovector[1] ) {
				empty_match = true;
			}
		}
	
		pcre2_match_data_free(match_data);

    } while (status >= 0 && find_all);

	if ( status < 0 && status != PCRE2_ERROR_NOMATCH ) {
		result.SetErrorValue();
	} else if ( !replace ) {
		result.SetBooleanValue( status >= 0 );
	} else {
		if ( full_target ) {
			output += &target[target_idx];
		}
		result.SetStringValue(output);
	}
 cleanup:
	if ( re ) {
		pcre2_code_free(re);
	}
    return true;
}

static bool 
doSplitTime(const Value &time, ClassAd * &splitClassAd)
{
    bool             did_conversion;
    int              integer;
    double           real;
    abstime_t        asecs;
    double           rsecs;
    const ClassAd    *classad;

    did_conversion = true;
    if (time.IsIntegerValue(integer)) {
        asecs.secs = integer;
        asecs.offset = timezone_offset( asecs.secs, false );
        absTimeToClassAd(asecs, splitClassAd);
    } else if (time.IsRealValue(real)) {
        asecs.secs = (int) real;
        asecs.offset = timezone_offset( asecs.secs, false );
        absTimeToClassAd(asecs, splitClassAd);
    } else if (time.IsAbsoluteTimeValue(asecs)) {
        absTimeToClassAd(asecs, splitClassAd);
    } else if (time.IsRelativeTimeValue(rsecs)) {
        relTimeToClassAd(rsecs, splitClassAd);
    } else if (time.IsClassAdValue(classad)) {
        splitClassAd = new ClassAd;
        splitClassAd->CopyFrom(*classad);
    } else {
        did_conversion = false;
    }
    return did_conversion;
}

static void 
absTimeToClassAd(const abstime_t &asecs, ClassAd * &splitClassAd)
{
	time_t	  clock;
    struct tm tms;

    splitClassAd = new ClassAd;

    clock = asecs.secs + asecs.offset;
    getGMTime( &clock, &tms );

    splitClassAd->InsertAttr("Type", "AbsoluteTime");
    splitClassAd->InsertAttr("Year", tms.tm_year + 1900);
    splitClassAd->InsertAttr("Month", tms.tm_mon + 1);
    splitClassAd->InsertAttr("Day", tms.tm_mday);
    splitClassAd->InsertAttr("Hours", tms.tm_hour);
    splitClassAd->InsertAttr("Minutes", tms.tm_min);
    splitClassAd->InsertAttr("Seconds", tms.tm_sec);
    // Note that we convert the timezone from seconds to minutes.
    splitClassAd->InsertAttr("Offset", asecs.offset);
    
    return;
}

static void 
relTimeToClassAd(double rsecs, ClassAd * &splitClassAd) 
{
    int		days, hrs, mins;
    double  secs;
    bool    is_negative;

    if( rsecs < 0 ) {
        rsecs = -rsecs;
        is_negative = true;
    } else {
        is_negative = false;
    }
    days = (int) rsecs;
    hrs  = days % 86400;
    mins = hrs  % 3600;
    secs = (mins % 60) + (rsecs - floor(rsecs));
    days = days / 86400;
    hrs  = hrs  / 3600;
    mins = mins / 60;
    
    if (is_negative) {
        if (days > 0) {
            days = -days;
        } else if (hrs > 0) {
            hrs = -hrs;
        } else if (mins > 0) {
            mins = -mins;
        } else {
            secs = -secs;
        }
    }
    
    splitClassAd = new ClassAd;
    splitClassAd->InsertAttr("Type", "RelativeTime");
    splitClassAd->InsertAttr("Days", days);
    splitClassAd->InsertAttr("Hours", hrs);
    splitClassAd->InsertAttr("Minutes", mins);
    splitClassAd->InsertAttr("Seconds", secs);
    
    return;
}

static void
make_formatted_time(const struct tm &time_components, string &format,
                    Value &result)
{
    char output[1024]; // yech
    strftime(output, 1023, format.c_str(), &time_components);
    result.SetStringValue(output);
    return;
}

static void
split_string_list(char const *str,char const *delim,vector< string > &list)
{
	if( !delim || !delim[0] ) {
		delim = " ,";
	}
	if( !str ) {
		return;
	}
	string item;
	while( *str ) {
		size_t len = strcspn(str,delim);
		if( len > 0 ) {
			item.assign(str,len);
			list.push_back(item);
			str += len;
		}
		if( *str ) {
			str++;
		}
	}
}

static void
split_string_set(char const *str,char const *delim,set< string > &string_set)
{
	if( !delim || !delim[0] ) {
		delim = " ,";
	}
	if( !str ) {
		return;
	}
	set<string>::value_type item;
	while( *str ) {
		size_t len = strcspn(str,delim);
		if( len > 0 ) {
			item.assign(str,len);
			string_set.insert(item);
			str += len;
		}
		if( *str ) {
			str++;
		}
	}
}

static bool
stringListsIntersect(const char*,const ArgumentList &argList,EvalState &state,Value &result)
{
	Value arg0, arg1, arg2;
	bool have_delimiter;
	string str0,str1,delimiter_string;

    // need two or three arguments: pattern, list, optional settings
	if( argList.size() != 2 && argList.size() != 3) {
		result.SetErrorValue( );
		return true;
	}
    if (argList.size() == 2) {
        have_delimiter = false;
    } else {
        have_delimiter = true;
    }

		// Evaluate args
	if( !argList[0]->Evaluate( state, arg0 ) || 
		!argList[1]->Evaluate( state, arg1 ) ) {
		result.SetErrorValue( );
		return true;
	}
    if( have_delimiter && !argList[2]->Evaluate( state, arg2 ) ) {
		result.SetErrorValue( );
		return true;
    }

		// if either arg is error, the result is error
	if( arg0.IsErrorValue( ) || arg1.IsErrorValue( ) ) {
		result.SetErrorValue( );
		return true;
	}
    if( have_delimiter && arg2.IsErrorValue( ) ) {
        result.SetErrorValue( );
        return true;
    }

		// if either arg is undefined, the result is undefined
	if( arg0.IsUndefinedValue( ) || arg1.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return true;
	}
    if( have_delimiter && arg2.IsUndefinedValue( ) ) {
		result.SetUndefinedValue( );
		return true;
    } else if ( have_delimiter && !arg2.IsStringValue( delimiter_string ) ) {
        result.SetErrorValue( );
        return true;
    }

		// if the arguments are not of the correct types, the result
		// is an error
	if( !arg0.IsStringValue( str0 ) || !arg1.IsStringValue( str1 ) ) {
		result.SetErrorValue( );
		return true;
	}
    result.SetBooleanValue(false);

	vector< string > list0;
	set< string > set1;

	split_string_list(str0.c_str(),delimiter_string.c_str(),list0);
	split_string_set(str1.c_str(),delimiter_string.c_str(),set1);

	vector< string >::iterator it;
	for(it = list0.begin();
		it != list0.end();
		it++)
	{
		if( set1.count(*it) ) {
			result.SetBooleanValue(true);
			break;
		}
	}

	return true;
}

} // classad
