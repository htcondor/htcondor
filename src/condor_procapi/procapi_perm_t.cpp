/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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

  if ( ProcAPI::getProcInfo( 1, pi, status ) == PROCAPI_FAILURE ){
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
