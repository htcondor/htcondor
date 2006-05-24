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

#ifndef CONDOR_HOLDCODES_H
#define CONDOR_HOLDCODES_H

/* This file contains hold reason codes.
 */

//There may still be some lingering cases that result in this
//unspecified hold code.  Hopefully they will be eliminated soon.
const int CONDOR_HOLD_CODE_Unspecified = 0;

//User put the job on hold with condor_hold
const int CONDOR_HOLD_CODE_UserRequest = 1;

//Globus reported an error.  The subcode is the GRAM error number.
const int CONDOR_HOLD_CODE_GlobusGramError = 2;

//The periodic hold expression evaluated to true
const int CONDOR_HOLD_CODE_JobPolicy   = 3;

//The credentials for the job (e.g. X509 proxy file) are invalid.
const int CONDOR_HOLD_CODE_CorruptedCredential = 4;

//A job policy expression (such as PeriodicHold) evaluated to UNDEFINED.
const int CONDOR_HOLD_CODE_JobPolicyUndefined   = 5;

//The condor_starter failed to start the executable.
//The subcode will contain the unix errno.
const int CONDOR_HOLD_CODE_FailedToCreateProcess = 6;

//The standard output file for the job could not be opened.
//The subcode will contain the unix errno.
const int CONDOR_HOLD_CODE_UnableToOpenOutput = 7;

//The standard input file for the job could not be opened.
//The subcode will contain the unix errno.
const int CONDOR_HOLD_CODE_UnableToOpenInput = 8;

//The standard output stream for the job could not be opened.
//The subcode will contain the unix errno.
const int CONDOR_HOLD_CODE_UnableToOpenOutputStream = 9;

//The standard input stream for the job could not be opened.
//The subcode will contain the unix errno.
const int CONDOR_HOLD_CODE_UnableToOpenInputStream = 10;

//An internal Condor protocol error was encountered when transferring files.
const int CONDOR_HOLD_CODE_InvalidTransferAck = 11;

//The condor_starter failed to download input files.
//The subcode will contain the unix errno.
const int CONDOR_HOLD_CODE_DownloadFileError = 12;

//The condor_starter failed to upload output files.
//The subcode will contain the unix errno.
const int CONDOR_HOLD_CODE_UploadFileError = 13;

#endif
