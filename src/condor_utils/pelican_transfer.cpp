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

#include "condor_common.h"
#include "basename.h"
#include "classad/classad_distribution.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "file_transfer.h"
#include "file_transfer_internal.h"
#include "pelican_transfer.h"
#include "stl_string_utils.h"

#include <curl/curl.h>

#include <fstream>
#include <iterator>
#include <memory>

namespace {

// Common checks for Pelican file transfer eligibility
// Returns true if basic requirements are met, false otherwise
bool ShouldUsePelicanTransferCommon(ClassAd *job_ad, bool peer_supports_pelican);

// Callback for libcurl to capture response data
static size_t PelicanCurlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	std::string *response = static_cast<std::string*>(userp);
	response->append(static_cast<char*>(contents), realsize);
	return realsize;
}

// Common checks for Pelican file transfer eligibility
// Returns true if basic requirements are met, false otherwise
bool ShouldUsePelicanTransferCommon(ClassAd *job_ad, bool peer_supports_pelican) {
	if (!job_ad) {
		return false;
	}

	// Check if Pelican is enabled via configuration
	bool pelican_enabled = param_boolean("ENABLE_PELICAN_FILE_TRANSFER", false);
	if (!pelican_enabled) {
		return false;
	}

	// Pelican transfers require HTCondor 25.7.0 or later on both sides
	// for the new TransferSubCommand::DownloadUrlWithAd protocol
	if (!peer_supports_pelican) {
		dprintf(D_FULLDEBUG, "Pelican: Skipping - peer doesn't support Pelican (< 25.7.0)\n");
		return false;
	}

	// Check if specific users/jobs are allowed to use Pelican
	std::string pelican_users;
	param(pelican_users, "PELICAN_ALLOWED_USERS");
	if (!pelican_users.empty()) {
		std::string owner;
		bool found_owner = false;
		if (job_ad->LookupString(ATTR_OWNER, owner)) {
			for (const auto& allowed_user: StringTokenIterator(pelican_users)) {
				if (owner == allowed_user) {
					dprintf(D_FULLDEBUG, "Pelican: User %s is allowed for Pelican transfer\n", owner.c_str());
					found_owner = true;
					break;
				}
			}
		}
		if (!found_owner) {
			dprintf(D_FULLDEBUG, "Pelican: User %s is not allowed for Pelican transfer\n", owner.c_str());
			return false;
		}
	}

	return true;
}

} // anonymous namespace

// Check if Pelican file transfer should be used for input sandbox
// Returns true if Pelican should be used, false otherwise
bool ShouldInputUsePelicanTransfer(ClassAd *job_ad, bool peer_supports_pelican) {
	if (!ShouldUsePelicanTransferCommon(job_ad, peer_supports_pelican)) {
		return false;
	}

	// Check for any complex functionality that might not be supported yet
	// For now, we'll be conservative and only enable for basic transfers
	
	// Check if directory transfers are requested - skip Pelican for now
	std::string transfer_input_files;
	if (job_ad->LookupString(ATTR_TRANSFER_INPUT_FILES, transfer_input_files)) {
		// Simple heuristic: if there's a "/" at the end of any path, it's a directory
		if (transfer_input_files.find("/,") != std::string::npos ||
		    transfer_input_files.find("/ ") != std::string::npos ||
		    (transfer_input_files.length() > 0 && transfer_input_files.back() == '/')) {
			dprintf(D_FULLDEBUG, "Pelican: Skipping input transfer due to directory transfer\n");
			return false;
		}
	}

	dprintf(D_FULLDEBUG, "Pelican: Input transfer enabled for this job\n");
	return true;
}

// Check if Pelican file transfer should be used for output sandbox
// Returns true if Pelican should be used, false otherwise
bool ShouldOutputUsePelicanTransfer(ClassAd *job_ad, bool peer_supports_pelican) {
	if (!ShouldUsePelicanTransferCommon(job_ad, peer_supports_pelican)) {
		return false;
	}

	// Currently no output-specific restrictions
	// Future: could check output file patterns, sizes, etc.

	dprintf(D_FULLDEBUG, "Pelican: Output transfer enabled for this job\n");
	return true;
}

