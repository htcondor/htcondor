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
	string buffer = "";

		// i1 = [7,23)
	Interval *i1 = new Interval;
	i1->lower.SetIntegerValue( 7 );
	i1->openLower = false;
	i1->upper.SetIntegerValue( 23 );
	i1->openUpper = true;
	if( !IntervalToString( i1, buffer ) ) {
		cerr << "IntervalToString failed on i1" << endl;
	}
	cout << "i1 = " << buffer << endl;
	buffer = "";

		// i2 = [12, 23]
	Interval *i2 = new Interval;
	i2->lower.SetIntegerValue( 12 );
	i2->openLower = false;
	i2->upper.SetIntegerValue( 23 );
	i2->openUpper = false;
	if( !IntervalToString( i2, buffer ) ) {
		cerr << "IntervalToString failed on i2" << endl;
	}
	cout << "i2 = " << buffer << endl;
	buffer = "";

		// i3 = [foo]
	Interval *i3 = new Interval;
	i3->lower.SetStringValue( "foo" );
	if( !IntervalToString( i3, buffer ) ) {
		cerr << "IntervalToString failed on i3" << endl;
	}
	cout << "i3 = " << buffer << endl;
	buffer = "";

		// i4 = [bar]
	Interval *i4 = new Interval;
	i4->lower.SetStringValue( "bar" );
	if( !IntervalToString( i4, buffer ) ) {
		cerr << "IntervalToString failed on i4" << endl;
	}
	cout << "i4 = " << buffer << endl;
	buffer = "";

		// i5 = [zap]
	Interval *i5 = new Interval;
	i5->lower.SetStringValue( "zap" );
	if( !IntervalToString( i5, buffer ) ) {
		cerr << "IntervalToString failed on i5" << endl;
	}
	cout << "i5 = " << buffer << endl;
	buffer = "";

		// test 1
	cout << endl;
	ValueRange vr1;

	vr1.Init( i1 );
	vr1.ToString( buffer );
	cout << "Init vr1 with [7,23): " << buffer << endl;
	buffer = "";

	vr1.Intersect( i2 );
	vr1.ToString( buffer );
	cout << "Intersect vr1 with [12,23]: " << buffer << endl;
	buffer = "";

		// test 2
	cout << endl;
	ValueRange vr2;

	vr2.Init( i3, false, true );
	vr2.ToString( buffer );
	cout << "Init vr2 with NOT[foo]: " << buffer << endl;
	buffer = "";

	vr2.Intersect( i4, false, true );
	vr2.ToString( buffer );
	cout << "Intersect vr2 with NOT[bar]: " << buffer << endl;
	buffer = "";

	vr2.Intersect( i3 );
	vr2.ToString( buffer );
	cout << "Intersect vr2 with [foo]: " << buffer << endl;
	buffer = "";

		// test 3
	cout << endl;
	ValueRange vr3;

	vr3.Init( i3, false, true );
	vr3.ToString( buffer );
	cout << "Init vr3 with NOT[foo]: " << buffer << endl;
	buffer = "";

	vr3.Intersect( i4, false, true );
	vr3.ToString( buffer );
	cout << "Intersect vr3 with NOT[bar]: " << buffer << endl;
	buffer = "";

	vr3.Intersect( i5 );
	vr3.ToString( buffer );
	cout << "Intersect vr3 with [zap]: " << buffer << endl;
	buffer = "";

	delete i1;
	delete i2;
	delete i3;
	delete i4;
	delete i5;
}
