
#ifndef JOB_REPORT_H
#define JOB_REPORT_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_qmgr.h"
#include "shadow.h"
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
