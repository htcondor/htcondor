/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_accountant.h"

//-------------------------------------------------------------

// about self
char* mySubSystem = "ACCOUNTANT";		// used by Daemon Core

//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
  Accountant accountant;
  accountant.Initialize();
  dprintf(D_ALWAYS,"main: After initialization\n");

  // accountant.DisplayLog();

  // accountant.ResetAllUsage();

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
  accountant.AddMatch("nice-user.Shlomo",Ad4);
  accountant.AddMatch("Yuval",Ad5);
  accountant.AddMatch("Yuval",Ad6);

  accountant.AddMatch("localuser@cs.wisc.edu",Ad6);
  accountant.AddMatch("nice-user.stamuser@cs.wisc.edu",Ad5);
  accountant.AddMatch("remoteuser@xxx.yyy.zzz",Ad4);

  accountant.UpdatePriorities();

  // accountant.SetPriority("Arieh",3);
  // accountant.SetPriority("nice-user.Shlomo",2);
  // accountant.SetPriority("Yuval",1);

  for(int i=0;i<2; i++) { sleep(5); accountant.UpdatePriorities(); }

  // while(1) accountant.AddMatch("Yuval",Ad6);

  dprintf(D_ALWAYS,"Arieh = %f\n",accountant.GetPriority("Arieh"));
  dprintf(D_ALWAYS,"Yuval = %f\n",accountant.GetPriority("Yuval"));
  dprintf(D_ALWAYS,"nice-user.Shlomo = %f\n",accountant.GetPriority("nice-user.Shlomo"));

  dprintf(D_ALWAYS,"localuser@cs.wisc.edu = %f\n",accountant.GetPriority("localuser@cs.wisc.edu"));
  dprintf(D_ALWAYS,"nice-user.stamuser@cs.wisc.edu = %f\n",accountant.GetPriority("nice-user.stamuser@cs.wisc.edu"));
  dprintf(D_ALWAYS,"remoteuser@xxx.yyy.zzz = %f\n",accountant.GetPriority("remoteuser@xxx.yyy.zzz"));

  // accountant.DisplayLog();
  // accountant.DisplayMatches();

  AttrList* AL=accountant.ReportState();
  AL->fPrint(stdout);

  accountant.AddMatch("Yuval",Ad5);
  AL=accountant.ReportState("Yuval");
  AL->fPrint(stdout);

  exit(0);

  return TRUE;
}

//-------------------------------------------------------------

int main_config( bool is_full )
{
	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	exit(0);
	return TRUE;	// to satisfy c++
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}

