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


#ifndef CONDOR_ERROR_H
#define CONDOR_ERROR_H

#include "condor_common.h"

BEGIN_C_DECLS

/*
Display a printf-style message for a user-visible error.
_retry causes an exit allowing a rollback to the last checkpoint,
while _fatal causes a permanent termination.
*/

void _condor_error_retry( const char *format, ... ) CHECK_PRINTF_FORMAT(1,2);
void _condor_error_fatal( const char *format, ... ) CHECK_PRINTF_FORMAT(1,2);

typedef enum {
	CONDOR_WARNING_KIND_ALL,
	CONDOR_WARNING_KIND_NOTICE,
	CONDOR_WARNING_KIND_READWRITE,
	CONDOR_WARNING_KIND_UNSUP,
	CONDOR_WARNING_KIND_BADURL,
	CONDOR_WARNING_KIND_MAX
} condor_warning_kind_t;

typedef enum {
	CONDOR_WARNING_MODE_ON,
	CONDOR_WARNING_MODE_ONCE,
	CONDOR_WARNING_MODE_OFF,
	CONDOR_WARNING_MODE_MAX
} condor_warning_mode_t;

/*
Display a printf-style warning message of the given kind.
This function may or may not squelch the message based
on previous calls to _condor_warning_config.
*/

void _condor_warning( condor_warning_kind_t kind, const char *format, ... ) CHECK_PRINTF_FORMAT(2,3);

/*
Configure warnings of a particular kind.
The kind may be a particular warning kind, or ALL to set the mode for all.
All warnings are ON by default.
*/

int _condor_warning_config( condor_warning_kind_t kind, condor_warning_mode_t mode );
int _condor_warning_config_byname( const char *kind, const char *mode );

/* Return a static buffer listing all of the available choices. */

char *_condor_warning_kind_choices();
char *_condor_warning_mode_choices();

END_C_DECLS

#endif
