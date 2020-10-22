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
#include "analysis.h"

int main( ) {
	std::string buffer = "";

		// i1 = [7,23)
	Interval *i1 = new Interval;
	i1->lower.SetIntegerValue( 7 );
	i1->openLower = false;
	i1->upper.SetIntegerValue( 23 );
	i1->openUpper = true;
	if( !IntervalToString( i1, buffer ) ) {
		std::cerr << "IntervalToString failed on i1" << std::endl;
	}
	std::cout << "i1 = " << buffer << std::endl;
	buffer = "";

		// i2 = [12, 23]
	Interval *i2 = new Interval;
	i2->lower.SetIntegerValue( 12 );
	i2->openLower = false;
	i2->upper.SetIntegerValue( 23 );
	i2->openUpper = false;
	if( !IntervalToString( i2, buffer ) ) {
		std::cerr << "IntervalToString failed on i2" << std::endl;
	}
	std::cout << "i2 = " << buffer << std::endl;
	buffer = "";

		// i3 = [foo]
	Interval *i3 = new Interval;
	i3->lower.SetStringValue( "foo" );
	if( !IntervalToString( i3, buffer ) ) {
		std::cerr << "IntervalToString failed on i3" << std::endl;
	}
	std::cout << "i3 = " << buffer << std::endl;
	buffer = "";

		// i4 = [bar]
	Interval *i4 = new Interval;
	i4->lower.SetStringValue( "bar" );
	if( !IntervalToString( i4, buffer ) ) {
		std::cerr << "IntervalToString failed on i4" << std::endl;
	}
	std::cout << "i4 = " << buffer << std::endl;
	buffer = "";

		// i5 = [zap]
	Interval *i5 = new Interval;
	i5->lower.SetStringValue( "zap" );
	if( !IntervalToString( i5, buffer ) ) {
		std::cerr << "IntervalToString failed on i5" << std::endl;
	}
	std::cout << "i5 = " << buffer << std::endl;
	buffer = "";

		// test 1
	std::cout << std::endl;
	ValueRange vr1;

	vr1.Init( i1 );
	vr1.ToString( buffer );
	std::cout << "Init vr1 with [7,23): " << buffer << std::endl;
	buffer = "";

	vr1.Intersect( i2 );
	vr1.ToString( buffer );
	std::cout << "Intersect vr1 with [12,23]: " << buffer << std::endl;
	buffer = "";

		// test 2
	std::cout << std::endl;
	ValueRange vr2;

	vr2.Init( i3, false, true );
	vr2.ToString( buffer );
	std::cout << "Init vr2 with NOT[foo]: " << buffer << std::endl;
	buffer = "";

	vr2.Intersect( i4, false, true );
	vr2.ToString( buffer );
	std::cout << "Intersect vr2 with NOT[bar]: " << buffer << std::endl;
	buffer = "";

	vr2.Intersect( i3 );
	vr2.ToString( buffer );
	std::cout << "Intersect vr2 with [foo]: " << buffer << std::endl;
	buffer = "";

		// test 3
	std::cout << std::endl;
	ValueRange vr3;

	vr3.Init( i3, false, true );
	vr3.ToString( buffer );
	std::cout << "Init vr3 with NOT[foo]: " << buffer << std::endl;
	buffer = "";

	vr3.Intersect( i4, false, true );
	vr3.ToString( buffer );
	std::cout << "Intersect vr3 with NOT[bar]: " << buffer << std::endl;
	buffer = "";

	vr3.Intersect( i5 );
	vr3.ToString( buffer );
	std::cout << "Intersect vr3 with [zap]: " << buffer << std::endl;
	buffer = "";

	delete i1;
	delete i2;
	delete i3;
	delete i4;
	delete i5;
}
