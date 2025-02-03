/***************************************************************
 *
 * Copyright (C) 201999999999, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"
#include "data_reuse.h"

#include "CondorError.h"
#include "file_lock.h"
#include "directory.h"
#include "directory_util.h"
#include "condor_mkstemp.h"
#include "metric_units.h"

#include <stdio.h>

#include <algorithm>
#include <sstream>

#include <openssl/evp.h>

using namespace htcondor;


DataReuseDirectory::~DataReuseDirectory()
{
	if (m_owner) {
		Cleanup();
	}
}

DataReuseDirectory::DataReuseDirectory(const std::string &dirpath, bool owner) :
	m_owner(owner),
	m_dirpath(dirpath),
	m_state_name(dircat(m_dirpath.c_str(), "use.log", m_logname))
{
	OpenSSL_add_all_digests();

	if (m_owner) {
		Cleanup();
		CreatePaths();
	}

	m_log.initialize(m_state_name.c_str(), 0, 0, 0),
	m_rlog.initialize(m_state_name.c_str(), false, false, false);

	std::string allocated_space_str;
	if (param(allocated_space_str, "DATA_REUSE_BYTES") && allocated_space_str.size()) {
		int64_t tmp_value;
		if (parse_int64_bytes(allocated_space_str.c_str(), tmp_value, 1)) {
			m_allocated_space = tmp_value;
		} else {
			dprintf(D_ALWAYS, "Invalid value for DATA_REUSE_BYTES (must be an integer, "
				"optionally with units like 'MB' or 'GB'): %s\n", allocated_space_str.c_str());
			return;
		}
	}

	dprintf(D_FULLDEBUG, "Allocating %llu bytes for the data reuse directory\n",
		static_cast<unsigned long long>(m_allocated_space));
	m_valid = true;

	CondorError err;
	LogSentry sentry = LockLog(err);
	if (!sentry.acquired()) {
		dprintf(D_FULLDEBUG, "Failed to acquire lock on state directory: %s\n",
			err.getFullText().c_str());
		return;
	}
	if (!UpdateState(sentry, err)) {
		dprintf(D_FULLDEBUG, "Failed to initialize state of reuse directory: %s\n",
			err.getFullText().c_str());
	}
}


void
DataReuseDirectory::CreatePaths()
{
	dprintf(D_FULLDEBUG, "Creating a new data reuse directory in %s\n", m_dirpath.c_str());
	if (!mkdir_and_parents_if_needed(m_dirpath.c_str(), 0700, 0700, PRIV_CONDOR)) {
		m_valid = false;
		return;
	}
	std::string subdir, subdir2;
	auto name = dircat(m_dirpath.c_str(), "tmp", subdir);
	if (!mkdir_and_parents_if_needed(name, 0700, 0700, PRIV_CONDOR)) {
		m_valid = false;
		return;
	}
	name = dircat(m_dirpath.c_str(), "sha256", subdir);
	char subdir_name[4];
	for (int idx = 0; idx < 256; idx++) {
		snprintf(subdir_name, sizeof(subdir_name), "%02x", idx);
		subdir_name[2] = '\0';
		auto name2 = dircat(name, subdir_name, subdir2);
		if (!mkdir_and_parents_if_needed(name2, 0700, 0700, PRIV_CONDOR)) {
			m_valid = false;
			return;
		}
	}
}


void
DataReuseDirectory::Cleanup() {
	Directory dir(m_dirpath.c_str());
	dir.Remove_Entire_Directory();
}


DataReuseDirectory::LogSentry::LogSentry(DataReuseDirectory &parent, CondorError &err)
	: m_parent(parent)
{
	m_lock = m_parent.m_log.getLock(err);
	if (m_lock == nullptr) {
		return;
	}

	m_acquired = m_lock->obtain(WRITE_LOCK);
}


DataReuseDirectory::LogSentry::~LogSentry()
{
	if (m_acquired) {
		m_lock->release();
	}
}


DataReuseDirectory::LogSentry
DataReuseDirectory::LockLog(CondorError &err)
{
	LogSentry sentry(*this, err);
	if (!sentry.acquired()) {
		err.push("DataReuse", 3, "Failed to acquire data reuse directory lockfile.");
	}
	return sentry;
}


std::string
DataReuseDirectory::FileEntry::fname(const std::string &dirpath,
	const std::string &checksum_type,
	const std::string &checksum,
	const std::string &tag)
{
	std::string hash_dir;
	dircat(dirpath.c_str(), checksum_type.c_str(), hash_dir);

	char hash_substring[3];
	hash_substring[2] = '\0';
	hash_substring[0] = checksum[0];
	hash_substring[1] = checksum[1];

	std::string file_dir;
	dircat(hash_dir.c_str(), hash_substring, file_dir);

	std::string fname;
	std::string hash_name(checksum.c_str() + 2, checksum.size()-2);
	hash_name += "." + tag;
	dircat(file_dir.c_str(), hash_name.c_str(), fname);

	return fname;
}


std::string
DataReuseDirectory::FileEntry::fname() const
{
	return fname(m_parent.m_dirpath, m_checksum_type, m_checksum, m_tag);
}


bool
DataReuseDirectory::ReserveSpace(uint64_t size, uint32_t time, const std::string &tag,
	std::string &id, CondorError &err)
{
	LogSentry sentry = LockLog(err);
	if (!sentry.acquired()) {
		return false;
	}

	if (!UpdateState(sentry, err)) {
		return false;
	}

	if ((m_reserved_space + size > m_allocated_space) && !ClearSpace(size, sentry, err))
	{
		err.pushf("DataReuse", 1, "Unable to allocate space; %llu bytes allocated, "
			"%llu bytes reserved, %llu additional bytes requested",
			static_cast<unsigned long long>(m_allocated_space),
			static_cast<unsigned long long>(m_reserved_space),
			static_cast<unsigned long long>(size));
		return false;
	}

	ReserveSpaceEvent event;
	auto now = std::chrono::system_clock::now();
	event.setExpirationTime(now + std::chrono::seconds(time));
	event.setReservedSpace(size);
	event.setTag(tag);
	std::string uuid_result = event.generateUUID();
	event.setUUID(uuid_result);
	if (!m_log.writeEvent(&event)) {
		err.push("DataReuse", 2, "Failed to write space reservation");
		return false;
	}
	id = uuid_result;

	return true;
}


bool
DataReuseDirectory::ClearSpace(uint64_t size, LogSentry &sentry, CondorError &err)
{
	if (!sentry.acquired()) {return false;}

	if (m_reserved_space + size <= m_allocated_space) {
		return true;
	}
	auto it = m_contents.begin();
	while (it != m_contents.end()) {
		auto &file_entry = **it;
		if (-1 == unlink(file_entry.fname().c_str())) {
			err.pushf("DataReuse", 4, "Failed to unlink cache entry: %s", strerror(errno));
			return false;
		}
		if (GetExtraDebug()) dprintf(D_FULLDEBUG, "Decreasing reserved space by %llu",
			static_cast<unsigned long long>(file_entry.size()));
		m_reserved_space -= file_entry.size();

		FileRemovedEvent event;
		event.setSize(file_entry.size());
		event.setChecksumType(file_entry.checksum_type());
		event.setChecksum(file_entry.checksum());
		event.setTag(file_entry.tag());
		it = m_contents.erase(it);
		if (!m_log.writeEvent(&event)) {
			err.push("DataReuse", 5, "Faild to write file deletion");
			return false;
		}

		if (m_reserved_space + size <= m_allocated_space) {
			return true;
		}
	}
	return false;
}


bool
DataReuseDirectory::UpdateState(LogSentry &sentry, CondorError &err)
{
	if (!sentry.acquired()) {return false;}

	struct stat stat_buf;
	{
		TemporaryPrivSentry sentry(PRIV_CONDOR);
		if (-1 == stat(m_state_name.c_str(), &stat_buf)) {
			err.pushf("DataReuse", 18, "Failed to stat the state file: %s.", strerror(errno));
			return false;
		}
	}
	if (0 == stat_buf.st_size) {
		return true;
	}

	bool all_done = false;
	do {
		ULogEvent *event = nullptr;
	#if 0  // can no longer pass a lock into the userlog reader
		auto outcome = m_rlog.readEventWithLock(event, *sentry.lock());
	#else
		auto outcome = m_rlog.readEvent(event);
	#endif


		switch (outcome) {
		case ULOG_OK:
			if (!HandleEvent(*event, err)) {return false;}
			break;
		case ULOG_NO_EVENT:
			all_done = true;
			break;
		case ULOG_RD_ERROR:
		case ULOG_UNK_ERROR:
		case ULOG_INVALID:
			dprintf(D_ALWAYS, "Failed to read reuse directory state file event.\n");
			return false;
		case ULOG_MISSED_EVENT:
			dprintf(D_ALWAYS, "Missed an event in the directory state file.\n");
			return false;
		};
	} while (!all_done);

	auto now = std::chrono::system_clock::now();
	auto iter = m_space_reservations.begin();
	while (iter != m_space_reservations.end()) {
		if (iter->second->getExpirationTime() < now) {
			dprintf(D_FULLDEBUG, "Expiring reservation %s\n.", iter->first.c_str());
			iter = m_space_reservations.erase(iter);
		} else {
			++iter;
		}
	}

	std::sort(m_contents.begin(), m_contents.end(),
		[](const std::unique_ptr<FileEntry> &left, const std::unique_ptr<FileEntry> &right) {
		return left->last_use() < right->last_use();
	});

	return true;
}

bool
DataReuseDirectory::HandleEvent(ULogEvent &event, CondorError & err)
{
	switch (event.eventNumber) {
	case ULOG_RESERVE_SPACE: {
		auto resEvent = static_cast<ReserveSpaceEvent&>(event);
		auto iter = m_space_reservations.find(resEvent.getUUID());
		if (iter == m_space_reservations.end()) {
			std::pair<std::string, std::unique_ptr<SpaceReservationInfo>> value(
				resEvent.getUUID(),
				std::unique_ptr<SpaceReservationInfo>(new SpaceReservationInfo(
					resEvent.getExpirationTime(),
					resEvent.getTag(),
					resEvent.getReservedSpace()))
			);
			m_space_reservations.insert(std::move(value));
			if (GetExtraDebug()) dprintf(D_FULLDEBUG, "Incrementing reserved space by %llu to %llu for UUID %s.\n",
				static_cast<unsigned long long>(resEvent.getReservedSpace()),
				static_cast<unsigned long long>(m_reserved_space + resEvent.getReservedSpace()),
				resEvent.getUUID().c_str());
			m_reserved_space += resEvent.getReservedSpace();
		} else if (iter->second->getTag() != resEvent.getTag()) {
			dprintf(D_ERROR, "Duplicate space reservation with incorrect tag (%s)\n",
				resEvent.getTag().c_str());
			err.pushf("DataReuse", 13, "Duplicate space reservation with incorrect tag (%s)",
				resEvent.getTag().c_str());
			return false;
		} else {
				// Duplicate matching reservation is interpreted as a lease extension.
			iter->second->setExpirationTime(resEvent.getExpirationTime());
		}
	}
		break;
	case ULOG_RELEASE_SPACE: {
		auto relEvent = static_cast<ReleaseSpaceEvent&>(event);
		auto iter = m_space_reservations.find(relEvent.getUUID());
		if (iter == m_space_reservations.end()) {
			dprintf(D_ALWAYS, "Release of space for reservation %s requested - but this"
				" reservation is unknown!\n", relEvent.getUUID().c_str());
			err.pushf("DataReuse", 14, "Release of space for reservation %s requested - but this"
				" reservation is unknown!", relEvent.getUUID().c_str());
			return false;
		}
		if (GetExtraDebug()) dprintf(D_FULLDEBUG, "Decrementing reserved space by %llu to %llu for UUID %s.\n",
			static_cast<unsigned long long>(iter->second->getReservedSpace()),
			static_cast<unsigned long long>(m_reserved_space - iter->second->getReservedSpace()), relEvent.getUUID().c_str());
		m_reserved_space -= iter->second->getReservedSpace();
		m_space_reservations.erase(iter);
		return true;
	}
		break;
	case ULOG_FILE_COMPLETE: {
		auto comEvent = static_cast<FileCompleteEvent&>(event);
		auto iter = m_space_reservations.find(comEvent.getUUID());
		if (iter == m_space_reservations.end()) {
			dprintf(D_ERROR, "File completed for non-existent space reservation %s.\n",
				comEvent.getUUID().c_str());
			err.pushf("DataReuse", 11, "File completed for non-existent space reservation %s",
				comEvent.getUUID().c_str());
			return false;
		}
		std::string fname = FileEntry::fname(m_dirpath, comEvent.getChecksumType(),
			comEvent.getChecksum(), iter->second->getTag());
		if (iter->second->getReservedSpace() < comEvent.getSize()) {
			dprintf(D_ERROR, "File completed with size %zu, which is larger than the space"
				" reservation size.\n", comEvent.getSize());
			err.pushf("DataReuse", 12, "File completed with size %zu, which is larger than the space"
				" reservation size.", comEvent.getSize());
			unlink(fname.c_str());
			return false;
		}
		auto event_time = std::chrono::system_clock::from_time_t(event.GetEventclock());
		if (iter->second->getExpirationTime() < event_time) {
			dprintf(D_ERROR, "File (checksum=%s, type=%s, tag=%s) completed at time %lu after "
				"space reservation %s expired at %lu.\n", comEvent.getChecksum().c_str(),
				comEvent.getChecksumType().c_str(), iter->second->getTag().c_str(),
				event.GetEventclock(), comEvent.getUUID().c_str(),
				std::chrono::system_clock::to_time_t(iter->second->getExpirationTime()));
			err.pushf("DataReuse", 16, "File (checksum=%s, type=%s, tag=%s) completed at time %lu after "
				"space reservation %s expired at %lu.\n", comEvent.getChecksum().c_str(),
				comEvent.getChecksumType().c_str(), iter->second->getTag().c_str(),
				event.GetEventclock(), comEvent.getUUID().c_str(),
				std::chrono::system_clock::to_time_t(iter->second->getExpirationTime()));
			unlink(fname.c_str());
			return false;
		}
		iter->second->setReservedSpace(iter->second->getReservedSpace() - comEvent.getSize());
		if (GetExtraDebug()) dprintf(D_FULLDEBUG, "For file completion, decrementing reserved space by %llu to %llu for UUID %s.\n",
			static_cast<unsigned long long>(comEvent.getSize()),
                        static_cast<unsigned long long>(m_reserved_space - comEvent.getSize()),
			comEvent.getUUID().c_str());
		m_reserved_space -= comEvent.getSize();

		bool already_has_file = false;
		for (const auto &entry : m_contents) {
			if ((entry->checksum() == comEvent.getChecksum()) &&
				(entry->checksum_type() == comEvent.getChecksumType()) &&
				(entry->tag() == iter->second->getTag()))
			{
				already_has_file = true;
				break;
			}
		}
		if (!already_has_file) {
			std::unique_ptr<FileEntry> entry(new FileEntry(*this,
				comEvent.getChecksum(),
				comEvent.getChecksumType(),
				iter->second->getTag(),
				comEvent.getSize(),
				comEvent.GetEventclock()));
			m_contents.emplace_back(std::move(entry));
			if (GetExtraDebug()) dprintf(D_FULLDEBUG, "Incrementing stored space by %zu to %zu\n",
				comEvent.getSize(), (size_t)m_stored_space + comEvent.getSize());
			m_stored_space += comEvent.getSize();

			auto util_iter = m_space_utilization.insert({iter->second->getTag(), SpaceUtilization()});
			util_iter.first->second.incWritten(comEvent.getSize());
		}
	}
		break;
	case ULOG_FILE_USED: {
		auto usedEvent = static_cast<FileUsedEvent&>(event);
		auto iter = std::find_if(m_contents.begin(),
			m_contents.end(),
			[&](const std::unique_ptr<FileEntry> &entry) -> bool {
				return entry->checksum_type() == usedEvent.getChecksumType() &&
					entry->checksum() == usedEvent.getChecksum() &&
					entry->tag() == usedEvent.getTag();
			});
		if (iter != m_contents.end()) {
			if (GetExtraDebug()) dprintf(D_FULLDEBUG, "Updated last use for file with checksum %s(%s) to %lu\n",
				usedEvent.getChecksum().c_str(), usedEvent.getChecksumType().c_str(), usedEvent.GetEventclock());
			(*iter)->update_last_use(event.GetEventclock());

			auto util_iter = m_space_utilization.insert({(*iter)->tag(), SpaceUtilization()});
			util_iter.first->second.incUsed((*iter)->size());

			return true;
		}
		dprintf(D_ALWAYS, "File with checksum %s used - but file is unknown to our state.\n",
			usedEvent.getChecksum().c_str());
		err.pushf("DataReuse", 14, "File with checksum %s used - but file is unknown to our state.",
			usedEvent.getChecksum().c_str());
		return false;
	}
		break;
	case ULOG_FILE_REMOVED: {
		auto remEvent = static_cast<FileRemovedEvent&>(event);
		auto iter = std::find_if(m_contents.begin(),
			m_contents.end(),
			[&](const std::unique_ptr<FileEntry> &entry) -> bool {
				return entry->checksum_type() == remEvent.getChecksumType() &&
					entry->checksum() == remEvent.getChecksum() &&
					entry->tag() == remEvent.getTag();
			});
		if (iter == m_contents.end()) {
			dprintf(D_ERROR, "File with checksum %s removed - but file is unknown to our state.\n",
				remEvent.getChecksum().c_str());
			err.pushf("DataReuse", 15, "File with checksum %s removed - but file is unknown to our state",
				remEvent.getChecksum().c_str());
			return false;
		}
		m_contents.erase(iter);
		m_stored_space -= remEvent.getSize();

		auto util_iter = m_space_utilization.insert({remEvent.getTag(), SpaceUtilization()});
		util_iter.first->second.incDeleted(remEvent.getSize());
	}
		break;
	default:
		dprintf(D_ALWAYS, "Unknown event in data reuse log.\n");
		err.pushf("DataReuse", 16, "Unknown event in data reuse log");
		return false;
	}
	return true;
}


bool
DataReuseDirectory::CacheFile(const std::string &source, const std::string &checksum,
	const std::string &checksum_type, const std::string &uuid,
	CondorError &err)
{
	if (!IsChecksumTypeSupported(checksum_type)) {
		err.pushf("DataReuse", 17, "Checksum type %s is not supported.",
			checksum_type.c_str());
		return false;
	}

	auto md = EVP_get_digestbyname(checksum_type.c_str());
	if (!md) {
		err.pushf("DataReuse", 9, "Failed to find impelmentation of checksum type %s.",
			checksum_type.c_str());
		return false;
	}

	int source_fd = -1;
	{
		TemporaryPrivSentry sentry(PRIV_USER);
		source_fd = safe_open_wrapper(source.c_str(), O_RDONLY);
	}
	if (source_fd == -1) {
		err.pushf("DataReuse", errno, "Unable to open cache file source (%s): %s",
			source.c_str(), strerror(errno));
		return false;
	}

	struct stat stat_buf;
	if (-1 == fstat(source_fd, &stat_buf)) {
		err.pushf("DataReuse", errno, "Unable to determine source file size (%s): %s",
			source.c_str(), strerror(errno));
		close(source_fd);
		return false;
	}

		//
		// Ok, source file looks good - let's try caching it.  First, we'll need to
		// grab the lock on the state.
		//
	LogSentry sentry = LockLog(err);
	if (!sentry.acquired()) {
		close(source_fd);
		return false;
	}

	if (!UpdateState(sentry, err)) {
		close(source_fd);
		return false;
	}

	auto iter = m_space_reservations.find(uuid);
	if (iter == m_space_reservations.end()) {
		err.pushf("DataReuse", 1, "Unknown space reservation requested: %s\n", uuid.c_str());
		close(source_fd);
		return false;
	}

	if (iter->second->getReservedSpace() < static_cast<size_t>(stat_buf.st_size)) {
		err.pushf("DataReuse", 2, "Insufficient space in reservation to save file.\n");
		close(source_fd);
		return false;
	}

	std::unique_ptr<FileEntry> new_entry(new FileEntry(*this, checksum, checksum_type, iter->second->getTag(), stat_buf.st_size, time(NULL)));
	std::string dest_fname = new_entry->fname();
	// create a filename pattern for condor_mkstmp consisting of fname + "." + "XXXXXX"
	// mkstmp will overwrite the 6 X's to create a unique filename.
	std::vector<char> dest_tmp_fname(dest_fname.size() + 1 + 6 + 1, 'X');
	strcpy(&dest_tmp_fname[0], dest_fname.c_str());
	dest_tmp_fname[dest_fname.size()] = '.';
	dest_tmp_fname[dest_fname.size() + 1 + 6] = 0;
	int dest_fd = -1;
	TemporaryPrivSentry priv_sentry(PRIV_CONDOR);
	dest_fd = condor_mkstemp(&dest_tmp_fname[0]);

	if (dest_fd == -1) {
		err.pushf("DataReuse", errno, "Unable to open cache file destination (%s): %s",
			dest_fname.c_str(), strerror(errno));
		close(source_fd);
		return false;
	}

	auto mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, NULL);

	const int membufsiz = 64 * 1024;
	std::unique_ptr<void, decltype(&free)> memory_buffer(malloc(membufsiz), free);
	ssize_t bytes;
	while (true) {
		bytes = full_read(source_fd, memory_buffer.get(), membufsiz);
		if (bytes <= 0) {
			break;
		}
		auto write_bytes = full_write(dest_fd, memory_buffer.get(), bytes);
		if (write_bytes != bytes) {
			bytes = -1;
			break;
		}
		int result = EVP_DigestUpdate(mdctx, memory_buffer.get(), write_bytes);
		if (result != 1) {
			err.pushf("DataReuse", errno, "Failure when updating hash");
			close(dest_fd);
			unlink(&dest_tmp_fname[0]);
			close(source_fd);
			EVP_MD_CTX_destroy(mdctx);
			return false;
		}
	}
	if (bytes < 0) {
		err.pushf("DataReuse", errno, "Failure when copying the file to cache directory: %s",
			strerror(errno));
		close(dest_fd);
		unlink(&dest_tmp_fname[0]);
		close(source_fd);
		EVP_MD_CTX_destroy(mdctx);
		return false;
	}
	close(dest_fd);
	close(source_fd);

	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_destroy(mdctx);
	std::vector<char> computed_checksum(2*md_len + 1, 0);
	for (unsigned int idx = 0; idx < md_len; idx++) {
		snprintf(&computed_checksum[2*idx], 3, "%02x", md_value[idx]);
	}
	if (strcmp(&computed_checksum[0], checksum.c_str())) {
		err.pushf("DataReuse", 11, "Source file checksum does not match expected one.");
		unlink(&dest_tmp_fname[0]);
		return false;
	}
	auto retval = rename(&dest_tmp_fname[0], dest_fname.c_str());
	if (-1 == retval) {
		err.pushf("DataReuse", errno, "Failed to rename temp reuse file %s to final filename %s: %s.", dest_tmp_fname.data(), dest_fname.c_str(), strerror(errno));
		unlink(&dest_tmp_fname[0]);
		return false;
	}

	FileCompleteEvent event;
	event.setUUID(uuid);
	event.setSize(stat_buf.st_size);
	event.setChecksumType(checksum_type);
	event.setChecksum(checksum);
	if (!m_log.writeEvent(&event)) {
		err.pushf("DataReuse", 3, "Failed to write out file complete event.");
		unlink(dest_fname.c_str());
		return false;
	}

	return true;
}


bool
DataReuseDirectory::Renew(uint32_t lifetime, const std::string &tag, const std::string &uuid,
	CondorError &err)
{
	LogSentry sentry = LockLog(err);
	if (!sentry.acquired()) {
		return false;
	}

	if (!UpdateState(sentry, err)) {
		return false;
	}

	auto iter = m_space_reservations.find(uuid);
	if (iter == m_space_reservations.end()) {
		err.pushf("DataReuse", 4, "Failed to find space reservation (%s) to renew.",
			uuid.c_str());
		return false;
	}
	if (iter->second->getTag() != tag) {
		err.pushf("DataReuse", 5, "Existing reservation's tag (%s) does not match requested one (%s).",
			iter->second->getTag().c_str(), tag.c_str());
		return false;
	}

	ReserveSpaceEvent event;
	auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(lifetime);
	event.setExpirationTime(expiry);
	iter->second->setExpirationTime(expiry);

	if (!m_log.writeEvent(&event)) {
		err.pushf("DataReuse", 6, "Failed to write out space reservation renewal.");
		return false;
	}

	return true;
}


bool
DataReuseDirectory::ReleaseSpace(const std::string &uuid, CondorError &err)
{
	LogSentry sentry = LockLog(err);
	if (!sentry.acquired()) {
		return false;
	}

	if (!UpdateState(sentry, err)) {
		return false;
	}

	auto iter = m_space_reservations.find(uuid);
	if (iter == m_space_reservations.end()) {
		err.pushf("DataReuse", 7, "Failed to find space reservation (%s) to release; "
			"there are %zu active reservations.",
			uuid.c_str(), m_space_reservations.size());
		return false;
	}

	ReleaseSpaceEvent event;
	event.setUUID(uuid);
	m_space_reservations.erase(iter);
	if (GetExtraDebug()) dprintf(D_FULLDEBUG, "Releasing space reservation %s\n", uuid.c_str());
	if (!m_log.writeEvent(&event)) {
		err.pushf("DataReuse", 10, "Failed to write out space reservation release.");
		return false;
	}

	return true;
}


bool
DataReuseDirectory::RetrieveFile(const std::string &destination, const std::string &checksum,
	const std::string &checksum_type, const std::string &tag, CondorError &err)
{
	if (!IsChecksumTypeSupported(checksum_type)) {
		err.pushf("DataReuse", 17, "Checksum type %s is not supported.",
			checksum_type.c_str());
		return false;
	}


	LogSentry sentry = LockLog(err);
	if (!sentry.acquired()) {
		return false;
	}

	if (!UpdateState(sentry, err)) {
		return false;
	}

	auto iter = std::find_if(m_contents.begin(),
		m_contents.end(),
		[&](const std::unique_ptr<FileEntry> &entry) -> bool {
			return entry->checksum_type() == checksum_type &&
				entry->checksum() == checksum &&
				entry->tag() == tag;
		});
	if (iter == m_contents.end()) {
		err.pushf("DataReuse", 8, "Failed to find requested file (checksum=%s, checksum_type=%s, tag=%s) in state database.", checksum.c_str(), checksum_type.c_str(), tag.c_str());
		return false;
	}

	std::string source = (*iter)->fname();

	int source_fd = -1;
	{
		TemporaryPrivSentry sentry(PRIV_CONDOR);
		source_fd = safe_open_wrapper(source.c_str(), O_RDONLY);
	}
	if (source_fd == -1) {
		err.pushf("DataReuse", errno, "Unable to open cache file source (%s): %s",
			source.c_str(), strerror(errno));
		return false;
	}

	int dest_fd = -1;
	{
		TemporaryPrivSentry sentry(PRIV_USER);
		dest_fd = safe_open_wrapper(destination.c_str(), O_EXCL | O_CREAT | O_WRONLY);
	}
	if (dest_fd == -1) {
		err.pushf("DataReuse", errno, "Unable to open cache file destination (%s): %s",
			destination.c_str(), strerror(errno));
		close(source_fd);
		return false;
	}

	auto md = EVP_get_digestbyname(checksum_type.c_str());
	if (!md) {
		err.pushf("DataReuse", 9, "Failed to find impelmentation of checksum type %s.",
			checksum_type.c_str());
		close(source_fd);
		close(dest_fd);
		return false;
	}
	auto mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, NULL);

	const int membufsiz = 64 * 1024;
	std::unique_ptr<void, decltype(&free)> memory_buffer(malloc(membufsiz), free);
	ssize_t bytes;
	while (true) {
		bytes = full_read(source_fd, memory_buffer.get(), membufsiz);
		if (bytes <= 0) {
			break;
		}
		auto write_bytes = full_write(dest_fd, memory_buffer.get(), bytes);
		if (write_bytes != bytes) {
			bytes = -1;
			break;
		}
		int result = EVP_DigestUpdate(mdctx, memory_buffer.get(), write_bytes);
		if (result != 1) {
			err.pushf("DataReuse", errno, "Failure when updating hash");
			close(dest_fd);
			close(source_fd);
			EVP_MD_CTX_destroy(mdctx);
			return false;
		}
	}
	if (bytes < 0) {
		err.pushf("DataReuse", errno, "Failure when copying the file to destination: %s",
			strerror(errno));
		close(dest_fd);
		close(source_fd);
		EVP_MD_CTX_destroy(mdctx);
		return false;
	}
	close(dest_fd);
	close(source_fd);

	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_destroy(mdctx);
	std::vector<char> computed_checksum(2*md_len + 1, 0);
	for (unsigned int idx = 0; idx < md_len; idx++) {
		snprintf(&computed_checksum[2*idx], 3, "%02x", md_value[idx]);
	}
	if (strcmp(&computed_checksum[0], checksum.c_str())) {
		err.pushf("DataReuse", 10, "Source file checksum does not match expected one.");
		// TODO: remove file.
		return false;
	}

	FileUsedEvent event;
	event.setChecksumType(checksum_type);
	event.setChecksum(checksum);
	event.setTag(tag);
	if (!m_log.writeEvent(&event)) {
		err.pushf("DataReuse", 8, "Failed to write out file use event.");
		return false;
	}
	return true;
}


void
DataReuseDirectory::PrintInfo(bool print_to_log)
{
	{
		CondorError err;
		LogSentry sentry = LockLog(err);
		if (!UpdateState(sentry, err)) {
			dprintf(D_ALWAYS, "Failed to print data reuse directory info because"
				"state update failed: %s\n", err.getFullText().c_str());
			return;
		}
	}
	std::stringstream ss;
	ss << "Data Reuse Directory status information:\n"
		"\t- Filesystem path: " << m_dirpath << "\n"
		"\t- Directory state is considered " << (m_valid ? "valid" : "INVALID") << "\n"
		"\t- State file location: " << m_state_name << "\n"
		"\t- Space allocated to the directory: " <<
			metric_units(static_cast<double>(m_allocated_space)) << "\n";
	ss << "\t- Space in transfer reservations: " <<
			metric_units(static_cast<double>(m_reserved_space)) << "\n";
	ss << "\t- Space use by committed files: " <<
			metric_units(static_cast<double>(m_stored_space));
	if (print_to_log)
		dprintf(D_ALWAYS, "%s\n", ss.str().c_str());
	else
		printf("%s\n", ss.str().c_str());

	ss.str("");
	ss.clear();

	if (!m_stored_space && !m_reserved_space) {return;}
	if (print_to_log && !IsDebugVerbose(D_ALWAYS)) {
		return;
	}

	std::map<std::string, std::pair<uint64_t, uint32_t>> per_user_reservations;
	for (const auto &entry : m_space_reservations) {
		auto result = per_user_reservations.insert({entry.second->getTag(), {0,0}});
		auto &result_pair = result.first->second;
		result_pair.first += entry.second->getReservedSpace();
		result_pair.second ++;
	}
	if (per_user_reservations.size()) {
		ss << "Space reservations per user:\n";
		for (const auto &entry : per_user_reservations) {
			ss << "\t- User " << entry.first << ": "
			"Space reserved - " << metric_units(static_cast<double>(entry.second.first)) << ", "
			"Reservation count - " << entry.second.second << "\n";
		}
	}

	std::map<std::string, std::pair<uint64_t, uint32_t>> per_user_space;
	for (const auto &entry : m_contents) {
		auto result = per_user_space.insert({entry->tag(), {0,0}});
		auto &result_pair = result.first->second;
		result_pair.first += entry->size();
		result_pair.second ++;
	}
	if (per_user_space.size()) {
		ss << "Space utilization per user:\n";
		for (const auto &entry : per_user_space) {
			ss << "\t- User " << entry.first << ": "
				"Space used - " << metric_units(static_cast<double>(entry.second.first)) << ", "
				"File count - " << entry.second.second << "\n";
		}
	}

	if (print_to_log)
		dprintf(D_ALWAYS, "%s\n", ss.str().c_str());
	else
		printf("%s\n", ss.str().c_str());

	ss.str("");
	ss.clear();

	if (!GetExtraDebug()) {return;}

	ss << "Active space reservations:\n";
	auto now = std::chrono::system_clock::now();
	for (const auto &entry : m_space_reservations) {
		ss << "\t- UUID " << entry.first << " for " <<
			entry.second->getTag() << ": " <<
			metric_units(static_cast<double>(entry.second->getReservedSpace())) <<
			", " << std::chrono::duration_cast<std::chrono::seconds>(entry.second->getExpirationTime() - now).count() <<
			" seconds remain.\n";
	}
	if (!m_space_reservations.size()) {
		ss << "\t(None!)\n";
	}

	ss << "\nStored files:\n";
	auto now_t = time(NULL);
	for (const auto &entry : m_contents) {
		ss << "\t- File with\n\t\t- Checksum " << entry->checksum() << "(" <<
			entry->checksum_type() << ")\n\t\t- Owner: " <<
			entry->tag() << "\n\t\t- Last use: " <<
			(now_t - entry->last_use()) << " seconds ago (now: " << now_t << ")" << "\n\t\t- File size: " <<
			metric_units(static_cast<double>(entry->size())) << "\n";
	}
	if (!m_contents.size()) {
		ss << "\t(None!)\n";
	}

	if (print_to_log)
		dprintf(D_FULLDEBUG, "%s\n", ss.str().c_str());
	else
		printf("%s\n", ss.str().c_str());
}


bool
DataReuseDirectory::Publish(classad::ClassAd &ad)
{
	{
		CondorError err;
		LogSentry sentry = LockLog(err);
		if (!UpdateState(sentry, err)) {
			dprintf(D_ALWAYS, "DataReuseDirectory::Publish failed to Update State\n");
		}
	}

	auto retval = true;
	retval &= ad.InsertAttr("HasDataReuse", m_valid);
	retval &= ad.InsertAttr("DataReuseAllocatedMB",
		static_cast<double>(m_allocated_space)/1'000'000.0);
	retval &= ad.InsertAttr("DataReuseReservedMB",
		static_cast<double>(m_reserved_space)/1'000'000.0);
	retval &= ad.InsertAttr("DataReuseUsedMB",
		static_cast<double>(m_stored_space)/1'000'000.0);

	std::unordered_map<std::string, SpaceUtilization> per_user_lifetime;
	SpaceUtilization global_util;
	for (const auto &entry : m_space_utilization) {
		auto iter = per_user_lifetime.insert({entry.first, SpaceUtilization()});
		iter.first->second.incUsed(entry.second.used());
		iter.first->second.incWritten(entry.second.written());
		iter.first->second.incDeleted(entry.second.deleted());
		global_util.incUsed(entry.second.used());
		global_util.incWritten(entry.second.written());
		global_util.incDeleted(entry.second.deleted());
	}
	retval &= ad.InsertAttr("DataReuseAggregateWrittenMB",
		static_cast<double>(global_util.written())/1'000'000.0);
	retval &= ad.InsertAttr("DataReuseAggregateReadMB",
		static_cast<double>(global_util.used())/1'000'000.0);
	retval &= ad.InsertAttr("DataReuseAggregateDeletedMB",
		static_cast<double>(global_util.deleted())/1'000'000.0);
	for (const auto &entry : per_user_lifetime) {
		retval &= ad.InsertAttr("DataReuse_" + entry.first + "_AggregateWrittenMB",
			static_cast<double>(entry.second.written())/1'000'000.0);
		retval &= ad.InsertAttr("DataReuse_" + entry.first + "_AggregateReadMB",
			static_cast<double>(entry.second.used()/1'000'000.0));
		retval &= ad.InsertAttr("DataReuse_" + entry.first + "_AggregateDeletedMB",
			static_cast<double>(entry.second.deleted()/1'000'000.0));
	}

		// No reason to print out per-user info for an invalid directory.
	if (!m_valid) {return retval;}

	std::map<std::string, std::pair<uint64_t, uint32_t>> per_user_reservations;
	for (const auto &entry : m_space_reservations) {
			// For now, we simplify to just the username instead of the
			// user@domain; the mapping is lossy but results in much simpler
			// attribute names below.
		const auto &tag = entry.second->getTag();
		auto user = tag.substr(0, tag.find('@'));
		auto result = per_user_reservations.insert({user, {0,0}});
		auto &result_pair = result.first->second;
		result_pair.first += entry.second->getReservedSpace();
		result_pair.second ++;
	}
	for (const auto &entry : per_user_reservations) {
		retval &= ad.InsertAttr("DataReuse_" + entry.first + "_SpaceReservedMB",
			static_cast<double>(entry.second.first)/1'000'000.0);
		retval &= ad.InsertAttr("DataReuse_" + entry.first + "_ReservationCount",
			static_cast<int32_t>(entry.second.second));
	}

	std::map<std::string, std::pair<uint64_t, uint32_t>> per_user_space;
	for (const auto &entry : m_contents) {
		const auto &tag = entry->tag();
		auto user = tag.substr(0, tag.find('@'));
		auto result = per_user_space.insert({user, {0,0}});
		auto &result_pair = result.first->second;
		result_pair.first += entry->size();
		result_pair.second ++;
	}
	for (const auto &entry : per_user_space) {
		retval &= ad.InsertAttr("DataReuse_" + entry.first + "_SpaceUsedMB",
			static_cast<double>(entry.second.first)/1'000'000.0);
		retval &= ad.InsertAttr("DataReuse_" + entry.first + "_FileCount",
			static_cast<int32_t>(entry.second.second));
	}

	return retval;
}


bool
DataReuseDirectory::GetExtraDebug()
{
	return param_boolean("DATA_REUSE_EXTRA_DEBUG", false);
}
