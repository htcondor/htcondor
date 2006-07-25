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

#ifndef CONDOR_ERROR_H
#define CONDOR_ERROR_H

#include "condor_common.h"

BEGIN_C_DECLS

/*
Display a printf-style message for a user-visible error.
_retry causes an exit allowing a rollback to the last checkpoint,
while _fatal causes a permanent termination.
*/

void _condor_error_retry( const char *format, ... );
void _condor_error_fatal( const char *format, ... );

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

void _condor_warning( condor_warning_kind_t kind, const char *format, ... );

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
