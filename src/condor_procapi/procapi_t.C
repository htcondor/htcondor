#include "condor_common.h"
#include "procapi.h"
#include <stdio.h>

/* This is a ProcApi test program for NT.  It can also be used in Unix... */

/* This quick-n-dirty tester is best used with perfmon, the 
   really cool performance tool provided with windows.  Use 
   it to determine pids in the system ("ID Process") and then
   check some of the results against what it says. */

// how many children to you want to create?  5 is a good number for me.
const int NUMKIDS = 5;

// when each child is created, it allocates some memory.  The first
// child allocates "MEMFACTOR" Megs.  The second allocates MEMFACTOR *
// 2 Megs.  etc, etc.  Be careful of thrashing...unless that's what
// you want.
const int MEMFACTOR = 3;

static void run_tests(void);
static int nt_test(void);
static void test1(void);
static void test2(void);
static void test3(void);
static void test4(void);
static void test5(void);
static void test6(void);
static void test7(void);
static void sprog1(pid_t *childlist, int numkids, int f);
static void sprog2(int numkids, int f);
static void test_monitor(char *jobname);

int main(void)
{
	printf("Performing the NT/UNIX test...\n");

/*	nt_test();*/
	run_tests();

	exit(EXIT_SUCCESS);
}

int nt_test(void)
{ 
	piPTR pi = NULL;
	pid_t pids[3];

// Use this for getProcSetInfo()...
//	printf ( "Feed me three pids\n" );
//	scanf ( "%d %d %d", &pids[0],  &pids[1],  &pids[2] );

// Use this for single pids:
	printf ( "Give me a parent pid:\n" );
	scanf ( "%d", &pids[0] );

	while (1) {
			
//		ProcAPI::getProcInfo( pids[0], pi );
		ProcAPI::getFamilyInfo( pids[0], pi );
//		ProcAPI::getProcSetInfo( pids, 3, pi );

		ProcAPI::printProcInfo ( pi );
		printf ( "\n" );

// Use this for testing less-than-one-second sampling intervals:
//		for ( int i=0 ; i<1000000 ; i++ )
//			;

	// Otherwise you can use this:
	sleep (5);
	}

	return 0;
}


/* Mike Yoder's old testing code... */
void run_tests(void) {

    printf ( "Test | Description\n" );
    printf ( "----   -----------\n" );
    printf ( " 1     Simple fork; monitor processes & family.\n" );
    printf ( " 2     Complex fork; monitor family.\n" );
    printf ( " 3     Determines if you can look at procs you don't own.\n");
    printf ( " 4 n/a Tests procSetInfo...asks for pids, returns info.\n");
    printf ( " 5     Tests getPidFamily...forks kids & finds them again.\n");
    printf ( " 6     Tests cpu usage over time.\n");
    printf ( " 7 n/a Fork a process; monitor it.  There's no return.\n\n");

	printf("Performing test 1\n");
	test1();
	sleep(5);
	printf("Performing test 2\n");
	test2();
	sleep(5);
	printf("Performing test 3\n");
	test3();
	sleep(5);
/*	printf("Performing test 4\n");*/
/*	test4();*/
/*	sleep(5);*/
	printf("Performing test 5\n");
	test5();
	sleep(5);
	printf("Performing test 6\n");
	test6();
	sleep(5);
	printf("Performing test 7\n");
/*	test7();*/
/*	sleep(5);*/

/*    printf ( " 8     Exit.\n\n");*/
/*    printf ( "Please enter your choice: " );*/
/*    fflush ( stdout );*/
/*    scanf ( "%d", &ans );*/

	/* figure out test_monitor stuff */
/*      printf ("Enter executable name:\n");*/
/*      scanf ("%s", file);*/
/*      test_monitor( file );*/

	printf("Done.\n");

}

void sprog1 ( pid_t *childlist, int numkids, int f ) {
  
  pid_t child;
  int i, j;
  char **big;

  for ( i = 0; i < numkids ; i++ ) {
    child = fork();
    if ( !child ) {  // in child 
      printf("Child process # %d started....\n", i );
      fflush (stdout);

      big = new char*[1024*(i+1)*f];
      for ( j = 0 ; j < 1024*(i+1)*f ; j++ ) {
        big[j] = new char[1024];
        if ( big[j] == NULL ) {
          printf("Shit, new failed in child %i.  j=%i\n", i, j);
        }
      }

      printf ( "Child %d done allocating %d megs of memory.\n", i, (i+1)*f );

      for ( j=0 ; j < 1024*(i+1)*f ; j++ ) {
        big[j][0] = 'x';
      }

      printf ( "Done touching all pages in child %d\n", i );

      // let's do some work in the child, to get the cpu time up:
      for ( j=0 ; j < 100000000 ; j++ ) 
        ;

      printf ( "Done doing meaningless work in child %d\n", i );

      sleep ( 60 );   // take a nap for one minute - allows measurement.

      // now we deallocate what we allocated, to be nice.  :-)
      for ( j = 0 ; j < 1024*(i+1)*f ; j++ ) {  // deallocation
        delete [] big[j];
      }
      delete [] big;

      exit(1);
    }
    else {
      printf("In parent.  Child process #%d (pid=%d) created.\n", i, child );
      childlist[i] = child;
    }
  }
}

