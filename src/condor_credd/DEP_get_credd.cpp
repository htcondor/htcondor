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
#include "condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "dc_credd.h"

int main(int argc, char **argv)
{
  char * credd_sin = argv[1];
  char * cred_name = argv[2];

  CondorError errorstack;
  char * cred_data = NULL;
  int cred_size = 0;

  DCCredd credd(credd_sin);
  if (credd.getCredentialData ((const char*)cred_name, (void*&)cred_data, cred_size, errorstack)) {
      printf ("Received %d \n%s\n", cred_size, cred_data);
	  return 0;
  } else {
	  fprintf (stderr, "ERROR (%d : %s)\n", 
			   errorstack.code(),
			   errorstack.message());
	  return 1;
  }
}

  