// Request a token from the Pelican registration service via domain socket
// This posts the job ad to the service and receives back a token, expiration time, and transfer URLs
PelicanRegistrationResponse PelicanRegisterJob(ClassAd *job_ad, const std::string &socket_path) {
	PelicanRegistrationResponse response;
	response.success = false;
	response.expires_at = 0;

	if (!job_ad) {
		response.error_message = "Null job ad provided";
		return response;
	}

	// Initialize curl
	CURL *curl = curl_easy_init();
	if (!curl) {
		response.error_message = "Failed to initialize curl";
		return response;
	}

	// Serialize the job ad to JSON format
	std::string job_ad_json;
	classad::ClassAdJsonUnParser unparser;
	unparser.Unparse(job_ad_json, job_ad);

	std::string response_string;
	struct curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");

	// Configure curl to use unix domain socket
	std::string url = "http://localhost/api/v1/sandbox/register";
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, socket_path.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, job_ad_json.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, PelicanCurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);  // 10 second timeout

	// Perform the request
	CURLcode res = curl_easy_perform(curl);
	
	if (res != CURLE_OK) {
		formatstr(response.error_message, "Curl request failed: %s", curl_easy_strerror(res));
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		return response;
	}

	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (http_code != 200) {
		formatstr(response.error_message, "HTTP error code: %ld", http_code);
		return response;
	}

	// Parse JSON response using ClassAd's JSON parser
	ClassAd json_ad;
	classad::ClassAdJsonParser parser;
	
	if (!parser.ParseClassAd(response_string, json_ad)) {
		response.error_message = "Failed to parse JSON response from registration service";
		return response;
	}

	// Extract token, expiration, and URLs from response
	std::string token_str;
	long long expires_at_val = 0;
	std::string input_url_str;
	std::string output_url_str;

	if (!json_ad.LookupString("token", token_str) || !json_ad.LookupInteger("expires_at", expires_at_val)) {
		response.error_message = "Response missing required fields (token, expires_at)";
		return response;
	}

	if (!json_ad.LookupString("input_url", input_url_str)) {
		response.error_message = "Response missing required field (input_url)";
		return response;
	}
	if (!json_ad.LookupString("output_url", output_url_str)) {
		response.error_message = "Response missing required field (output_url)";
		return response;
	}

	response.token = token_str;
	response.expires_at = expires_at_val;
	response.input_url = input_url_str;
	response.output_url = output_url_str;
	response.success = true;

	dprintf(D_FULLDEBUG, "Pelican: Successfully obtained token, expires at %ld\n", response.expires_at);
	if (!input_url_str.empty()) {
		dprintf(D_FULLDEBUG, "Pelican: Input URL: %s\n", input_url_str.c_str());
	}
	if (!output_url_str.empty()) {
		dprintf(D_FULLDEBUG, "Pelican: Output URL: %s\n", output_url_str.c_str());
	}
	return response;
}

