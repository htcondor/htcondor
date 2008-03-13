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

#ifndef _DAP_LOGGER_H_
#define _DAP_LOGGER_H_

#include "condor_common.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "condor_event.h"
#include "condor_daemon_core.h"

#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif
#include "condor_fix_iostream.h"
#include "classad/classad_distribution.h"

void write_dap_log(const char *logfilename, const char *status, const char *param1, const char *value1, const char *param2 = NULL, const char *value2 = NULL, const char *param3 = NULL, const char *value3 = NULL, const char *param4 = NULL, const char *value4 = NULL, const char *param5 = NULL, const char *value5 = NULL, const char *param6 = NULL, const char *value6 = NULL);

void write_classad_log(const char *logfilename, const char *status, classad::ClassAd *classad);

void write_collection_log(classad::ClassAdCollection *dapcollection, const char *dap_id, const char *update);

void write_xml_log(const char *logfilename, classad::ClassAd *classad, const char *status);

void write_xml_user_log(const char *logfilename,
			const char *param1, const char *value1,
			const char *param2 = NULL, const char *value2 = NULL,
			const char *param3 = NULL, const char *value3 = NULL,
			const char *param4 = NULL, const char *value4 = NULL,
			const char *param5 = NULL, const char *value5 = NULL,
			const char *param6 = NULL, const char *value6 = NULL,
			const char *param7 = NULL, const char *value7 = NULL,
			const char *param8 = NULL, const char *value8 = NULL,
			const char *param9 = NULL, const char *value9 = NULL,
			const char *param10 = NULL, const char *value10 = NULL);
bool
user_log(	const classad::ClassAd *ad,
			const enum ULogEventNumber eventNum);


#endif
















