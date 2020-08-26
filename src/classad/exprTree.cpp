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


#include "classad/common.h"
#include "classad/exprTree.h"
#include "classad/sink.h"

#ifndef WIN32
#include <sys/time.h>
#endif

using namespace std;

namespace classad {

extern int exprHash( const ExprTree* const&, int );

static const int MAX_CLASSAD_RECURSION = 1000;

void (*ExprTree::user_debug_function)(const char *) = 0;

/* static */ void 
ExprTree:: set_user_debug_function(void (*dbf)(const char *)) {
	user_debug_function = dbf;
}

void ExprTree::debug_print(const char *message) const {
	if (user_debug_function != 0) {
		user_debug_function(message);
	}
}

void ExprTree::debug_format_value(Value &value, double time) const {
		bool boolValue = false;
		long long intValue = 0;
		double doubleValue = 0;
		string stringValue = "";

			// Unparsing this of kind CLASSAD_NODE will result in the
			// entire classad, which is unnecessarily verbose
		if (CLASSAD_NODE == GetKind()) return;

		PrettyPrint	unp;
		string		buffer;
		unp.Unparse( buffer, this );

		std::string result("Classad debug: ");
		if (time) {
			char buf[24];
			snprintf(buf, sizeof(buf), "%5.5fms", time * 1000);
			result += "["; result += buf; result += "] ";
		}
		result += buffer;
		result += " --> ";

		switch(value.GetType()) {
			case Value::NULL_VALUE:
				result += "NULL\n";
				break;
			case Value::ERROR_VALUE:
				if ((FN_CALL_NODE == GetKind()) && !static_cast<const FunctionCall*>(this)->FunctionIsDefined()) {
					result += "ERROR (function is not defined)\n";
				} else if (classad::CondorErrMsg.size()) {
					result += "ERROR (" + classad::CondorErrMsg + ")\n";
				} else {
					result += "ERROR\n";
				}
				break;
			case Value::UNDEFINED_VALUE:
				result += "UNDEFINED\n";
				break;
			case Value::BOOLEAN_VALUE:
				if(value.IsBooleanValue(boolValue))
					result += boolValue ? "TRUE\n" : "FALSE\n";
				break;
			case Value::INTEGER_VALUE:
				if(value.IsIntegerValue(intValue)) {
					char buf[32];
					sprintf(buf, "%lld", intValue);
					result += buf;
					result += "\n";
				}
				break;

			case Value::REAL_VALUE:
				if(value.IsRealValue(doubleValue)) {
							char buf[24];
					sprintf(buf, "%g", doubleValue);
					result += buf;
					result += "\n";
				}
				break;
			case Value::RELATIVE_TIME_VALUE:
				result += "RELATIVE TIME\n";
				break;
			case Value::ABSOLUTE_TIME_VALUE:
				result += "ABSOLUTE TIME\n";
				break;
			case Value::STRING_VALUE:
				if(value.IsStringValue(stringValue)) {
					result += stringValue;
					result += "\n";
				}
				break;
			case Value::CLASSAD_VALUE:
				result += "CLASSAD\n";
				break;
			case Value::SCLASSAD_VALUE:
				result += "SCLASSAD\n";
				break;
			case Value::LIST_VALUE:
				result += "LIST\n";
				break;
			case Value::SLIST_VALUE:
				result += "SLIST\n";
				break;
			default:
				break;
		}
		debug_print(result.c_str());
}

bool ExprTree::
Evaluate (EvalState &state, Value &val) const
{
	double diff = 0;
#ifndef WIN32
	struct timeval begin, end;
	if (state.debug) {
		gettimeofday(&begin, NULL);
	}
#endif
	bool eval = _Evaluate( state, val );
#ifndef WIN32
	if (state.debug) {
		gettimeofday(&end, NULL);
		diff = (end.tv_sec + (end.tv_usec * 0.000001)) -
			(begin.tv_sec + (begin.tv_usec * 0.000001));
	}
#endif

	if(state.debug && GetKind() != ExprTree::LITERAL_NODE &&
			GetKind() != ExprTree::OP_NODE)
	{
		debug_format_value(val, diff);
	}

	return eval;
}

bool ExprTree::
Evaluate( EvalState &state, Value &val, ExprTree *&sig ) const
{
	double diff = 0;
#ifndef WIN32
	struct timeval begin, end;
	if (state.debug) {
		gettimeofday(&begin, NULL);
	}
#endif
	bool eval = _Evaluate( state, val, sig );
#ifndef WIN32
	if (state.debug) {
		gettimeofday(&end, NULL);
		diff = (end.tv_sec + (end.tv_usec * 0.000001)) -
			(begin.tv_sec + (begin.tv_usec * 0.000001));
	}
#endif

	if(state.debug && GetKind() != ExprTree::LITERAL_NODE &&
			GetKind() != ExprTree::OP_NODE)
	{
		debug_format_value(val, diff);
	}

	return eval;
}


void ExprTree::
SetParentScope( const ClassAd* scope ) 
{
	_SetParentScope( scope );
}


bool ExprTree::
Evaluate( Value& val ) const
{
	EvalState 	state;

	state.SetScopes( GetParentScope() );
	return( Evaluate( state, val ) );
}


bool ExprTree::
Evaluate( Value& val, ExprTree*& sig ) const
{
	EvalState 	state;

	state.SetScopes( GetParentScope() );
	return( Evaluate( state, val, sig  ) );
}


bool ExprTree::
Flatten( Value& val, ExprTree *&tree ) const
{
	EvalState state;

	state.SetScopes( GetParentScope() );
	return( Flatten( state, val, tree ) );
}


bool ExprTree::
Flatten( EvalState &state, Value &val, ExprTree *&tree, int* op) const
{
	return( _Flatten( state, val, tree, op ) );
}

bool ExprTree::isClassad(ClassAd ** ptr)
{
	bool bRet = false;
	
	
	if ( CLASSAD_NODE == GetKind() )
	{
		if (ptr){
			*ptr = (ClassAd *) this;
		}
		
		bRet = true;
	}
	
	return (bRet);
}

SAL_Ret_notnull
const ExprTree* ExprTree::self() const
{
	const ExprTree * pRet=this;
	return (pRet);
}

/* This version is for shared-library compatibility.
 * Remove it the next time we have to bump the ClassAds SO version.
 */
SAL_Ret_notnull
const ExprTree* ExprTree::self()
{
	const ExprTree * pRet=this;
	return (pRet);
}

void ExprTree::
Puke( ) const
{
	PrettyPrint	unp;
	string		buffer;

	unp.Unparse( buffer, this );
	printf( "%s\n", buffer.c_str( ) );
}


int 
exprHash( const ExprTree* const& expr, int numBkts ) 
{
	unsigned char *ptr = (unsigned char*) &expr;
	int	result = 0;
	
	for( int i = 0 ; (unsigned)i < sizeof( expr ) ; i++ ) result += ptr[i];
	return( result % numBkts );
}



EvalState::
EvalState( )
{
	rootAd = NULL;
	curAd  = NULL;

	depth_remaining = MAX_CLASSAD_RECURSION;
	flattenAndInline = false;	// NAC
	debug = false;
	inAttrRefScope = false;
}

EvalState::
~EvalState( )
{
	/*
	classad_hash_map< const ExprTree*, Value, ExprHash >::iterator i;

	for (i = cache_to_delete.begin(); i != cache_to_delete.end(); i++) {
		const ExprTree *tree = i->first;
		fprintf(stderr, "**** Deleting tree: %x\n", tree);
		delete tree;
	}

	*/	
	return;
}

void EvalState::
SetScopes( const ClassAd *curScope )
{
	curAd = curScope;
	SetRootScope( );
}


void EvalState::
SetRootScope( )
{
	const ClassAd *prevScope = curAd;
    if (curAd == NULL) {
        rootAd = NULL;
    } else {
        const ClassAd *curScope = curAd->GetParentScope();
        
        while( curScope ) {
            if( curScope == curAd ) {	// NAC - loop detection
                rootAd = NULL;
                return;					// NAC
            }							// NAC
            prevScope = curScope;
            curScope  = curScope->GetParentScope();
        }
        
        rootAd = prevScope;
    }
    return;
}

bool operator==(const ExprTree &tree1, const ExprTree &tree2)
{
    return tree1.SameAs(&tree2);
}

} // classad
