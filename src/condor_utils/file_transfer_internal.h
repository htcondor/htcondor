/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
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

#ifndef _FILE_TRANSFER_INTERNAL_H
#define _FILE_TRANSFER_INTERNAL_H

#include <vector>
#include <string>
#include <memory>

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_url.h"
#include "cedar_enums.h"
#include "stream.h"

// Forward declarations
class ReliSock;

// Internal implementation details for file transfer
// This header should only be included by file_transfer.cpp and related implementation files

class FileTransferItem {
public:
	// Copy constructor - needed for vector copy construction
	// Deep copies the ClassAd if present
	FileTransferItem(const FileTransferItem& other);

	// Copy assignment operator
	FileTransferItem& operator=(const FileTransferItem& other);

	// Default constructor, destructor, and move operations
	FileTransferItem() = default;
	~FileTransferItem() = default;
	FileTransferItem(FileTransferItem&&) = default;
	FileTransferItem& operator=(FileTransferItem&&) = default;

	const std::string &srcName() const { return m_src_name; }
	const std::string &destDir() const { return m_dest_dir; }
	const std::string &destUrl() const { return m_dest_url; }
	const std::string &srcScheme() const { return m_src_scheme; }
	const std::string &xferQueue() const { return m_xfer_queue; }
	filesize_t fileSize() const { return m_file_size; }
	void setDestDir(const std::string &dest) { m_dest_dir = dest; }
	void setXferQueue(const std::string &queue) { m_xfer_queue = queue; }
	void setFileSize(filesize_t new_size) { m_file_size = new_size; }
	void setDomainSocket(bool value) { is_domainsocket = value; }
	void setSymlink(bool value) { is_symlink = value; }
	void setDirectory(bool value) { is_directory = value; }
	bool isDomainSocket() const {return is_domainsocket;}
	bool isSymlink() const {return is_symlink;}
	bool isDirectory() const {return is_directory;}
	bool isSrcUrl() const {return !m_src_scheme.empty();}
	bool isDestUrl() const {return !m_dest_scheme.empty();}
	bool hasQueue() const { return !m_xfer_queue.empty(); }
	condor_mode_t fileMode() const {return m_file_mode;}
	void setFileMode(condor_mode_t new_mode) {m_file_mode = new_mode;}

	// Optional ClassAd for URL transfers (e.g., Pelican with token)
	ClassAd* getUrlAd() const { return m_url_ad.get(); }
	void setUrlAd(std::unique_ptr<ClassAd> ad) { m_url_ad = std::move(ad); }

	void setSrcName(const std::string &src);

	void setDestUrl(const std::string &dest_url);

	bool operator<(const FileTransferItem &other) const;

private:
	std::string m_src_scheme;
	std::string m_dest_scheme;
	std::string m_src_name;
	std::string m_dest_dir;
	std::string m_dest_url;
	std::string m_xfer_queue;
	bool is_domainsocket{false};
	bool is_directory{false};
	bool is_symlink{false};
	condor_mode_t m_file_mode{NULL_FILE_PERMISSIONS};
	filesize_t m_file_size{0};
	std::unique_ptr<ClassAd> m_url_ad; // For URL transfers with metadata
};

typedef std::vector<FileTransferItem> FileTransferList;

#endif // _FILE_TRANSFER_INTERNAL_H
