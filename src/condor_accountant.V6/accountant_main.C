//---------------------------------------------------------------------------
//
// Condor Accountant - Main
// Adiel Yoaz
//
//---------------------------------------------------------------------------

#include "condor_debug.h"
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_classad.h"
#include "condor_accountant.h"

//---------------------------------------------------------------------------

char* mySubSystem="ACCOUNTANT";

//---------------------------------------------------------------------------

void usage(char* name)
{
  dprintf(D_ALWAYS, "Usage: %s [-t]\n", name); 
  exit( 1 );
}

//---------------------------------------------------------------------------

int main_init(int argc, char* argv[])
{
  for(int i=1;i<argc;i++) {
    if(argv[i][0] != '-') usage(argv[0]);
  }

  Accountant accountant;

  ClassAd* AccountantClassAd=new ClassAd();
  config(AccountantClassAd);

#if 1

  //-------------------------------------------------
  // Testing
  //-------------------------------------------------

  ClassAd* Ad1=new ClassAd("Name = \"1\",StartdIpAddr = \"fred\"",',');
  ClassAd* Ad2=new ClassAd("Name = \"2\",StartdIpAddr = \"fred\"",',');
  ClassAd* Ad3=new ClassAd("Name = \"3\",StartdIpAddr = \"fred\"",',');
  ClassAd* Ad4=new ClassAd("Name = \"4\",StartdIpAddr = \"fred\"",',');
  ClassAd* Ad5=new ClassAd("Name = \"5\",StartdIpAddr = \"fred\"",',');
  ClassAd* Ad6=new ClassAd("Name = \"6\",StartdIpAddr = \"fred\"",',');

  dprintf(D_ALWAYS,"Start\n");

  accountant.AddMatch("Arieh",Ad1);
  accountant.AddMatch("Arieh",Ad2);
  accountant.AddMatch("Arieh",Ad3);
  accountant.AddMatch("Shlomo",Ad4);
  accountant.AddMatch("Shlomo",Ad5);
  accountant.AddMatch("Yuval",Ad6);

  accountant.SetPriority("Arieh",3);
  accountant.SetPriority("Shlomo",2);
  accountant.SetPriority("Yuval",1);

  accountant.UpdatePriorities();
  accountant.SavePriorities();
  // accountant.Reset();
  // accountant.SavePriorities();
  accountant.UpdatePriorities();
  accountant.LoadPriorities();
  accountant.UpdatePriorities();

  sleep(10);
  
  // while(1) { sleep(1); accountant.UpdatePriorities(); }

#endif

  return TRUE;
} 

//---------------------------------------------------------------------------

int main_config()
{
  return TRUE;
}

//---------------------------------------------------------------------------

int main_shutdown_fast()
{
  return TRUE;
}

//---------------------------------------------------------------------------

int main_shutdown_graceful()
{
  return TRUE;
}
