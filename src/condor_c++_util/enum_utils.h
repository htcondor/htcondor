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

#ifndef _CONDOR_ENUM_UTILS_H
#define _CONDOR_ENUM_UTILS_H

/*
  This file contains utilities for mapping various enum types from our
  code to and from the corresponding strings.
*/

#include "translation_utils.h"

typedef enum { 
	CLAIM_COD=1,
	CLAIM_OPPORTUNISTIC,
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
	JA_VACATE_FAST_JOBS
} JobAction;

const char* getJobActionString( JobAction action );
JobAction getJobActionNum( const char* string );


#endif /* _CONDOR_ENUM_UTILS_H */
