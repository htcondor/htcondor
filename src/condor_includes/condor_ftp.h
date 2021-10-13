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

#ifndef _CONDOR_FTP_
#define _CONDOR_FTP_

#include <string>

/* This file describes the enumeration to specify the file transfer
protocol used between a submitting client and the schedd/transferd. Please keep
these in the order you find, appending new ones only at the end. */

enum FTPMode /* File Transfer Protocol Mode */
{
	FTP_UNKNOWN = 0,	/* I don't know what file transfer protocol to use */
	FTP_CFTP,			/* Use the internal condor FileTansfer Object */
};

enum FTPDirection
{
	FTPD_UNKNOWN = 0,	/* Don't know what direction I should use */
	FTPD_UPLOAD,		/* upload from the perspective of the calling process */
	FTPD_DOWNLOAD,		/* download from the per. of the calling process. */
};

// Used by the tools to determine how they should put a sandbox into the 
// condor system, either via the schedd directly, or via a transferd.
// These states are created to model backwards compatibility and feature
// selection between submitting/retreiving clients and various versions 
// of Condor.
enum SandboxTransferMethod
{
	STM_UNKNOWN = 0,		/* don't know where/how I should move my sandbox */
	STM_USE_SCHEDD_ONLY,	/* use the old sandbox transfer protocol which 
								locates and store/reads a sandbox directly 
								through the	schedd. This is the "original" 
								method for getting a sandbox into/out of 
								Condor */
	STM_USE_TRANSFERD,		/* Ask the schedd where a transferd is located on 
								behalf of the submitting/retreiving client, and
								then the client will speak directly to the 
								transferd */
};

// functions used to convert a SandboxTransferMethod enum back and forth to
// a std::string
void stm_to_string(SandboxTransferMethod stm, std::string &str);
void string_to_stm(const std::string &str, SandboxTransferMethod &stm);

#endif 
