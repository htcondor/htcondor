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
#include "condor_common.h"
#include "AcctDaemon.h"

int Accountant::Init(DaemonCore* DC)
{
  Core=DC;
  
  // Register message handlers
  core->Register(this, NEGOTIATE, (void*)Negotiate, REQUEST);
  
  // Register signal handlers
  core->Register(this, SIGCHLD, (void*)SigChldHandler, SIGNAL);
  core->Register(this, SIGINT, (void*)SigIntHandler, SIGNAL);
  core->Register(this, SIGHUP, (void*)SigHupHandler, SIGNAL);
  core->Register(this, SIGQUIT, (void*)SigQuitHandler, SIGNAL);
  core->Register(this, SIGTERM, (void*)SigTermHandler, SIGNAL);

  install_sig_handler(SIGPIPE, SIG_IGN); // Ignore SIGPIPE
  
  return 0;
}

void Accountant::Negotiate(ReliSock* s, struct sockaddr_in* sin)
{
}

//------------------------------------------------
// Signal Handlers
//------------------------------------------------

void Accountant::SigChldHandler()
{
  dprintf(D_ALWAYS,"SIGCHLD recieved.\n");
}

void Accountant::SigIntHandler()
{
  dprintf(D_ALWAYS,"SIGINT recieved.\n");
  exit(0);
}

void Accountant::SigHupHandler()
{
  dprintf(D_ALWAYS,"SIGCHLD recieved.\n");
  
  // config( ad );
  // Init();
  // ::Init();
  // timeout();
}

void Accountant::SigQuitHandler()
{
  dprintf(D_ALWAYS,"SIGQUIT recieved.\n");
}

void Accountant::SigTermHandler()
{
  dprintf(D_ALWAYS,"SIGTERM recieved.\n");
}
