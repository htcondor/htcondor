//////////////////////////////////////////////////////////////////////////////
//
//
//
// Adiel Yoaz
//
//////////////////////////////////////////////////////////////////////////////

#include "debug.h"
#include "condor_daemon_core.h"
#include "condor_classad.h"
#include "Accountant.h"

#include <iostream.h>

extern "C"
{
  void config(ClassAd*);
  void dprintf_config(char*, int);
  void dprintf(int, char*...); 
}

main()
{
  // DaemonCore core(10,10,10);
  Accountant accountant(100,100);

  //----------------------------------------------
  // Test
  //----------------------------------------------

  ClassAd* Ad1=new ClassAd("Name = \"XXX\",STARTD_IP_ADDR = \"1.2.3\"",',');
  ClassAd* Ad2=new ClassAd("Name = \"XXX\",STARTD_IP_ADDR = \"2.2.3\"",',');
  ClassAd* Ad3=new ClassAd("Name = \"XXX\",STARTD_IP_ADDR = \"3.2.3\"",',');
  ClassAd* Ad4=new ClassAd("Name = \"XXX\",STARTD_IP_ADDR = \"4.2.3\"",',');
  ClassAd* Ad5=new ClassAd("Name = \"XXX\",STARTD_IP_ADDR = \"5.2.3\"",',');
  ClassAd* Ad6=new ClassAd("Name = \"XXX\",STARTD_IP_ADDR = \"6.2.3\"",',');

  cout<<"Start"<<endl;
  accountant.SetPriority("Arieh",10);
  accountant.SetPriority("Shlomo",20);
  accountant.SetPriority("Arieh",27);
  cout << accountant.GetPriority("Arieh") << endl;
  cout << accountant.GetPriority("Shlomo") << endl;
  cout << accountant.GetPriority("Yoram") << endl;

  accountant.AddMatch("Arieh",Ad1);
  accountant.AddMatch("Shlomo",Ad2);
  accountant.UpdatePriorities();

  accountant.AddMatch("Arieh",Ad3);
  accountant.AddMatch("Shlomo",Ad1);
  accountant.UpdatePriorities();

  accountant.AddMatch("Arieh",Ad1);
  accountant.AddMatch("Shlomo",Ad4);
  accountant.UpdatePriorities();

  cout<<"End."<<endl;
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
