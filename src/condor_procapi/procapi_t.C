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
#include "procapi_t.h"
#include <strings.h>

bool verbose;
bool monitor;
int main(int argc, char* argv[])
{
	int success;
	verbose = false;

	if (argc > 1)
	{
	  if(strcmp(argv[1], "-v") == 0){
	    verbose = true;
	  }
	  else if(strcmp(argv[1], "-m") == 0){
	    monitor = true;
	  }
	  else{
	    printf("invalid command line argument");
	    exit(1);
	  }
	}
	if (argc > 2)
	{
	  if(strcmp(argv[2], "-v") == 0){
	    verbose = true;
	  }
	  else if(strcmp(argv[2], "-m") == 0){
	    monitor = true;
	  }
	  else{
	    printf("invalid command line argument");
	    exit(1);
	  }
	}

	printf("Performing the ProcAPI test...\n");
	if (verbose == true){
	  printf("running in verbose mode\n");
	}
	if(monitor){
	  char jobname[512];
	  printf("What is the path of the program you would like to monitor? ");
	  scanf("%s", jobname);
	  printf("\n");
	  test_monitor(jobname);
	  // well never get here
	  exit(0);
	}


	
	success = run_tests();  
	
	if(success == 1){
	  printf("Tests completed sucessfully\n");
	  exit(EXIT_SUCCESS);
	}
	else{
	  printf("Tests failed\n");
	  exit(1);
	}	
}


/* Mike Yoder's old testing code... */
int run_tests(void) {

    if(verbose){
    printf ( "Test | Description\n" );
    printf ( "----   -----------\n" );
    printf ( " 1     Simple fork; monitor processes & family.\n" );
    printf ( " 2     Complex fork; monitor family.\n" );
    printf ( " 3     Determines if you can look at procs you don't own.\n");
    printf ( " 4 n/a Tests procSetInfo...asks for pids, returns info.\n");
    printf ( " 5     Tests getPidFamily...forks kids & finds them again.\n");
    printf ( " 6     Tests cpu usage over time.\n");
    printf ( " 7 n/a Fork a process; monitor it.  There's no return.\n\n");
    }


    int success = 1;
    int temp = 1;
    
    //performing the getProcInfo test/////////
    bool test_success = true;
    printf("Performing test 1\n");
    for(int i = 0; i < PROC_INFO_NUMTIMES; i++){
      temp = getProcInfo_test(verbose);
      if(temp < 0){
	printf("test 1 failed in trial %d\n", i);
	success = temp;
	test_success = false;
      }
    }
    if(test_success){
      printf("test 1 successfully completed %d trials!\n",PROC_INFO_NUMTIMES );
    }
    else{
      printf("test 1 failed!\n");
    }
    

    //performing the getFamilyInfo test/////////
    test_success = true;
    
    printf("Performing test 2\n");
    for(int i = 0; i < FAMILY_INFO_NUMTIMES; i++){
      temp = getFamilyInfo_test(verbose);
      
      if(temp < 0){
	printf("test 2 failed in trial %d\n", i);
	success = temp;
	test_success = false;
      }
    }
    if(test_success){
      printf("test 2 successfully completed %d trials!\n",FAMILY_INFO_NUMTIMES );
    }
    else{
      printf("test 2 failed!\n");
    }

    //performing the permissions test/////////
    test_success = true;
    
    printf("Performing test 3\n");
    for(int i = 0; i < PERM_NUMTIMES; i++){
      temp = permission_test(verbose);
      
      if(temp < 0){
	printf("test 3 failed in trial %d\n", i);
	success = temp;
	test_success = false;
      }
    }
    if(test_success){
      printf("test 3 successfully completed %d trials!\n",PERM_NUMTIMES );
    }
    else{
      printf("test 3 failed!\n");
    }

    // performing the getProcSetInfo test////////////
    test_success = true;

    printf("Performing test 4\n");
    for(int i = 0; i < SET_INFO_NUMTIMES; i++){
      temp = getProcSetInfo_test(verbose);
	
      if(temp < 0){
	printf("test 4 failed in trial %d\n", i);
	success = temp;
	test_success = false;
      }
    }
    if(test_success){
      printf("test 4 successfully completed %d trials!\n",SET_INFO_NUMTIMES );
    }
    else{
      printf("test 4 failed!\n");
    }
    //performing the getPidFamily test/////////////
    test_success = true;

    printf("Performing test 5\n");
    for(int i = 0; i < PID_FAMILY_NUMTIMES; i++){
      temp = getPidFamily_test(verbose);
      
      if(temp < 0){
	printf("test 5 failed in trial %d\n", i);
	success = temp;
	test_success = false;
      }
    }
    if(test_success){
      printf("test 5 successfully completed %d trials!\n",PID_FAMILY_NUMTIMES );
    }
    else{
      printf("test 5 failed!\n");
    }
    // performing the cpuusage test//////////
    test_success = true;

    printf("Performing test 6\n");
    for(int i = 0; i < CPU_USAGE_NUMTIMES; i++){
      temp = cpuusage_test(verbose);

      if(temp < 0){
	printf("test 6 failed in trial %d\n", i);
	success = temp;
	test_success = false;
      }
    }
    if(test_success){
      printf("test 6 successfully completed %d trials!\n",CPU_USAGE_NUMTIMES );
    }
    else{
      printf("test 6 failed!\n");
    }
    
    return success;
	
}




// Test monitor basically watches an executable of the name passed to it 
// and periodically prints out information about it.  It currently performs 
// no checks to ensure the information it retrieves is correct.



/////////////////////////////test monitor//////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void test_monitor ( char * jobname ) {
  pid_t child;
  int rval;
  piPTR pi = NULL;
  int i;
  int status;
  PidEnvID penvid;

  pidenvid_init(&penvid);

  printf ( "Here's the interesting test.  This one does a fork()\n");
  printf ( "and then an exec() of the name of the program passed to\n");
  printf ( "it.  In this case, that's %s.\n", jobname );
  printf ( "This monitoring program will wake up every 10 seconds and\n");
  printf ( "spit out a report.\n");

  child = fork();
  
  if ( !child ) { // in child
    rval = execl( jobname, jobname, (char*)0 );
    if ( rval < 0 ) {
      perror ( "Exec problem:" );
      exit(0);
    }
  }

  for(i = 0; i < 9; i++) {
    sleep ( 10  );
    if ( ProcAPI::getFamilyInfo( child, pi, &penvid, status ) 
								== PROCAPI_FAILURE) 
	{
      printf ( "Problem getting information.  Exiting.\n");
      exit(1);
    }
    else {
      ProcAPI::printProcInfo ( pi );
    }
  }

  delete pi;  // we'll never get here, but oh well...:-)

}



