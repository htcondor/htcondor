//******************************************************************************
// AcctDaemon.h
//
// Yoaz Adiel
//
// Defines Accountant class. This class gathers information and calculate
// priorities base on the information gathered. Info class collects statistics
// of the pool.
//
//******************************************************************************

#ifndef _AcctDaemon_H_
#define _AcctDaemon_H_

#include "condor_daemon_core.h"

class Accountant {

public:

  Init(DaemonCore* DC);
  void SigChldHandler();
  void SigIntHandler();
  void SigHupHandler();
  void SigQuitHandler();
  void SigTermHandler();

private:

  DaemonCore* Core;
  
};

#endif
