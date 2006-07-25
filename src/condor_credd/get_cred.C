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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
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
  if (credd.getCredentialData (cred_name, (void*)cred_data, cred_size, errorstack)) {
      printf ("Received %d \n%s\n", cred_size, cred_data);
	  return 0;
  } else {
	  fprintf (stderr, "ERROR (%d : %s)\n", 
			   errorstack.code(),
			   errorstack.message());
	  return 1;
  }
}

  
