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

#ifndef CONDOR_PARAMETERS_H
#define CONDOR_PARAMETERS_H

/* This file contains string constants for parameters that may
 * exist in the Condor configuration file. */

// Collector parameters
extern const char *PARAM_COLLECTOR_PORT;
extern const char *PARAM_CONDOR_VIEW_PORT;
extern const char *PARAM_CONDOR_DEVELOPERS_COLLECTOR_PORT;

// Negotiator parameters
extern const char *PARAM_NEGOTIATOR_PORT;

// Gridmanager parameters
extern const char *PARAM_HOLD_IF_CRED_EXPIRED;
extern const char *PARAM_GLOBUS_GATEKEEPER_TIMEOUT;

// Utility functions
int param_get_collector_port(void);
int param_get_condor_view_port(void);
int param_get_condor_developers_collector_port();
int param_get_negotiator_port(void);

#endif /* CONDOR_PARAMETERS_H */
