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
#include "stageout_transform.h"
#include "file_transfer_internal.h"
#include "pelican_transfer.h"
#include "compat_classad.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_io.h"
#include <fstream>

/**
 * Serializes a single FileTransferItem to a ClassAd.
 * Each field is converted to an appropriate ClassAd attribute.
 * Call this one-at-a-time to avoid memory issues with large numbers of items.
 *
 * @param item The FileTransferItem to serialize
 * @param ad Output parameter for the resulting ClassAd
 * @return True on success, false on failure
 */
bool SerializeTransferItem(const FileTransferItem& item, classad::ClassAd& ad) {
	// Required fields
	if (!ad.InsertAttr("SrcName", item.srcName())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert SrcName\n");
		return false;
	}
	if (!ad.InsertAttr("DestDir", item.destDir())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert DestDir\n");
		return false;
	}

	// Destination URL (mandatory)
	if (!ad.InsertAttr("DestUrl", item.destUrl())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert DestUrl\n");
		return false;
	}

	// Source scheme (mandatory)
	if (!ad.InsertAttr("SrcScheme", item.srcScheme())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert SrcScheme\n");
		return false;
	}

	// Transfer queue (mandatory)
	if (!ad.InsertAttr("XferQueue", item.xferQueue())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert XferQueue\n");
		return false;
	}

	// File metadata (mandatory)
	if (!ad.InsertAttr("FileSize", item.fileSize())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert FileSize\n");
		return false;
	}
	if (!ad.InsertAttr("FileMode", (int)item.fileMode())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert FileMode\n");
		return false;
	}

	// Flags (mandatory)
	if (!ad.InsertAttr("IsDomainSocket", item.isDomainSocket())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert IsDomainSocket\n");
		return false;
	}
	if (!ad.InsertAttr("IsSymlink", item.isSymlink())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert IsSymlink\n");
		return false;
	}
	if (!ad.InsertAttr("IsDirectory", item.isDirectory())) {
		dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert IsDirectory\n");
		return false;
	}

	// Optional URL Ad (for Pelican with token or other metadata)
	if (item.getUrlAd() != nullptr) {
		classad::ClassAd *url_ad_copy = new classad::ClassAd(*item.getUrlAd());
		if (!ad.Insert("UrlAd", url_ad_copy)) {
			dprintf(D_ALWAYS, "SerializeTransferItem: Failed to insert UrlAd\n");
			delete url_ad_copy;
			return false;
		}
	}

	return true;
}

/**
 * Deserializes a single ClassAd to a FileTransferItem.
 * Reconstructs all fields from the ClassAd representation.
 * Call this one-at-a-time to avoid memory issues with large numbers of items.
 *
 * @param ad The ClassAd to deserialize
 * @param success Output parameter set to true on success, false on error
 * @return A FileTransferItem with the deserialized data (invalid if success is false)
 */
FileTransferItem DeserializeTransferItem(const classad::ClassAd& ad, bool& success) {
	FileTransferItem item;
	success = false;

	std::string src_name, dest_dir, dest_url, src_scheme, xfer_queue;
	long long file_size = 0;
	int file_mode = 0;
	bool is_domain_socket = false, is_symlink = false, is_directory = false;

	// Extract all required fields - error out if any are missing
	if (!ad.EvaluateAttrString("SrcName", src_name)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid SrcName\n");
		return item;
	}
	if (!ad.EvaluateAttrString("DestDir", dest_dir)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid DestDir\n");
		return item;
	}
	if (!ad.EvaluateAttrString("DestUrl", dest_url)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid DestUrl\n");
		return item;
	}
	if (!ad.EvaluateAttrString("SrcScheme", src_scheme)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid SrcScheme\n");
		return item;
	}
	if (!ad.EvaluateAttrString("XferQueue", xfer_queue)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid XferQueue\n");
		return item;
	}
	if (!ad.EvaluateAttrNumber("FileSize", file_size)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid FileSize\n");
		return item;
	}
	if (!ad.EvaluateAttrNumber("FileMode", file_mode)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid FileMode\n");
		return item;
	}
	if (!ad.EvaluateAttrBool("IsDomainSocket", is_domain_socket)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid IsDomainSocket\n");
		return item;
	}
	if (!ad.EvaluateAttrBool("IsSymlink", is_symlink)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid IsSymlink\n");
		return item;
	}
	if (!ad.EvaluateAttrBool("IsDirectory", is_directory)) {
		dprintf(D_ALWAYS, "DeserializeTransferItem: Missing or invalid IsDirectory\n");
		return item;
	}

	item.setSrcName(src_name);
	item.setDestDir(dest_dir);
	item.setDestUrl(dest_url);
	item.setXferQueue(xfer_queue);
	item.setFileSize(file_size);
	item.setFileMode((condor_mode_t)file_mode);
	item.setDomainSocket(is_domain_socket);
	item.setSymlink(is_symlink);
	item.setDirectory(is_directory);

	// Optional URL Ad
	classad::ExprTree *url_ad_expr = ad.Lookup("UrlAd");
	if (url_ad_expr) {
		classad::ClassAd *url_ad = dynamic_cast<classad::ClassAd*>(url_ad_expr);
		if (url_ad) {
			auto ad_ptr = std::make_unique<classad::ClassAd>(*url_ad);
			item.setUrlAd(std::move(ad_ptr));
		}
	}

	success = true;
	return item;
}

