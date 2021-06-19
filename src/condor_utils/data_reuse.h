/***************************************************************
 *
 * Copyright (C) 2019, Condor Team, Computer Sciences Department,
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

#ifndef __DATA_REUSE_H_
#define __DATA_REUSE_H_

#include "read_user_log.h"
#include "write_user_log.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

class CondorError;

namespace htcondor {

class DataReuseDirectory {
public:
	DataReuseDirectory(const std::string &dirpath, bool owner);

	~DataReuseDirectory();

	DataReuseDirectory(const DataReuseDirectory&) = delete;
	DataReuseDirectory& operator=(const DataReuseDirectory*) = delete;

	const std::string &GetDirectory() const {return m_dirpath;}

	bool ReserveSpace(uint64_t size, uint32_t lifetime, const std::string &tag,
		std::string &id, CondorError &err);

	bool Renew(uint32_t lifetime, const std::string &tag, const std::string &uuid,
		CondorError &err);

	bool ReleaseSpace(const std::string &uuid, CondorError &err);

	bool CacheFile(const std::string &source, const std::string &checksum,
		const std::string &checksum_type, const std::string &uuid,
		CondorError &err);

	bool RetrieveFile(const std::string &destination, const std::string &checksum,
		const std::string &checksum_type, const std::string &tag,
		CondorError &err);

		// Publish various data reuse statistics to the ad.
	bool Publish(classad::ClassAd &ad);

	static bool IsChecksumTypeSupported(const std::string &type) {return type == "sha256";}

	// Print known info about the state of the directory:
	// - print_to_log: Set to True to print data via dprintf; otherwise, will print to stdout.
	void PrintInfo(bool print_to_log);

private:
	class LogSentry {
	public:
		LogSentry(DataReuseDirectory &parent, CondorError &err);
		~LogSentry();
		LogSentry(const LogSentry&) = delete;
		LogSentry(LogSentry&&) = default;
		LogSentry& operator=(const LogSentry*) = delete;

		bool acquired() const {return m_acquired;}
		FileLockBase *lock() const {return m_lock;}
	private:
		bool m_acquired{false};
		DataReuseDirectory &m_parent;
		FileLockBase *m_lock{nullptr};
	};

	class FileEntry {
	public:
		FileEntry(DataReuseDirectory &parent, const std::string &checksum,
			const std::string &checksum_type, const std::string &tag,
			uint64_t size, time_t last_use)
		: m_size(size),
		m_last_use(last_use),
		m_checksum(checksum),
		m_checksum_type(checksum_type),
		m_tag(tag),
		m_parent(parent)
		{}

		uint64_t size() const {return m_size;}
		const std::string &checksum() const {return m_checksum;}
		const std::string &checksum_type() const {return m_checksum_type;}
		const std::string &tag() const {return m_tag;}
		static std::string fname(const std::string &dirpath, const std::string &checksum_type, const std::string &checksum, const std::string &tag);
		std::string fname() const;
		time_t last_use() const {return m_last_use;}
		void update_last_use(time_t current) {m_last_use = current > m_last_use ? current : m_last_use;}

	private:
		uint64_t m_size{0};
		time_t m_last_use{0};
		std::string m_checksum;
		std::string m_checksum_type;
		std::string m_tag;
		const DataReuseDirectory &m_parent;
	};

	class SpaceReservationInfo {
	public:
		SpaceReservationInfo(std::chrono::system_clock::time_point expiry_time,
			const std::string &tag, size_t reserved)
		: m_expiry_time(expiry_time),
		m_tag(tag),
		m_reserved(reserved)
		{}

		const std::string &getTag() const {return m_tag;}

		size_t getReservedSpace() const {return m_reserved;}
		void setReservedSpace(size_t space) {m_reserved = space;}

		std::chrono::system_clock::time_point getExpirationTime() const {return m_expiry_time;}
		void setExpirationTime(std::chrono::system_clock::time_point updated) {m_expiry_time = updated;}

	private:
		std::chrono::system_clock::time_point m_expiry_time;
		const std::string m_tag;
		size_t m_reserved{0};
	};

	class SpaceUtilization {
	public:
		uint64_t used() const {return m_used;}
		uint64_t written() const {return m_written;}
		uint64_t deleted() const {return m_deleted;}

		void incUsed(uint64_t used) {m_used += used;}
		void incWritten(uint64_t written) {m_written += written;}
		void incDeleted(uint64_t deleted) {m_deleted += deleted;}
	private:
		uint64_t m_used{0};
		uint64_t m_written{0};
		uint64_t m_deleted{0};
	};

	bool ClearSpace(uint64_t size, LogSentry &sentry, CondorError &err);
	bool UpdateState(LogSentry &sentry, CondorError &err);
	bool HandleEvent(ULogEvent &event, CondorError &err);

	LogSentry LockLog(CondorError &err);
	bool UnlockLog(LogSentry sentry, CondorError &err);

	void CreatePaths();
	void Cleanup();

		// Returns true if we should have an extra-high debug level.
	static bool GetExtraDebug();

	bool m_owner{true};
	bool m_valid{false};

	uint64_t m_reserved_space{0};
	uint64_t m_stored_space{0};
	uint64_t m_allocated_space{0};

	std::string m_dirpath;
	std::string m_logname;

	std::string m_state_name; // Pathname of the state file.
	WriteUserLog m_log;
	ReadUserLog m_rlog;

	std::unordered_map<std::string, std::unique_ptr<SpaceReservationInfo>> m_space_reservations;
	std::vector<std::unique_ptr<FileEntry>> m_contents;

		// Track the space read, written and deleted per-tag.
	std::unordered_map<std::string, SpaceUtilization> m_space_utilization;
};

}

#endif  // __DATA_REUSE_H_
