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


#ifndef __CLASSAD_FN_CALL_H__
#define __CLASSAD_FN_CALL_H__

#include <map>
#include <vector>
#include "classad/classad.h"

namespace classad {

typedef std::vector<ExprTree*> ArgumentList;

typedef bool (*ClassAdFunc)(const char *, const ArgumentList &, EvalState &,
							Value &);

typedef struct 
{
	std::string       functionName;

	ClassAdFunc       function;
	
	// We are likely to add some flags here in the future.  These
	// flags will describe things like how to cache results of the
	// function. Currently they are unused, so set it to zero. 
	int          flags;
} ClassAdFunctionMapping;

// If you create a shared library, you have to provide a function
// named "Initialize" that conforms to this prototype. You will return
// a table of function mappings.  The last mapping in the table should
// have a NULL function pointer. It will be ignored.
typedef	 ClassAdFunctionMapping *(*ClassAdSharedLibraryInit)(void);

/// Node of the expression which represents a call to an function
class FunctionCall : public ExprTree
{
 public:
    /// Copy Constructor
    FunctionCall(FunctionCall &functioncall);

	/// Destructor
	virtual ~FunctionCall ();
	
    FunctionCall & operator=(FunctionCall &functioncall);

	/// node type
	virtual NodeKind GetKind (void) const { return FN_CALL_NODE; }

	/** Factory method to make a function call expression
	 * 	@param fnName	The name of the function to be called
	 * 	@param argList	A vector representing the argument list
	 * 	@return The constructed function call expression
	 */
	static FunctionCall *MakeFunctionCall( const std::string &fnName, 
										   std::vector<ExprTree*> &argList );
	
#ifdef TJ_PICKLE
	static FunctionCall *Make(ExprStream & stm);
	// validate and skip over the next expression in the stream if it is a valid ClassAd
	// returns the number of bytes read from the stream, or 0 on failure.
	static unsigned int Scan(ExprStream & stm);
	unsigned int Pickle(ExprStreamMaker & stm) const;
#endif

	/** Deconstructor to get the components of a function call
	 * 	@param fnName	The name of the function being called
	 * 	@param argList  The argument list
	 */
	void GetComponents( std::string &, std::vector<ExprTree*> &) const;
	
	/// Make a deep copy of the expression
	virtual ExprTree* Copy( ) const;
	
    bool CopyFrom(const FunctionCall &functioncall);

    virtual bool SameAs(const ExprTree *tree) const;

    friend bool operator==(const FunctionCall &fn1, const FunctionCall &fn2);

	static void RegisterFunction(std::string &functionName, ClassAdFunc function);
	static void RegisterFunctions(ClassAdFunctionMapping *functions);

	static bool RegisterSharedLibraryFunctions(const char *shared_library_path);

	/** Returns true if the function expression points to a valid
	 *  function in the ClassAd library.
	 */
	bool FunctionIsDefined() const {return function != NULL;}

	virtual const ClassAd *GetParentScope( ) const { return( parentScope ); }

 protected:
	/// Constructor
	FunctionCall ();
	
	typedef std::map<std::string, ClassAdFunc, CaseIgnLTStr> FuncTable;
	
 private:
	virtual void _SetParentScope( const ClassAd* );
	virtual bool _Evaluate( EvalState &, Value & ) const;
	virtual bool _Evaluate( EvalState &, Value &, ExprTree *& ) const;
	virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
	
	// information common to all function calls a mapping from
	// function names (char*) to static methods We have a function to
	// return the object--it's static, and we want to make sure that
	// it's constructor has been called whenever we need to use it.
    static FuncTable &getFunctionTable(void);
	static bool		 initialized;
	
	const ClassAd *parentScope;

	// function call specific information
	std::string		functionName;
	ClassAdFunc		function;
	ArgumentList	arguments;

	
	// function implementations
	// type predicates
	static bool isType(const char*,const ArgumentList&,EvalState&,Value&);
	
	// list membership predicates
	static bool testMember(const char*,const ArgumentList&,EvalState&,
						   Value&);
	
	static bool size(const char*,const ArgumentList&,EvalState&,
					 Value&);
	static bool sumAvg(const char*,const ArgumentList&,EvalState&,
					   Value&);
	static bool minMax(const char*,const ArgumentList&,EvalState&,
					   Value&);
	static bool listCompare(const char*,const ArgumentList&,EvalState&,
							Value&);
	
	// time management (constructors)


    static bool epochTime (const char *, const ArgumentList &argList, 
                           EvalState &, Value &val);
	static bool currentTime(const char*,const ArgumentList&,EvalState&,
							Value&);
	static bool timeZoneOffset(const char*,const ArgumentList&,EvalState&,
							   Value&);
	static bool dayTime(const char*,const ArgumentList&,EvalState&,Value&);

	// time management (selectors)
	static bool getField(const char*,const ArgumentList&,EvalState&,Value&);
	static bool splitTime(const char*,const ArgumentList&,EvalState&,Value&);
    static bool formatTime(const char*,const ArgumentList&,EvalState&,Value&);

	// string management
	static bool strCat(const char*,const ArgumentList&,EvalState&,Value&);
	static bool changeCase(const char*,const ArgumentList&,EvalState&,
						   Value&);
	static bool subString(const char*,const ArgumentList&,EvalState&,
						  Value&);
	static bool compareString(const char*,const ArgumentList&,EvalState&,
						  Value&);

	static bool compareVersion( const char * name, const ArgumentList & args,
		EvalState & state, Value & result );

	static bool versionInRange( const char * name, const ArgumentList & args,
		EvalState & state, Value & result );

	// pattern matching
	static bool matchPattern(const char*,const ArgumentList&,EvalState&,
							 Value&);
	static bool substPattern( const char*,const ArgumentList &argList,EvalState &state,	Value &result );

	static bool matchPatternMember(const char*,const ArgumentList &argList,EvalState &state,
                              Value &result);

	// type conversion
	static bool convInt(const char*,const ArgumentList&,EvalState&,Value&);
	static bool convReal(const char*,const ArgumentList&,EvalState&,Value&);
	static bool convString(const char*,const ArgumentList&,EvalState&,
						   Value&);

	static bool convBool(const char*,const ArgumentList&,EvalState&,Value&);
	static bool convTime(const char*,const ArgumentList&,EvalState&,Value&);
	
	static bool unparse(const char*,const ArgumentList&,EvalState&,Value&);
	static bool hasRefs(const char*, const ArgumentList&, EvalState&, Value&);
	
	// (floor, ceil, round)
	static bool doRound(const char*,const ArgumentList&,EvalState&,Value&);
	static bool random(const char*,const ArgumentList&,EvalState&,Value&);
	// 2 argument math functions (pow, log, quantize)
	static bool doMath2(const char*,const ArgumentList&,EvalState&,Value&);

	static bool ifThenElse( const char* name,const ArgumentList &argList,EvalState &state,Value &result );
	
	static bool interval( const char* name,const ArgumentList &argList,EvalState &state,Value &result );

	static bool eval( const char* name,const ArgumentList &argList,EvalState &state,Value &result );

	static bool debug( const char* name,const ArgumentList &argList,EvalState &state,Value &result );
 	//static bool doReal(const char*,const ArgumentList&,EvalState&,Value&);
};

} // classad

#endif//__CLASSAD_FN_CALL_H__
