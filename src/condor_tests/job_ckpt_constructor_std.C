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


 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "x_fake_ckpt.h"

class TestObj {
public:
	TestObj( const char * );
	void display();
	void setString( const char *str );
private:
	char	*string;
};

TestObj::TestObj( const char *init )
{
	string = new char[ strlen(init) + 1 ];
	assert( string );
	strcpy( string, init );
}

void
TestObj::setString( const char *str )
{
	delete [] string;
	string = new char[ strlen(str) + 1 ];
	assert( string );
	strcpy( string, str );
}

void
TestObj::display()
{
	printf( "\"%s\"\n", string );
}

TestObj MyObj( "Hello World" );

int
main()
{
	MyObj.display();
	MyObj.setString( "Goodby World" );

	ckpt_and_exit();

	MyObj.display();

	printf( "Normal End Of Job\n" );

	exit( 0 );
	return 0;
}
