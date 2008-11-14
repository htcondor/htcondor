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

#include "condor_crontab.h"
//#include "condor_common.h"
//#include "condor_debug.h"
//#include "extArray.h"

#include <iostream>


int main (int argc, char **argv)
{
	CronTab cronTab( "*", "*", "29", "2", "0-7/3" );
	//CronTab cronTab( "*", "*/5", "*", "*", "*" );
	//cout << "NEXT RUN: " << cronTab.nextRun() << endl;
	cronTab.nextRun();
	
	//
	// Quick test for ExtArray
	//	
	/*
	ExtArray<int> arr;
	
	arr[0] = 1;
	arr[1] = 2;
	arr[3] = 4;
	arr[4] = 5;
	arr[5] = 6;
	arr.add(7);
	arr.add(8);
	arr.set(0, 9);
	
	for (int ctr = 0, cnt = arr.getlast(); ctr <= cnt; ctr++ ) {
		cout << ctr << ") " << arr[ctr] << endl;
	}
	*/
	return (1);
}
