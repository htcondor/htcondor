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

  cout<<"Start"<<endl;
  accountant.SetPriority("Arieh",10);
  accountant.SetPriority("Shlomo",20);
  accountant.SetPriority("Arieh",0);
  cout << accountant.GetPriority("Arieh") << endl;
  cout << accountant.GetPriority("Shlomo") << endl;
  cout << accountant.GetPriority("Yoram") << endl;

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
