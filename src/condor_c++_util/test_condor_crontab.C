/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
