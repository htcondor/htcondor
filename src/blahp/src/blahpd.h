/*
#  File:     blahpd.h
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
#
#    See http://www.eu-egee.org/partners/ for details on the copyright
#    holders.  
#  
#    Licensed under the Apache License, Version 2.0 (the "License"); 
#    you may not use this file except in compliance with the License. 
#    You may obtain a copy of the License at 
#  
#        http://www.apache.org/licenses/LICENSE-2.0 
#  
#    Unless required by applicable law or agreed to in writing, software 
#    distributed under the License is distributed on an "AS IS" BASIS, 
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
#    See the License for the specific language governing permissions and 
#    limitations under the License.
#
#
*/

#ifndef BLAHPD_H_INCLUDED
#define BLAHPD_H_INCLUDED

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "config.h"

#define RCSID_VERSION		"$GahpVersion: %s Mar 31 2008 INFN\\ blahpd\\ (%s) $"

#define DEFAULT_GLITE_LOCATION "/usr"
#define DEFAULT_GLEXEC_COMMAND "/usr/sbin/glexec"
#define DEFAULT_SUDO_COMMAND "/usr/bin/sudo"

/* Change this in order to select the default batch system
 * (overridden by BLAH_LRMS env variable)*/
#define DEFAULT_LRMS	"lsf"

#define BLAHPD_CRLF	"\r\n"

#define POLL_INTERVAL	3

#define MALLOC_ERROR	1

#define JOBID_MAX_LEN	256
#define ERROR_MAX_LEN	256
#define RESLN_MAX_LEN	2048
#define MAX_JOB_NUMBER  10

#ifdef DEBUG
#define BLAHDBG(format, message) fprintf(stderr, format, message)
#else
#define BLAHDBG(format, message)
#endif

/* Job states
 * */
typedef enum job_states {
	UNDEFINED,
	IDLE,
	RUNNING,
	REMOVED,
	COMPLETED,
	HELD
} job_status_t;


#endif /* defined BLAHPD_H_INCLUDED */
