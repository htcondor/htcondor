/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" void ckpt_and_exit();

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