void sprog2( int numkids, int f ) {

  pid_t child;
  char **big;
  int i = numkids;
  int j;
  
  if ( numkids == 0 )   // bail out when we're done. (stop the recursion!)
    return;

  child = fork();
  if ( !child ) { // in child
    sprog2 ( numkids-1, f );

    big = new char*[1024*i*f];
    for ( j = 0 ; j < 1024*i*f ; j++ ) {
      big[j] = new char[1024];
      if ( big[j] == NULL ) {
        printf("Shit, new failed in child %i.  j=%i\n", i, j);
      }
    }

    printf ( "Child %d done allocating %d megs of memory.\n", i, i*f );
    
    for ( j=0 ; j < 1024*i*f ; j++ ) {
      big[j][0] = 'x';
    }
    
    printf ( "Done touching all pages in child %d\n", i );
    
    // let's do some work in the child, to get the cpu time up:
    for ( j=0 ; j < 100000000 ; j++ ) 
      ;
    
    printf ( "Done doing meaningless work in child %d\n", i );
    
    sleep ( 60 );   // take a nap for one minute - allows measurement.
    
    // now we deallocate what we allocated, to be nice.  :-)
    for ( j = 0 ; j < 1024*i*f ; j++ ) {  // deallocation
      delete [] big[j];
    }
    delete [] big;
    
    exit(1);
  }
}

void test1(void) {

  pid_t children[NUMKIDS];
  piPTR pi = NULL;

  printf ( "\n..................................\n" );
  printf ( "This first test demonstrates the creation and monitoring of\n" );
  printf ( "a simple family of processes.  This process forks off %d copies\n",
           NUMKIDS );
  printf ( "of itself, and each of these children allocate some memory,\n" );
  printf ( "touch each page of it, do some cpu-intensive work, and then\n" );
  printf ( "deallocate the memory they allocated. They then sleep for a\n" );
  printf ( "minute and exit.  These children are monitored individually\n" );
  printf ( "and as a family of processes.\n\n" );

  sprog1 ( children, NUMKIDS, MEMFACTOR );

  printf ( "Parent:  sleeping 30 secs to let kids play awhile.\n" );
  // take a break to let children work
  for ( int i=0 ; i < 30 ; i+=5 ) {
    printf ( "." );
    fflush(stdout);
    sleep ( 5 );
  }

  printf ( "\n" );

  for ( int i=0 ; i < NUMKIDS ; i++ ) {
    printf ( "Info for Child #%d, pid=%d...", i, children[i] );
    printf ( "should have allocated %d Megs.\n", (i+1)*MEMFACTOR );
    ProcAPI::getProcInfo ( children[i], pi );
    ProcAPI::printProcInfo( pi );
  }
  printf ( "Parental info: pid:%d\n", getpid() );
  ProcAPI::getProcInfo ( getpid(), pi );
  ProcAPI::printProcInfo( pi );

  printf ( "Now get the information of this pid and all its descendents:\n" );
  printf ( "Total allocated memory should be around %d Megs.\n", 
           (NUMKIDS*(NUMKIDS+1))/2 * MEMFACTOR );

  printf ( "Family info for pid: %d\n", getpid() );
  if ( ProcAPI::getFamilyInfo( getpid(), pi ) < 0 )
    printf ( "Unable to get info.  That shouldn't have happened.\n");
  else
    ProcAPI::printProcInfo( pi );

  delete pi;
}

