/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

#ifndef __FN_CALL_H__
#define __FN_CALL_H__

#include <map>
#include <vector>
#include "classad.h"
#include "classad_features.h"

BEGIN_NAMESPACE( classad )

typedef std::vector<ExprTree*> ArgumentList;

typedef struct 
{
	std::string       functionName;

	// The function should be a ClassAdFunc. Because we use this structure
	// as part of the interface with a shared library that uses an extern
	// C interface, we keep it as a void*, unfortunately.
	// The onus is on you to make sure that it's really a ClassAdFunc!
	void         *function;
	
	// We are likely to add some flags here in the future.  These
	// flags will describe things like how to cache results of the
	// function. Currently they are unused, so set it to zero. 
	int          flags;
} ClassAdFunctionMapping;

#ifdef ENABLE_SHARED_LIBRARY_FUNCTIONS
// If you create a shared library, you have to provide a function
// named "Initialize" that conforms to this prototype. You will return
// a table of function mappings.  The last mapping in the table should
// have a NULL function pointer. It will be ignored.
typedef	 ClassAdFunctionMapping *(*ClassAdSharedLibraryInit)(void);
#endif

/// Node of the expression which represents a call to an inbuilt
//function
class FunctionCall : public ExprTree
{
 public:
 	typedef	bool(*ClassAdFunc)(const char*, const ArgumentList&, EvalState&,
							   Value&);

    /// Copy Constructor
    FunctionCall(FunctionCall &functioncall);

	/// Destructor
	~FunctionCall ();
	
    FunctionCall & operator=(FunctionCall &functioncall);

	/** Factory method to make a function call expression
	 * 	@param fnName	The name of the function to be called
	 * 	@param argList	A vector representing the argument list
	 * 	@return The constructed function call expression
	 */
	static FunctionCall *MakeFunctionCall( const std::string &fnName, 
										   std::vector<ExprTree*> &argList );
	
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

#ifdef ENABLE_SHARED_LIBRARY_FUNCTIONS
	static bool RegisterSharedLibraryFunctions(const char *shared_library_path);
#endif

 protected:
	/// Constructor
	FunctionCall ();
	
	typedef std::map<std::string, void*, CaseIgnLTStr > FuncTable;
	
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
	
	// sum, average and bounds (max and min)
	/*
	  static bool sumAvgFrom(const char*,const ArgumentList&,EvalState&,
	  Value&);
	  static bool boundFrom(const char*,const ArgumentList&,EvalState&,
	  Value&);
	*/
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
	static bool makeTime(const char*,const ArgumentList&,EvalState&,Value&);
	static bool makeDate(const char*,const ArgumentList&,EvalState&,Value&);
	// time management (selectors)
	static bool getField(const char*,const ArgumentList&,EvalState&,Value&);
	static bool splitTime(const char*,const ArgumentList&,EvalState&,Value&);
    static bool formatTime(const char*,const ArgumentList&,EvalState&,Value&);
	// time management (conversions)
	static bool inTimeUnits(const char*,const ArgumentList&,EvalState&,
							Value&);

	// string management
	static bool strCat(const char*,const ArgumentList&,EvalState&,Value&);
	static bool changeCase(const char*,const ArgumentList&,EvalState&,
						   Value&);
	static bool subString(const char*,const ArgumentList&,EvalState&,
						  Value&);
	static bool compareString(const char*,const ArgumentList&,EvalState&,
						  Value&);
	
	// pattern matching
	static bool matchPattern(const char*,const ArgumentList&,EvalState&,
							 Value&);
	
	// type conversion
	static bool convInt(const char*,const ArgumentList&,EvalState&,Value&);
	static bool convReal(const char*,const ArgumentList&,EvalState&,Value&);
	static bool convString(const char*,const ArgumentList&,EvalState&,
						   Value&);

	static bool convBool(const char*,const ArgumentList&,EvalState&,Value&);
	static bool convTime(const char*,const ArgumentList&,EvalState&,Value&);
	
	// math (floor, ceil, round)
	static bool doMath(const char*,const ArgumentList&,EvalState&,Value&);
	static bool random(const char*,const ArgumentList&,EvalState&,Value&);
	
 	//static bool doReal(const char*,const ArgumentList&,EvalState&,Value&);
};

END_NAMESPACE // classad

#endif//__FN_CALL_H__
