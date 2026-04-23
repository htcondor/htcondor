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
class FileTransfer;
class DCTransferQueue;
struct FTProtocolBits;

// Pelican registration response structure
struct PelicanRegistrationResponse {
	std::string token;
	time_t expires_at;
	bool success;
	std::vector<std::string> input_urls;   // URLs for input sandbox downloads (supports multiple for parallel/shared transfers)
	std::vector<std::string> output_urls;  // URLs for output sandbox uploads (supports multiple for parallel transfers)
	std::string error_message;
	std::string ca_contents; // Auto-detected CA certificates from TLS handshake
};

// Check if Pelican file transfer should be used for input sandbox
// Returns true if Pelican should be used, false otherwise
bool ShouldInputUsePelicanTransfer(ClassAd *job_ad, bool peer_supports_pelican);

// Check if Pelican file transfer should be used for output sandbox
// Returns true if Pelican should be used, false otherwise
bool ShouldOutputUsePelicanTransfer(ClassAd *job_ad, bool peer_supports_pelican);

// Request a token from the Pelican registration service via domain socket
// This posts the job ad to the service and receives back token, expiration time, and URLs
PelicanRegistrationResponse PelicanRegisterJob(ClassAd *job_ad, const std::string &socket_path);

// Transform output file list to use Pelican
// Called by shadow-side to transform the output transfer list
// Returns true on success, false on failure
bool TransformPelicanOutputList(
	FileTransferList &filelist,
	ClassAd *job_ad);

// Prepare file list for Pelican transfer
// Removes regular files from the list and replaces with Pelican URL + metadata
// Returns true on success, false on failure
bool PreparePelicanInputTransferList(
	FileTransferList &filelist,
	ClassAd *job_ad,
	const std::string &jobid,
	FileTransfer *ft,
	ReliSock *sock,
	DCTransferQueue &xfer_queue,
	filesize_t sandbox_size,
	FTProtocolBits &protocolState);

#endif // _PELICAN_TRANSFER_H