void test2(void) {
  piPTR pi = NULL;
  pid_t child;

  printf ( "\n....................................\n" );
  printf ( "This test is similar to test 1, except that the children are\n");
  printf ( "forked off in a different way.  In this case, the parent forks\n");
  printf ( "a child, which forks a child, which forks a child, etc.  It\n");
  printf ( "should still be possible to monitor this group by getting the\n");
  printf ( "family info for the parent pid.  Let's see...\n\n");

  child = fork();
  if ( !child ) { // in child 
    sprog2 ( NUMKIDS, 1 );
    sleep(60);
    exit(1);
  }

  printf ( "Parent:  sleeping 30 secs to let kids play awhile.\n" );
  // take a break to let children work
  for ( int i=0 ; i < 30 ; i+=5 ) {
    printf ( "." );
    fflush(stdout);
    sleep ( 5 );
  }
  printf ( "\n" );
  
  printf ( "Total allocated memory should be around %d Megs.\n", 
           (NUMKIDS*(NUMKIDS+1))/2 );

  printf ( "Family info for pid: %d\n", child );
  if ( ProcAPI::getFamilyInfo( child, pi ) < 0 )
    printf ( "Unable to get info.  That shouldn't have happened.\n");
  else
    ProcAPI::printProcInfo( pi );
  
  delete pi;  
}

void test3(void) {

  piPTR pi = NULL;

  printf ( "\n..................................\n" );
  printf ( "This test determines if you can get information on processes\n" );
  printf ( "other than those you own.  If you get a summary of all the\n" );
  printf ( "processes in the system ( parent=1 ) then you can.\n\n" );

  if ( ProcAPI::getFamilyInfo( 1, pi ) < 0 )
    printf ( "Unable to get info.  Try becoming root.  :-)\n");
  else
    ProcAPI::printProcInfo( pi );

  delete pi;
}

void test4(void) {

  piPTR pi = NULL;

  int numpids;
  pid_t *pids;

  printf ( "\n..................................\n" );
  printf ( "Test 4 is a quick test of getProcSetInfo().  First enter the\n");
  printf ( "number of processes to look at, then individual pids.\n");
  printf ( "# pids -> " );
  scanf ( "%d", &numpids );
  printf ( "\nPids -> " );

  pids = new pid_t[numpids];
  for ( int i=0 ; i < numpids ; i++ ) {
    scanf ( "%d", &pids[i] );
  }

  ProcAPI::getProcSetInfo ( pids, numpids, pi );
  
  printf ( "\n\nThe totals are:\n" );
  ProcAPI::printProcInfo ( pi );

  delete pi;
  delete [] pids;
}

void test5(void) {
  
  piPTR pi = NULL;

  printf ( "\n..................................\n" );
  printf ( "This is a quick test of getPidFamily().  It forks off some\n" );
  printf ( "processes and it'll print out the pids associated with\n" );
  printf ( "these children.\n" );

  int child;

  child = fork();
  if ( !child ) { // in child
    printf ( "Child %d created.\n", getpid() );    
    child = fork();
    if ( !child ) { // in child
      printf ( "Child %d created.\n", getpid() );    
      child = fork();
      if ( !child ) { // in child
        printf ( "Child %d created.\n", getpid() );    
        sleep(20);
        exit(1);
      }
      else {
        sleep(20);
        exit(1);
      }
    }
    else {
      sleep(20);
      exit(1);
    }
  }
  else { // in parent
    sleep (10);
    pid_t *pidf = new pid_t[20];
    ProcAPI::getPidFamily( getpid(), pidf );
    printf ( "Result of getPidFamily...the descendants are:\n" );

    for ( int i=0 ; pidf[i] != 0 ; i++ ) {
      printf ( " %d ", pidf[i] );
    }

	delete [] pidf;

    printf ( "\n" );

    printf ( "If the above results didn't match, I f*cked up.\n" );
  }
}

void test6(void) {

  piPTR pi = NULL;
  pid_t child;

  printf ( "\n..................................\n" );
  printf ( "Here's the test of cpu usage monitoring over time.  I'll\n");
  printf ( "fork off a process, which will alternate between sleeping\n");
  printf ( "and working.  I'll return info on that process.\n");
  printf ( "This test runs for one minute.\n");

  child = fork();
  if ( !child ) { // in child
    for ( int i=0 ; i<4 ; i++ ) {
      char * poo = new char[1024*1024];
      printf ( "Child working.\n");
      for ( int j=0 ; j < 200000000 ; j++ )
        ;
      printf ( "Child sleeping.\n");
      sleep(10);
    }
    exit(1);
  }
  else {
    for ( int i=0 ; i<12 ; i++ ) {
      sleep(5);
      printf ("\n");
      ProcAPI::getProcInfo ( child, pi );
      ProcAPI::printProcInfo ( pi );
    }
  }
}

void test_monitor ( char * jobname ) {
  pid_t child;
  int rval;
  piPTR pi = NULL;

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

  while ( 1 ) {
    sleep ( 10  );
    if ( ProcAPI::getFamilyInfo( child, pi ) < 0 ) {
      printf ( "Problem getting information.  Exiting.\n");
      exit(1);
    }
    else {
      ProcAPI::printProcInfo ( pi );
    }
  }

  delete pi;  // we'll never get here, but oh well...:-)

}