/**
 * Sends the stageout file list to the shadow for transformation.
 * The starter sends its complete list of transfer items to the shadow,
 * which can then decide how to handle them (e.g., redirect to Pelican).
 */
std::vector<FileTransferItem> RequestStageoutTransform(
	FileTransfer *ft,
	ReliSock *sock,
	const std::vector<FileTransferItem>& filelist,
	std::string& error_msg,
	DCTransferQueue &xfer_queue,
	filesize_t sandbox_size,
	FTProtocolBits &protocolState)
{
	dprintf(D_FULLDEBUG, "RequestStageoutTransform: Starting with %zu items\n", filelist.size());
	std::vector<FileTransferItem> result;

	if (!ft || !sock) {
		error_msg = "Invalid FileTransfer or socket connection";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;  // Return original list on error
	}

	// Send the TransferCommand::Other header
	if (!sock->snd_int(static_cast<int>(TransferCommand::Other), false) ||
	    !sock->end_of_message()) {
		error_msg = "Failed to send TransferCommand";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Send a dummy filename (required by protocol)
	if (!sock->put("_stageout_transform_") || !sock->end_of_message()) {
		error_msg = "Failed to send filename";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Here, we must wait for the go-ahead from the transfer peer.
	if (!ft->ReceiveTransferGoAhead(sock, "", false, protocolState.peer_goes_ahead_always, protocolState.peer_max_transfer_bytes)) {
		error_msg = "Failed to receive go-ahead from peer";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Obtain the transfer token from the transfer queue.
	if (!ft->ObtainAndSendTransferGoAhead(xfer_queue, false, sock, sandbox_size, "", protocolState.I_go_ahead_always)) {
		error_msg = "Failed to obtain and send go-ahead";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Send the ClassAd with subcommand and the count of transfer items
	sock->encode();
	ClassAd request_ad;
	request_ad.Assign("SubCommand", static_cast<int>(TransferSubCommand::StageoutTransform));
	request_ad.Assign("ItemCount", (long long)filelist.size());
	
	dprintf(D_FULLDEBUG, "RequestStageoutTransform: Sending %zu items to shadow\n", filelist.size());

	if (!putClassAd(sock, request_ad)) {
		error_msg = "Failed to send request ClassAd";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Send each transfer item as a separate ClassAd to avoid memory bloat
	for (const auto& item : filelist) {
		classad::ClassAd item_ad;
		if (!SerializeTransferItem(item, item_ad)) {
			error_msg = "Failed to serialize item ClassAd";
			dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
			return filelist;
		}
		if (!putClassAd(sock, item_ad)) {
			error_msg = "Failed to send item ClassAd";
			dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
			return filelist;
		}
	}

	if (!sock->end_of_message()) {
		error_msg = "Failed to send end of message after items";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Receive the response ClassAd with item count
	sock->decode();
	ClassAd response_ad;
	if (!getClassAd(sock, response_ad) || !sock->end_of_message()) {
		error_msg = "Failed to receive response ClassAd";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Check if there was an error
	std::string remote_error;
	if (response_ad.LookupString("Error", remote_error)) {
		formatstr(error_msg, "Remote error: %s", remote_error.c_str());
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Extract the count of transformed items
	long long item_count = 0;
	if (!response_ad.LookupInteger("ItemCount", item_count)) {
		error_msg = "Response missing ItemCount field";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	// Receive each transformed item as a separate ClassAd
	result.reserve(item_count);
	for (long long i = 0; i < item_count; i++) {
		classad::ClassAd item_ad;
		if (!getClassAd(sock, item_ad)) {
			error_msg = "Failed to receive item ClassAd";
			dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
			return filelist;
		}

		bool success = false;
		FileTransferItem item = DeserializeTransferItem(item_ad, success);
		if (!success) {
			error_msg = "Failed to deserialize item ClassAd";
			dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
			return filelist;
		}
		result.push_back(std::move(item));
	}

	if (!sock->end_of_message()) {
		error_msg = "Failed to receive end of message after items";
		dprintf(D_ALWAYS, "RequestStageoutTransform: %s\n", error_msg.c_str());
		return filelist;
	}

	sock->encode();  // Leave socket in encode mode for further transfers

	dprintf(D_FULLDEBUG, "RequestStageoutTransform: Received %zu transformed items from shadow\n", result.size());
	return result;
}

/**
 * Handles the shadow-side of the stageout transform protocol.
 * Receives file transfer items from the starter, transforms them (e.g., for Pelican),
 * and sends back the transformed list.
 */
bool HandleStageoutTransform(
	ReliSock *sock,
	ClassAd *job_ad,
	long long item_count)
{
	if (!sock) {
		dprintf(D_ALWAYS, "HandleStageoutTransform: Invalid socket\n");
		return false;
	}

	// Validate item_count to prevent excessive memory allocation on errors
	// Use a reasonable upper bound (1,000,000; would be an excessive use case
	// for CEDAR transfers)
	const long long MAX_TRANSFER_ITEMS = 1000000;
	if (item_count < 0 || item_count > MAX_TRANSFER_ITEMS) {
		dprintf(D_ALWAYS, "HandleStageoutTransform: Invalid item_count %lld (max %lld)\n",
			item_count, MAX_TRANSFER_ITEMS);
		return false;
	}

	// Receive all items from the starter
	// (request ClassAd with ItemCount already consumed by caller)
	dprintf(D_FULLDEBUG, "HandleStageoutTransform: Receiving %lld items from starter\n", item_count);

	std::vector<FileTransferItem> filelist;
	filelist.reserve(item_count);
	for (long long i = 0; i < item_count; i++) {
		classad::ClassAd item_ad;
		if (!getClassAd(sock, item_ad)) {
			dprintf(D_ALWAYS, "HandleStageoutTransform: Failed to receive item %lld\n", i);
			return false;
		}

		bool success = false;
		FileTransferItem item = DeserializeTransferItem(item_ad, success);
		if (!success) {
			dprintf(D_ALWAYS, "HandleStageoutTransform: Failed to deserialize item %lld\n", i);
			return false;
		}
		filelist.push_back(std::move(item));
	}

	if (!sock->end_of_message()) {
		dprintf(D_ALWAYS, "HandleStageoutTransform: Failed to receive end of message after items\n");
		return false;
	}

	dprintf(D_FULLDEBUG, "HandleStageoutTransform: Received %zu items, beginning transformation\n", filelist.size());

	// Transform the file list for Pelican if applicable
	// Make a copy so we can send back the original if transformation fails
	std::vector<FileTransferItem> transformed_list = filelist;
	ClassAd response_ad;

	if (ShouldOutputUsePelicanTransfer(job_ad, true)) {
		// Transform the list using Pelican
		if (!TransformPelicanOutputList(transformed_list, job_ad)) {
			dprintf(D_ALWAYS, "HandleStageoutTransform: Failed to transform output list for Pelican\n");
			response_ad.Assign("Error", "Failed to transform output list for Pelican");
			// Fall back to original list
			transformed_list = filelist;
		}
	} else {
		// Not using Pelican - just pass through the original list
		dprintf(D_FULLDEBUG, "HandleStageoutTransform: Not using Pelican, passing through original list\n");
	}

	// Send response ClassAd with transformed item count
	response_ad.Assign("ItemCount", (long long)transformed_list.size());

	sock->encode();
	if (!putClassAd(sock, response_ad) || !sock->end_of_message()) {
		dprintf(D_ALWAYS, "HandleStageoutTransform: Failed to send response ClassAd\n");
		return false;
	}

	// Send each transformed item
	for (const auto& item : transformed_list) {
		classad::ClassAd item_ad;
		if (!SerializeTransferItem(item, item_ad)) {
			dprintf(D_ALWAYS, "HandleStageoutTransform: Failed to serialize transformed item\n");
			return false;
		}
		if (!putClassAd(sock, item_ad)) {
			dprintf(D_ALWAYS, "HandleStageoutTransform: Failed to send transformed item\n");
			return false;
		}
	}

	if (!sock->end_of_message()) {
		dprintf(D_ALWAYS, "HandleStageoutTransform: Failed to send end of message after items\n");
		return false;
	}

	// Leave socket in decode mode as the calling function assumes a decode EOM is next.
	sock->decode();

	dprintf(D_FULLDEBUG, "HandleStageoutTransform: Successfully sent %zu transformed items to starter\n",
		transformed_list.size());
	return true;
}
