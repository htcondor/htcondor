//////////////////////////////////////////////////////////////////////////////
//
// Adiel Yoaz
//
//////////////////////////////////////////////////////////////////////////////

#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "condor_classad.h"
#include "condor_accountant.h"

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

  //  sleep(1);
  //accountant.RemoveMatch("XXX6.2.3");
  //sleep(1);
  //accountant.UpdatePriorities();
  
  //sleep(10);
  sleep(2);
  accountant.RemoveMatch("1@fred");
  dprintf(D_ALWAYS,"Before second removal\n");
  accountant.RemoveMatch("1@fred");
  sleep(4);
  dprintf(D_ALWAYS,"Before addMatch\n");
  accountant.AddMatch("Arieh",Ad1);
  sleep(4);
  //for (int i=0,j; i<10000000; i++) j=(i+1)*2/3+(i*7);
  accountant.UpdatePriorities();
  sleep(10);
  
  while(1) { sleep(1); accountant.UpdatePriorities(); }

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
