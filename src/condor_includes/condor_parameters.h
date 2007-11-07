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
