#include "signal2.h"
#include "constants2.h"
#include <iostream.h>


void BlockSignal(int sig)
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot obtain current signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigaddset(&mask, sig) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot add signal (" << sig << ") to signal mask" 
	   << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot set new signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
}


void UnblockSignal(int sig)
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot obtain current signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigdelset(&mask, sig) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot remove signal (" << sig << ") from signal mask" 
	   << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot set new signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
}


