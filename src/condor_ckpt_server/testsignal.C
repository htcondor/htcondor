/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include <iostream.h>
#include <signal.h>


extern "C" int pause();
extern "C" int sigaction();


typedef void (*SIG_HANDLER)();


void sighandler(int sig_num);


int main(void)
{
  struct sigaction usr1, usr2, temp;

  usr1.sa_handler = (SIG_HANDLER) sighandler;
  sigemptyset(&usr1.sa_mask);
  usr1.sa_flags = 0;
  usr2.sa_handler = (SIG_HANDLER) sighandler;
  sigemptyset(&usr2.sa_mask);
  usr2.sa_flags = 0;
  if (sigaction(SIGUSR1, &usr1, &temp) < 0)
    {
      cerr << "sigaction() error" <<endl;
      exit(-1);
    }
  if (sigaction(SIGUSR2, &usr2, &temp) < 0)
    {
      cerr << "sigaction() error" <<endl;
      exit(-1);
    }
  while (1)
    pause();
}


void sighandler(int sig_num)
{
  if (sig_num == SIGUSR1)
    cout << "SIGUSR1 signal received" << endl;
  else if (sig_num == SIGUSR2)
    cout << "SIGUSR2 signal received" << endl;
  else
    cout << "Bogus crap received" << endl;
}

