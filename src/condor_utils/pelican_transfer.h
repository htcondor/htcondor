/***************************************************************
 *
 * Copyright (C) 2025, Condor Team, Computer Sciences Department,
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

#ifndef _PELICAN_TRANSFER_H
#define _PELICAN_TRANSFER_H

#include <ctime>
#include <string>
#include <vector>

// Forward declarations
class ReliSock;

// Note: FileTransferList, FileTransferItem, and ClassAd are defined in file_transfer_internal.h
// which is included by file_transfer.h. Code using pelican_transfer.h should
// include file_transfer.h to get all necessary definitions.

// Pelican token response structure
struct PelicanTokenResponse {
	std::string token;
	time_t expires_at;
	bool success;
	std::string error_message;
};

// Common checks for Pelican file transfer eligibility
// Returns true if basic requirements are met, false otherwise
bool ShouldUsePelicanTransferCommon(ClassAd *job_ad, bool peer_supports_pelican);

// Check if Pelican file transfer should be used for input sandbox
// Returns true if Pelican should be used, false otherwise
bool ShouldInputUsePelicanTransfer(ClassAd *job_ad, bool peer_supports_pelican);

// Check if Pelican file transfer should be used for output sandbox
// Returns true if Pelican should be used, false otherwise
bool ShouldOutputUsePelicanTransfer(ClassAd *job_ad, bool peer_supports_pelican);

// Request a token from the Pelican token service via domain socket
// This posts the job ad to the service and receives back a token and expiration time
PelicanTokenResponse PelicanRequestToken(ClassAd *job_ad, const std::string &socket_path);

// Generate Pelican URL for the sandbox
std::string PelicanGenerateSandboxURL(const std::string &hostname, int cluster, int proc, bool is_input);

// Prepare file list for Pelican transfer
// Removes regular files from the list and replaces with Pelican URL + metadata
// Returns true on success, false on failure
bool PreparePelicanTransferList(FileTransferList &filelist, ClassAd *job_ad, const std::string &jobid, ReliSock *sock);

// Build a response ClassAd for a Pelican URL metadata request
// Called by shadow when starter requests metadata for output transfers
// Returns true if Pelican metadata was successfully added, false otherwise
bool BuildPelicanMetadataResponse(const std::string& url, ClassAd *job_ad, ClassAd& response_ad);

// Prepare output sandbox files for Pelican transfer
// Similar to PreparePelicanTransferList but for output/upload side
bool PreparePelicanOutputTransferList(
	ReliSock *sock,
	FileTransferList& filelist,
	ClassAd *job_ad,
	bool peer_supports_pelican);

#endif // _PELICAN_TRANSFER_H
