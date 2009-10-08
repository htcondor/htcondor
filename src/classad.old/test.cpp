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
#include "condor_classad.h"
#include "condor_config.h"

ClassAd a;
ClassAd b;

extern "C" int SetSyscalls() { return 0; }

int main (void)
{
	EvalResult 	r;
	ExprTree   	*t;
	char		buffer[1024];
	char		buf[2048];

	config();
	while( strcmp( gets( buffer ) , "done" ) != 0 ) {
		buf[0] = '\0';
		if (ParseClassAdRvalExpr (buffer, t)) {
			cerr << "Parse error" << endl;
			exit( 1 );
		}
		t->PrintToStr( buf );
		puts( buf );
		t->EvalTree((ClassAd*)NULL,(ClassAd*)NULL,&r);
		r.fPrintResult(stdout);
		printf("\n");
	}
		
	return 0;
}
