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

const int CONDOR_HOLD_CODE_Unspecified = 0;
const int CONDOR_HOLD_CODE_UserRequest = 1;
const int CONDOR_HOLD_CODE_GlobusGramError = 2;
const int CONDOR_HOLD_CODE_JobPolicy   = 3;
const int CONDOR_HOLD_CODE_CorruptedCredential = 4;
const int CONDOR_HOLD_CODE_JobPolicyUndefined   = 5;
const int CONDOR_HOLD_CODE_FailedToCreateProcess = 6;
const int CONDOR_HOLD_CODE_UnableToOpenOutput = 7;
const int CONDOR_HOLD_CODE_UnableToOpenInput = 8;
const int CONDOR_HOLD_CODE_UnableToOpenOutputStream = 9;
const int CONDOR_HOLD_CODE_UnableToOpenInputStream = 10;
const int CONDOR_HOLD_CODE_InvalidTransferAck = 11;
const int CONDOR_HOLD_CODE_DownloadFileError = 12;
const int CONDOR_HOLD_CODE_UploadFileError = 13;

#endif
