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

#ifndef JOB_REPORT_H
#define JOB_REPORT_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_qmgr.h"
#include "metric_units.h"

/*
This module serves to store and recall job summary info that will be
given back to the user, either by text or through a ClassAd.

The job_report_store_* functions record information
about the job as it is available.

The job_report_display_* functions produce text output according to the
stored data.

The job_report_update_queue function transmits the same stored data
to the currently open ClassAd Q.
*/

#define JOB_REPORT_RECORD_MAX 2048

int  job_report_store_call( int call );
int  job_report_store_error( char *format, ... );
void job_report_store_file_info( char *name, long long oc, long long rc, long long wc, long long sc, long long rb, long long wb );

void job_report_display_calls( FILE *f );
void job_report_display_errors( FILE *f );
void job_report_display_file_info( FILE *f, int total_time );

void job_report_update_queue( PROC *proc );

#endif
