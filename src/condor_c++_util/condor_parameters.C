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
#include "condor_debug.h"
#include "condor_parameters.h"
#include "condor_config.h"
#include "condor_network.h" 

// Collector parameters
const char *PARAM_COLLECTOR_PORT                   = "COLLECTOR_PORT";
const char *PARAM_CONDOR_VIEW_PORT                 = "CONDOR_VIEW_PORT";
const char *PARAM_CONDOR_DEVELOPERS_COLLECTOR_PORT = "CONDOR_DEVELOPERS_COLLECTOR_PORT";

// Negotiator parameters
const char *PARAM_NEGOTIATOR_PORT   = "NEGOTIATOR_PORT";

// Gridmanager parameters
const char *PARAM_HOLD_IF_CRED_EXPIRED = "HOLD_JOB_IF_CREDENTIAL_EXPIRES";
const char *PARAM_GLOBUS_GATEKEEPER_TIMEOUT = "GLOBUS_GATEKEEPER_TIMEOUT";

/****************************************************************************
 *
 * Function: param_get_collector_port
 * Purpose:  Find the port that the collector should be running on. 
 *           This occurs in multiple places in the code, so we made it
 *           into a function, even though it is rather simple.
 *
 ****************************************************************************/
int param_get_collector_port(void)
{
	int collector_port;

	collector_port = param_integer(PARAM_COLLECTOR_PORT, COLLECTOR_PORT);

	if (collector_port <= 0 || collector_port >= 65536) {
		dprintf(D_ALWAYS, "%s has bad value (%d), resetting to %d\n",
				PARAM_COLLECTOR_PORT, collector_port, COLLECTOR_PORT);
		collector_port = COLLECTOR_PORT;
	}

	return collector_port;
}

/****************************************************************************
 *
 * Function: param_get_condor_view_port
 * Purpose: Find the port that the Condor View collector should be
 *          running on. This occurs in multiple places in the code, so we 
 *          made it into a function, even though it is rather simple.
 *
 ****************************************************************************/
int param_get_condor_view_port(void)
{
	int condor_view_port;

	condor_view_port = param_integer(PARAM_CONDOR_VIEW_PORT, CONDOR_VIEW_PORT);
	if (condor_view_port <= 0 || condor_view_port >= 65536) {
		dprintf(D_ALWAYS, "%s has bad value (%d), resetting to %d\n",
				PARAM_CONDOR_VIEW_PORT, condor_view_port, CONDOR_VIEW_PORT);
		condor_view_port = CONDOR_VIEW_PORT;
	}

	return condor_view_port;
}

/****************************************************************************
 *
 * Function: param_get_collector_port
 * Purpose:  Find the port that the collector should be running on. 
 *           This occurs in multiple places in the code, so we made it
 *           into a function, even though it is rather simple.
 *
 ****************************************************************************/
int param_get_condor_developers_collector_port(void)
{
	int collector_port;

	collector_port = param_integer(PARAM_CONDOR_DEVELOPERS_COLLECTOR_PORT, 
								   COLLECTOR_PORT);

	if (collector_port <= 0 || collector_port >= 65536) {
		dprintf(D_ALWAYS, "%s has bad value (%d), resetting to %d\n",
				PARAM_CONDOR_DEVELOPERS_COLLECTOR_PORT, collector_port, 
				COLLECTOR_PORT);
		collector_port = COLLECTOR_PORT;
	}

	return collector_port;
}

/****************************************************************************
 *
 * Function: param_get_negotiator_port
 * Purpose:  Find the port that the negotiator should be running on. 
 *           This occurs in multiple places in the code, so we made it
 *           into a function, even though it is rather simple.
 *
 ****************************************************************************/
int param_get_negotiator_port(void)
{
	int negotiator_port;

	negotiator_port = param_integer(PARAM_NEGOTIATOR_PORT, NEGOTIATOR_PORT);
	if (negotiator_port <= 0 || negotiator_port >= 65536) {
		dprintf(D_ALWAYS, "%s has bad value (%d), resetting to %d\n",
				PARAM_NEGOTIATOR_PORT, negotiator_port, NEGOTIATOR_PORT);
		negotiator_port = NEGOTIATOR_PORT;
	}
	return negotiator_port;
}
