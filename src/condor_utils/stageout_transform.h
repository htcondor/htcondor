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

#ifndef _STAGEOUT_TRANSFORM_H
#define _STAGEOUT_TRANSFORM_H

#include "condor_common.h"
#include "file_transfer.h"
#include <vector>

// Forward declarations
class FileTransferItem;
class FileTransfer;
class ReliSock;
namespace classad { class ClassAd; }

/**
 * Serializes a single FileTransferItem object to a ClassAd.
 * Converts an item to a ClassAd representation suitable for sending to shadow.
 * Call this one-at-a-time to avoid memory issues with large numbers of items.
 *
 * @param item The FileTransferItem object to serialize
 * @param ad Output parameter for the resulting ClassAd
 * @return True on success, false on failure
 */
bool SerializeTransferItem(const FileTransferItem& item, classad::ClassAd& ad);

/**
 * Deserializes a single ClassAd back into a FileTransferItem object.
 * Reconstructs a FileTransferItem from a ClassAd returned by shadow.
 * Call this one-at-a-time to avoid memory issues with large numbers of items.
 *
 * @param ad The ClassAd to deserialize
 * @param success Output parameter set to true on success, false on error
 * @return A FileTransferItem object (invalid if success is false)
 */
FileTransferItem DeserializeTransferItem(const classad::ClassAd& ad, bool& success);

/**
 * Sends the stageout file list to the shadow for transformation.
 * The starter sends its complete list of transfer items to the shadow,
 * which can then decide how to handle them (e.g., redirect to Pelican).
 *
 * @param ft The FileTransfer object (needed for go-ahead functions)
 * @param sock The socket connection to the shadow
 * @param filelist The list of files to transform
 * @param error_msg Output parameter for error messages
 * @param xfer_queue The transfer queue for go-ahead exchange
 * @param sandbox_size The size of the sandbox
 * @param protocolState The current protocol state
 * @return The transformed file list from the shadow, or the original list on error
 */
std::vector<FileTransferItem> RequestStageoutTransform(
	FileTransfer *ft,
	ReliSock *sock,
	const std::vector<FileTransferItem>& filelist,
	std::string& error_msg,
	class DCTransferQueue &xfer_queue,
	filesize_t sandbox_size,
	struct FTProtocolBits &protocolState
);

/**
 * Handles the shadow-side of the stageout transform protocol.
 * Receives file transfer items from the starter, transforms them (e.g., for Pelican),
 * and sends back the transformed list.
 *
 * @param sock The socket connection to the starter
 * @param job_ad The job ClassAd (for accessing job attributes)
 * @param item_count The number of items to receive (from request ClassAd)
 * @return True on success, false on failure
 */
bool HandleStageoutTransform(
	ReliSock *sock,
	ClassAd *job_ad,
	long long item_count
);

#endif /* _STAGEOUT_TRANSFORM_H */