// Prepare file list for Pelican transfer
// Removes regular files from the list and replaces with Pelican URL + metadata
// Returns true on success, false on failure
bool PreparePelicanInputTransferList(FileTransferList &filelist, ClassAd *job_ad, const std::string &jobid, FileTransfer *ft, ReliSock *sock, DCTransferQueue &xfer_queue, filesize_t sandbox_size, FTProtocolBits &protocolState) {
	dprintf(D_FULLDEBUG, "Pelican: Preparing file list for job %s\n", jobid.c_str());

	std::vector<std::string> desired_schemes = {"pelican"};
	std::string error_msg;
	if (!ft->CheckUrlSchemeSupport(sock, desired_schemes, error_msg, xfer_queue, sandbox_size, protocolState)) {
		dprintf(D_FULLDEBUG, "Pelican: Peer does not support pelican:// scheme: %s\n", error_msg.c_str());
		return false;
	}

	// Get the Pelican registration service socket path from configuration
	std::string socket_path;
	if (!param(socket_path, "PELICAN_REGISTRATION_SOCKET")) {
		dprintf(D_ALWAYS, "Pelican: Configuration error - PELICAN_REGISTRATION_SOCKET not set\n");
		return false;
	}

	// Request a token from the Pelican service
	PelicanRegistrationResponse token_response = PelicanRegisterJob(job_ad, socket_path);
	if (!token_response.success) {
		dprintf(D_ALWAYS, "Pelican: Failed to obtain token: %s\n", token_response.error_message.c_str());
		return false;
	}

	// Get the input URL from token response
	if (token_response.input_url.empty()) {
		dprintf(D_ALWAYS, "Pelican: Registration response missing input_url\n");
		return false;
	}

	std::string pelican_url = token_response.input_url;
	dprintf(D_FULLDEBUG, "Pelican: Using input URL from registration: %s\n", pelican_url.c_str());

	// Get CA contents if needed
	std::string ca_contents;
	std::string ca_file;
	if (param(ca_file, "PELICAN_CA_FILE")) {
		std::ifstream ca_stream(ca_file);
		if (ca_stream) {
			ca_contents.assign(std::istreambuf_iterator<char>(ca_stream), std::istreambuf_iterator<char>());
			dprintf(D_FULLDEBUG, "Pelican: Loaded CA file (%zu bytes)\n", ca_contents.size());
		} else {
			dprintf(D_ALWAYS, "Pelican: Warning - could not read CA file %s\n", ca_file.c_str());
		}
	}

	// Remove all non-URL, file transfer items
	// These will now be handled by Pelican
	auto it = filelist.begin();
	while (it != filelist.end()) {
		if (!it->isSrcUrl()) {
			dprintf(D_FULLDEBUG, "Pelican: Removing %s from transfer list (will use Pelican)\n",
			        it->srcName().c_str());
			it = filelist.erase(it);
		} else {
			++it;
		}
	}

	// Create a ClassAd with Pelican transfer metadata
	auto pelican_ad = std::make_unique<ClassAd>();
	pelican_ad->Assign("PelicanToken", token_response.token);
	pelican_ad->Assign("PelicanTokenExpires", (long long)token_response.expires_at);
	if (!ca_contents.empty()) {
		pelican_ad->Assign("PelicanCAContents", ca_contents);
	}
	pelican_ad->Assign("PelicanTransfer", true);

	// Add a FileTransferItem for the Pelican URL
	FileTransferItem pelican_item;
	pelican_item.setSrcName(pelican_url);
	pelican_item.setUrlAd(std::move(pelican_ad));
	filelist.push_back(std::move(pelican_item));

	dprintf(D_FULLDEBUG, "Pelican: Successfully prepared transfer list\n");
	return true;
}

// Build a response ClassAd for a Pelican URL metadata request
// Called by shadow when starter requests metadata for output transfers
// Returns true if Pelican metadata was successfully added, false otherwise
bool BuildPelicanMetadataResponse(const std::string& url, ClassAd *job_ad, ClassAd& response_ad) {
	if (!starts_with_ignore_case(url, "pelican://")) {
		return false;
	}

	if (!ShouldOutputUsePelicanTransfer(job_ad, true)) {
		response_ad.Assign("Error", "Pelican transfer not available");
		return false;
	}

	// Get the Pelican registration service socket path from configuration
	std::string socket_path;
	if (!param(socket_path, "PELICAN_REGISTRATION_SOCKET")) {
		dprintf(D_ALWAYS, "BuildPelicanMetadataResponse: Configuration error - PELICAN_REGISTRATION_SOCKET not set\n");
		response_ad.Assign("Error", "Pelican configuration error - missing PELICAN_REGISTRATION_SOCKET");
		return false;
	}

	// Request token from Pelican service
	PelicanRegistrationResponse token_response = PelicanRegisterJob(job_ad, socket_path);

	if (!token_response.success) {
		dprintf(D_ALWAYS, "BuildPelicanMetadataResponse: Failed to get Pelican token: %s\n",
		        token_response.error_message.c_str());
		response_ad.Assign("Error", token_response.error_message);
		return false;
	}

	// Success - add token, URL, and CA info to response
	response_ad.Assign("PelicanToken", token_response.token);
	response_ad.Assign("PelicanTokenExpires", (long long)token_response.expires_at);

	// Return the actual output URL from registration
	if (token_response.output_url.empty()) {
		dprintf(D_ALWAYS, "BuildPelicanMetadataResponse: Registration response missing output_url\n");
		response_ad.Assign("Error", "Registration response missing output_url");
		return false;
	}
	response_ad.Assign("PelicanOutputUrl", token_response.output_url);

	// Read CA file if configured
	std::string ca_file;
	if (param(ca_file, "PELICAN_CA_FILE")) {
		std::string ca_contents;
		std::ifstream ca_stream(ca_file);
		if (ca_stream.good()) {
			ca_contents.assign(std::istreambuf_iterator<char>(ca_stream),
			                   std::istreambuf_iterator<char>());
			if (!ca_contents.empty()) {
				response_ad.Assign("PelicanCAContents", ca_contents);
			}
		}
	}

	dprintf(D_FULLDEBUG, "BuildPelicanMetadataResponse: Successfully provided Pelican metadata\n");
	return true;
}

