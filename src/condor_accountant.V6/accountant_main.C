//---------------------------------------------------------------------------
//
// Condor Accountant - Main
// Adiel Yoaz
//
//---------------------------------------------------------------------------

#include "condor_common.h"
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
  // config(AccountantClassAd);

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

  accountant.AddMatch("Arieh",Ad1);
  accountant.AddMatch("Yuval",Ad2);
  accountant.AddMatch("Arieh",Ad3);
  accountant.AddMatch("Shlomo",Ad4);
  accountant.AddMatch("Yuval",Ad5);
  accountant.AddMatch("Yuval",Ad6);

  // accountant.UpdatePriorities();

  // accountant.SetPriority("Arieh",3);
  // accountant.SetPriority("Shlomo",2);
  // accountant.SetPriority("Yuval",1);

  // for(int i=0;i<5; i++) { sleep(1); accountant.UpdatePriorities(); }

  // while(1) accountant.AddMatch("Yuval",Ad6);

  exit(0);

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

int main(int argc, char* argv[]) { return main_init(argc, argv); }

