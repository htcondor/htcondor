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


#ifndef _CONDOR_ENUM_UTILS_H
#define _CONDOR_ENUM_UTILS_H

/*
  This file contains utilities for mapping various enum types from our
  code to and from the corresponding strings.
*/

#include "translation_utils.h"

typedef enum { 
	CLAIM_NONE=0,
	CLAIM_COD=1,
	CLAIM_OPPORTUNISTIC,
	CLAIM_FETCH,
} ClaimType;

const char* getClaimTypeString( ClaimType type );
ClaimType   getClaimTypeNum( const char* string );


typedef enum {
    CLAIM_UNCLAIMED,
    CLAIM_IDLE,
    CLAIM_RUNNING,
    CLAIM_SUSPENDED,
    CLAIM_VACATING,
    CLAIM_KILLING,
} ClaimState;

const char* getClaimStateString( ClaimState state );
ClaimState  getClaimStateNum( const char* string );


typedef enum { 
    HOOK_FETCH_WORK=0,
    HOOK_REPLY_FETCH,
    HOOK_EVICT_CLAIM,
    HOOK_PREPARE_JOB,
    HOOK_UPDATE_JOB_INFO,
    HOOK_JOB_EXIT,
    HOOK_EVICT_JOB,
    HOOK_TRANSLATE_JOB,
    HOOK_JOB_CLEANUP,
    HOOK_JOB_FINALIZE,
} HookType;

const char* getHookTypeString( HookType type );
HookType    getHookTypeNum( const char* string );


typedef enum { 
	VACATE_GRACEFUL = 1,
	VACATE_FAST
} VacateType;

const char* getVacateTypeString( VacateType type );
VacateType  getVacateTypeNum( const char* string );


typedef enum { 
	STF_NO = 1,
	STF_YES,
	STF_IF_NEEDED
} ShouldTransferFiles_t;

const char* getShouldTransferFilesString( ShouldTransferFiles_t type );
ShouldTransferFiles_t getShouldTransferFilesNum( const char* string );


typedef enum { 
	FTO_NONE = 0,
	FTO_ON_EXIT = 1,
	FTO_ON_EXIT_OR_EVICT,
	FTO_ON_SUCCESS
} FileTransferOutput_t;

const char* getFileTransferOutputString( FileTransferOutput_t type );
FileTransferOutput_t getFileTransferOutputNum( const char* string );


// ATTR_JOB_ACTION should be one of these
typedef enum {
	JA_ERROR = 0,
	JA_HOLD_JOBS,
	JA_RELEASE_JOBS,
	JA_REMOVE_JOBS,
	JA_REMOVE_X_JOBS,
	JA_VACATE_JOBS,
	JA_VACATE_FAST_JOBS,
	JA_CLEAR_DIRTY_JOB_ATTRS,
	JA_SUSPEND_JOBS,
	JA_CONTINUE_JOBS
} JobAction;

const char* getJobActionString( JobAction action );
JobAction getJobActionNum( const char* string );


// enum for possible CRON_AUTOPUBLISH values
typedef enum {
	CAP_ERROR = -1,
	CAP_NEVER = 0,
	CAP_ALWAYS,
	CAP_IF_CHANGED
} CronAutoPublish_t;

const char* getCronAutoPublishString( CronAutoPublish_t val );
CronAutoPublish_t getCronAutoPublishNum( const char* string );

// enum for provisioner job states
enum ProvisionerState {
	NEW = 0,
	PROVISIONING_STARTED = 1,
	PROVISIONING_COMPLETE = 2,
	DEPROVISIONING_STARTED = 3,
	DEPROVISIONING_COMPLETE = 4
};


#endif /* _CONDOR_ENUM_UTILS_H */
