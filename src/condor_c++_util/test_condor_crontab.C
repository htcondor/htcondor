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
