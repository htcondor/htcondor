#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include "exprTree.h"
#include "evalContext.h"


// this class will never be instantiated --- it only serves as an encapsulation
// of the static function table and the despatch operation defined on it.
class FunctionTable
{
	public:
		static void dispatchFunction(char*,ArgumentList*,EvalState&,Value&);

	private:
		typedef 
			void (*ClassAdFunction) (char*,ArgumentList*,EvalState&,Value&);

		struct FunctionTableEntry
		{
    		char            *functionName;
    		ClassAdFunction actionRoutine;
		};

		static FunctionTableEntry table[];
};

#endif//__FUNCTIONS_H__