// Prepare output sandbox files for Pelican transfer
// Similar to PreparePelicanTransferList but for output/upload side
bool PreparePelicanOutputTransferList(
	FileTransfer *ft,
	ReliSock *sock,
	FileTransferList& filelist,
	ClassAd *job_ad,
	bool peer_supports_pelican,
	DCTransferQueue &xfer_queue,
	filesize_t sandbox_size,
	FTProtocolBits &protocolState)
{
	if (!ShouldOutputUsePelicanTransfer(job_ad, peer_supports_pelican)) {
		return false;  // Not an error, just don't use Pelican
	}

	// Get the job ID for the metadata request
	int cluster_id = 0, proc_id = 0;
	if (!job_ad->LookupInteger(ATTR_CLUSTER_ID, cluster_id) ||
	    !job_ad->LookupInteger(ATTR_PROC_ID, proc_id)) {
		dprintf(D_ALWAYS, "Pelican: Failed to get job ID from ClassAd\n");
		return false;
	}

	// Construct a placeholder URL for the metadata request
	// The actual URL will come from the metadata response
	std::string placeholder_url;
	formatstr(placeholder_url, "pelican://placeholder/sandboxes/%d.%d/output",
	          cluster_id, proc_id);

	// Request metadata (token, actual URL, CA) from the shadow
	ClassAd metadata_ad;
	std::string error_msg;
	if (!ft->RequestUrlMetadata(sock, placeholder_url, metadata_ad, error_msg, xfer_queue, sandbox_size, protocolState)) {
		dprintf(D_ALWAYS, "Pelican: Failed to get metadata for %s: %s\n",
		        placeholder_url.c_str(), error_msg.c_str());
		return false;
	}

	// Extract the actual Pelican URL from metadata response
	std::string pelican_url;
	if (!metadata_ad.LookupString("PelicanOutputUrl", pelican_url)) {
		dprintf(D_ALWAYS, "Pelican: Metadata response missing PelicanOutputUrl\n");
		return false;
	}
	metadata_ad.Delete("PelicanOutputUrl");

	dprintf(D_FULLDEBUG, "Pelican: Using output URL from metadata: %s\n", pelican_url.c_str());

	// Create a single FileTransferItem with a tarfiles list containing all non-URL files
	FileTransferItem pelican_item;

	// Set as destination URL (for protocol routing)
	pelican_item.setDestUrl(pelican_url);

	// Create ClassAd with transfer information
	auto file_ad = std::make_unique<ClassAd>();

	// Build list of file paths to include in tarball
	classad::ExprList *tarfiles_list = new classad::ExprList();
	for (const auto& item : filelist) {
		if (!item.isSrcUrl() && !item.isDirectory()) {
			// Add file path to tarfiles list
			classad::Value file_value;
			file_value.SetStringValue(item.srcName());
			tarfiles_list->push_back(classad::Literal::MakeLiteral(file_value));

			dprintf(D_FULLDEBUG, "Pelican: Adding output file %s to tarball\n",
			        item.srcName().c_str());
		}
	}

	if (tarfiles_list->size() == 0) {
		delete tarfiles_list;
		dprintf(D_FULLDEBUG, "Pelican: No output files to transfer\n");
		return false;
	}

	// Add tarfiles list to ClassAd
	file_ad->Insert("tarfiles", tarfiles_list);

	// Copy metadata from the metadata response
	file_ad->Update(metadata_ad);

	pelican_item.setUrlAd(std::move(file_ad));

	// Remove non-URL files from the original transfer list
	auto it = filelist.begin();
	while (it != filelist.end()) {
		if (!it->isSrcUrl() && !it->isDirectory()) {
			dprintf(D_FULLDEBUG, "Pelican: Removing %s from normal transfer list\n",
			        it->srcName().c_str());
			it = filelist.erase(it);
		} else {
			++it;
		}
	}
	
	// Add the single Pelican item to the file list
	filelist.push_back(std::move(pelican_item));
	
	dprintf(D_FULLDEBUG, "Pelican: Successfully prepared output files for transfer\n");
	return true;
}
