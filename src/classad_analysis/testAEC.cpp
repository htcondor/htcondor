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


#include "condor_common.h"
#include "conversion.h"

//void usage( char *myName );

int main ( int argc, char *argv[] )
{
	classad::ClassAdParser parser;
	classad::PrettyPrint pp;
	std::string buffer = "";
	const int NUM_EXPRS = 10;
	std::vector< std::string > rankStrings( NUM_EXPRS );
	std::vector< classad::ExprTree * > rankExprs( NUM_EXPRS );

		// Real Rank expressions
	rankStrings[0] = "0";
	rankStrings[1] = "( IsCommittedJob is true )";
	rankStrings[2] = "( Owner == \"amir\" || Owner == \"butts\" || Owner == \"glew\" || Owner == \"harit\" || Owner == \"sodani\" || Owner == \"zilles\" )";
	rankStrings[3] = "Owner == \"amir\" || Owner == \"butts\" || Owner == \"glew\" || Owner == \"harit\" || Owner == \"sodani\" || Owner == \"zilles\"";
	rankStrings[4] = "( ( Owner == \"amir\" ) || ( Owner == \"zilles\" ) || ( Owner == \"butts\" ) || ( Owner == \"param\" ) || ( Owner == \"saisanth\" ) ) * 100";
	rankStrings[5] = "( Owner == \"ferris\" && ClusterId == 64430 && ProcId == 0 && Cmd == \"/local.wheel/seymour/twohour\" )";
	rankStrings[6] = "Scheduler is \"DedicatedScheduler@raven.cs.wisc.edu\"";

		// Test expressions
	rankStrings[7] = "( a == 4 ) * ( b < 34 || b > 78 ) * foo + ( bar - 2 ) + 6";
	rankStrings[8] = "attribute";
	rankStrings[9] = "booleanAttr + ( nonBooleanAttr == 20 )";

	for( int i = 0; i < NUM_EXPRS; i++ ) {
		rankExprs[i] = parser.ParseExpression( rankStrings[i] );
		cout << "TEST " << i << endl;
		cout << "------" << endl;
		pp.Unparse( buffer, rankExprs[i] );
		cout << "rankExprs[" << i << "] = " << buffer << endl;
		buffer = "";
		classad::ExprTree *newExpr = AddExplicitConditionals( rankExprs[i] );
		if( newExpr == NULL ) {
			cout << "AddExplicitConditionals returned NULL" << endl;
		}
		else {
			pp.Unparse( buffer, newExpr );
			cout << "result = " << buffer << endl;
			cout << endl;
			buffer = "";
		}
	}
}
