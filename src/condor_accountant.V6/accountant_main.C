//////////////////////////////////////////////////////////////////////////////
//
//
//
// Adiel Yoaz
//
//////////////////////////////////////////////////////////////////////////////

#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "condor_classad.h"
#include "Accountant.h"

// extern int Termlog;

void usage(char* name)
{
  dprintf(D_ALWAYS, "Usage: %s [-t]", name); 
  exit( 1 );
}

main(int argc, char* argv[])
{
  int Termlog=1;

  for(int i=1;i<argc;i++) {
    if(argv[i][0] != '-') usage(argv[0]);
    switch(argv[i][1]) {
    case 't':
      Termlog = 1;
      break;
    }
  }

  // DaemonCore core(10,10,10);
  Accountant accountant(100,100);

  //----------------------------------------------
  // Test
  //----------------------------------------------

  ClassAd* Ad1=new ClassAd("Name = \"XXX\",StartdIpAddr = \"1.2.3\"",',');
  ClassAd* Ad2=new ClassAd("Name = \"XXX\",StartdIpAddr = \"2.2.3\"",',');
  ClassAd* Ad3=new ClassAd("Name = \"XXX\",StartdIpAddr = \"3.2.3\"",',');
  ClassAd* Ad4=new ClassAd("Name = \"XXX\",StartdIpAddr = \"4.2.3\"",',');
  ClassAd* Ad5=new ClassAd("Name = \"XXX\",StartdIpAddr = \"5.2.3\"",',');
  ClassAd* Ad6=new ClassAd("Name = \"XXX\",StartdIpAddr = \"6.2.3\"",',');

  dprintf(D_ALWAYS,"Start\n");

  accountant.SetPriority("Arieh",10);
  accountant.SetPriority("Shlomo",20);
  accountant.SetPriority("Arieh",27);
  accountant.SetPriority("Yuval",200);

  accountant.AddMatch("Arieh",Ad1);
  accountant.AddMatch("Shlomo",Ad2);
  //accountant.AddMatch("Yuval",Ad6);

  accountant.UpdatePriorities();

  accountant.AddMatch("Arieh",Ad3);
  accountant.AddMatch("Shlomo",Ad1);
  accountant.UpdatePriorities();

  accountant.AddMatch("Arieh",Ad2);
  accountant.AddMatch("Shlomo",Ad4);
  accountant.AddMatch("Shlomo",Ad5);
  accountant.AddMatch("Yoram",Ad6);
  
  accountant.UpdatePriorities();

  for (int i=0;i<30;i++) { accountant.UpdatePriorities(); sleep(1); }
  accountant.AddMatch("Yoram",Ad1);
  for (int i=0;i<30;i++) { accountant.UpdatePriorities(); sleep(1); }  

  dprintf(D_ALWAYS,"End.");

  //----------------------------------------------
  
  // ClassAd* AccountantClassAd=new ClassAd();
  // config(AccountantClassAd);
  
  //dprintf_config("ACCOUNTANT", 2);
  //dprintf(D_ALWAYS, "*************************************************\n");
  //dprintf(D_ALWAYS, "************** ACCOUNTANT START UP **************\n");
  //dprintf(D_ALWAYS, "*************************************************\n"); 
  
  // accountant.Init(&core);
  //core.Driver(); 
} 
