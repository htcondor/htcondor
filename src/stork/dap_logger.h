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
#ifndef _DAP_LOGGER_H_
#define _DAP_LOGGER_H_

#include "condor_common.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "condor_event.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "classad_distribution.h"

void write_dap_log(char *logfilename, char *status, char *param1, char *value1, char *param2 = NULL, char *value2 = NULL, char *param3 = NULL, char *value3 = NULL, char *param4 = NULL, char *value4 = NULL, char *param5 = NULL, char *value5 = NULL, char *param6 = NULL, char *value6 = NULL);

void write_classad_log(char *logfilename, char *status, classad::ClassAd *classad);

void write_collection_log(classad::ClassAdCollection *dapcollection, char *dap_id, const char *update);

void write_xml_log(char *logfilename, classad::ClassAd *classad, const char *status);

void write_xml_user_log(char *logfilename,char *param1, char *value1, char
		*param2 = NULL, char *value2 = NULL, char *param3 = NULL, char *value3
		= NULL, char *param4 = NULL, char *value4 = NULL, char *param5 = NULL,
		char *value5 = NULL, char *param6 = NULL, char *value6 = NULL, char
		*param7 = NULL, char *value7 = NULL,char *param8 = NULL, char *value8 =
		NULL,char *param9 = NULL, char *value9 = NULL,
		char *param10 = NULL, char *value10 = NULL);
bool
user_log(	const classad::ClassAd *ad,
			const enum ULogEventNumber eventNum);


#endif
















