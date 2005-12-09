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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "JobLogReader.h"
#include "Scheduler.h"
#include "JobRouter.h"

// about self
char* mySubSystem = "SCHEDD7";		// used by Daemon Core


Scheduler schedd;
JobRouter *job_router;

//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	schedd.init();
	
	job_router = new JobRouter(&schedd);
	job_router->init();

	return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool is_full )
{
	dprintf(D_ALWAYS, "main_config() called\n");

	schedd.config();
	job_router->config();

	return TRUE;
}

static void Stop()
{
	// JobRouter creates an instance lock, so delete it now to clean up.
	schedd.stop();
	delete job_router;
	job_router = NULL;
	DC_Exit(0);
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	Stop();
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	Stop();
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int argc, char* argv[] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}

