/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_pidenvid.h"
#include "procapi_t.h"

// this test is not 100% complete it should check for each different os 
// whether seeing the information is an error or not


/////////////////////////////test3/////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int permission_test(bool verbose) {

	int status;
	PidEnvID penvid;

	pidenvid_init(&penvid);

  piPTR pi = NULL;

  if(verbose){
  printf ( "\n..................................\n" );
  printf ( "This test determines if you can get information on processes\n" );
  printf ( "other than those you own.  If you get a summary of all the\n" );
  printf ( "processes in the system ( parent=1 ) then you can.\n\n" );
  }

  if ( ProcAPI::getFamilyInfo( 1, pi, &penvid, status ) == PROCAPI_FAILURE ){
      printf ( "Unable to get info on process 1 (init)\n");
  }
  else{
   
    printf("user has access to info on all processes:\n init info:\n");
    if(verbose){
      ProcAPI::printProcInfo( pi );
    }
  }
  delete pi;

  return 1;
}
