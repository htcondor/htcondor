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
#include "xferstat2.h"
#include "network2.h"
#include <iostream.h>
#include <stdio.h>


int main(void)
{
  TransferState  xfs;
  int            count;
  struct in_addr addr;
  char           temp[100];

  for (count=100; count<110; count++)
    {
      addr.s_addr = count;
      sprintf(temp, "file%04d", count);
      xfs.Insert(count, count-99, addr, temp, "tsao", count, 4, 0, RECV);
    }
  xfs.Dump();
  cout << "Key for child #104:              " << xfs.GetKey(104) << endl;
  addr.s_addr = 107;
  cout << "Key for 107.0.0.0/tsao/file0107: " << xfs.GetKey(addr, "tsao",
		    "file0107") << endl;
  if (xfs.Delete(100) != OK)
    cout << "Cannot delete 100" << endl;
  if (xfs.Delete(104) != OK)
    cout << "Cannot delete 100" << endl;
  if (xfs.Delete(109) != OK)
    cout << "Cannot delete 100" << endl;
  if (xfs.Delete(110) != OK)
    cout << "Cannot delete 100" << endl;
  xfs.Dump();
  cout << "Key for child #104:              " << xfs.GetKey(104) << endl;
  return 0;
}
