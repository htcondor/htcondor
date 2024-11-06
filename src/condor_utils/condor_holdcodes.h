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


#ifndef CONDOR_HOLDCODES_H
#define CONDOR_HOLDCODES_H


/* This file contains hold reason codes.

   They are stored in a reflective enum class CONDOR_HOLD_CODE via enum.h 
   (see https://tinyurl.com/yfn3auay for complete docs on Better Enums).
   The TLDR is you can use these enums as you would expect, including conversions to ints, like so:
       int code = CONDOR_HOLD_CODE::JobPolicy;
       switch (code) {
	        CONDOR_HOLD_CODE::GlobalGramError:
            ...
       }
   But you can also do a bunch more than with normal enums.  For example, you can fetch 
   the name of the enum as a string with method _to_string, and create an enum from an 
   int with _from_integral.  A pithy example using both:
       int reason_code = 1;   // code 1 is CONDOR_HOLD_CODE::UserRequest
       const char *holdstr = (CONDOR_HOLD_CODE::_from_integral(reason_code))._to_string();
       // Now holdstr = "UserRequst"
   There is a lot more, see the docs for BetterEnums at the url above.
 */

#define BETTER_ENUMS_MACRO_FILE "enum_larger.h"
#include "enum.h" 
BETTER_ENUM(CONDOR_HOLD_CODE, int,

	//There may still be some lingering cases that result in this
	//unspecified hold code.  Hopefully they will be eliminated soon.
	Unspecified = 0,

	//User put the job on hold with condor_hold
	UserRequest = 1,

	//Globus reported an error.  The subcode is the GRAM error number.
	GlobusGramError = 2,

	//The periodic hold expression evaluated to true
	JobPolicy = 3,

	//The credentials for the job (e.g. X509 proxy file) are invalid.
	CorruptedCredential = 4,

	//A job policy expression (such as PeriodicHold) evaluated to UNDEFINED.
	JobPolicyUndefined = 5,

	//The condor_starter failed to start the executable.
	//The subcode will contain the unix errno.
	FailedToCreateProcess = 6,

	//The standard output file for the job could not be opened.
	//The subcode will contain the unix errno.
	UnableToOpenOutput = 7,

	//The standard input file for the job could not be opened.
	//The subcode will contain the unix errno.
	UnableToOpenInput = 8,

	//The standard output stream for the job could not be opened.
	//The subcode will contain the unix errno.
	UnableToOpenOutputStream = 9,

	//The standard input stream for the job could not be opened.
	//The subcode will contain the unix errno.
	UnableToOpenInputStream = 10,

	//An internal Condor protocol error was encountered when transferring files.
	InvalidTransferAck = 11,

	//There was a failure transferring the job's output (or checkpoint) files
	//from the EP back to the the AP.
	//The subcode will contain the unix errno.
	TransferOutputError = 12,

	//There was a failure transferring the job's input files
	//from the AP to the EP.
	//The subcode will contain the unix errno.
	TransferInputError = 13,

	//The initial working directory of the job cannot be accessed.
	//The subcode will contain the unix errno.
	IwdError = 14,

	//The user requested the job be submitted on hold.
	SubmittedOnHold = 15,

	//Input files are being spooled.
	SpoolingInput = 16,

	//In the standard universe, the job and shadows versions aren't
	//compatible.
	JobShadowMismatch = 17,

	//An internal Condor protocol error was encountered when transferring files.
	InvalidTransferGoAhead = 18,

	/**
	   HOOK_PREPARE_JOB was defined but couldn't execute or returned failure.
	   The hold subcode will be 0 if we failed to execute, the exit status
	   if it exited with a failure code, or a negative number with the signal
	   number if it was killed by a signal (e.g. -9).
	*/
	HookPrepareJobFailure = 19,

	MissedDeferredExecutionTime = 20,

	StartdHeldJob = 21,

	// There was a problem opening or otherwise initializing
	// the user log for writing.
	UnableToInitUserLog = 22,

	FailedToAccessUserAccount = 23,

	NoCompatibleShadow = 24,

	InvalidCronSettings = 25,

	// The SYSTEM_PERIODIC_HOLD expression put the job on hold
	SystemPolicy = 26,

	SystemPolicyUndefined = 27,

	GlexecChownSandboxToUser = 28,

	PrivsepChownSandboxToUser = 29,

	GlexecChownSandboxToCondor = 30,

	PrivsepChownSandboxToCondor = 31,

	MaxTransferInputSizeExceeded = 32,

	MaxTransferOutputSizeExceeded = 33,

	JobOutOfResources = 34,

	InvalidDockerImage = 35,

	FailedToCheckpoint = 36,

	EC2UserError = 37,
	EC2InternalError = 38,
	EC2AdminError = 39,
	EC2ConnectionProblem = 40,
	EC2ServerError = 41,
	EC2InstancePotentiallyLostError = 42,

	PreScriptFailed = 43,
	PostScriptFailed = 44,

	// Running singularity test before a sinularity job returned non-zero
	SingularityTestFailed = 45,

	JobDurationExceeded = 46,
	JobExecuteExceeded = 47,

	HookShadowPrepareJobFailure = 48,

	VacateBase = 1000,
	JobPolicyVacate = 1000,
	SystemPolicyVacate = 1001,
	ShadowException = 1002,
	JobNotStarted = 1003,
	UserVacateJob = 1004,
	JobShouldRequeue = 1005,
	FailedToActivateClaim = 1006,
	StarterError = 1007,
	ReconnectFailed = 1008,
	ClaimDeactivated = 1009,
	StartdVacateCommand = 1010,
	StartdPreemptExpression = 1011,
	StartdException = 1012,
	StartdShutdown = 1013,
	StartdDraining = 1014,
	StartdCoalesce = 1015,
	StartdHibernate = 1016,
	StartdReleaseCommand = 1017,
	StartdPreemptingClaimRank = 1018,
	StartdPreemptingClaimUserPrio = 1019
	// NOTE!!! If you add a new hold code here, don't forget to add a commas after all entries but the last!
	// NOTE!!! If you add a new hold code here, don't forget to update the Appendix in the Manual for Job ClassAds!
)

BETTER_ENUM(FILETRANSFER_HOLD_CODE, int,

	// These are two enums are only to be used by the FileTransfer object.

	//The starter or shadow failed to receive or write job files.
	//The subcode will contain the unix errno.
	DownloadFileError = 12,

	//The starter or shadow failed to read or send job files.
	//The subcode will contain the unix errno.
	UploadFileError = 13

)

#endif
