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
