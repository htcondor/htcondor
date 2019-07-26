
#include "read_user_log.h"
#include "write_user_log.h"
#include "MyString.h"

#include <memory>
#include <string>
#include <vector>

class CondorError;

namespace htcondor {

class DataReuseDirectory {
public:
	DataReuseDirectory(const std::string &dirpath, bool owner);

	~DataReuseDirectory();

	DataReuseDirectory(const DataReuseDirectory&) = delete;
	DataReuseDirectory& operator=(const DataReuseDirectory*) = delete;

	const std::string &GetDirectory() const {return m_dirpath;}

	bool ReserveSpace(uint64_t size, uint32_t time, std::string &tag,
		std::string &id, CondorError &err);

	bool Renew(uint32_t time, std::string &tag, const std::string &uuid,
		CondorError &err);

	bool ReleaseSpace(const std::string &uuid, CondorError &err);

	bool CacheFile(const std::string &source, const std::string &checksum,
		const std::string &checksum_type, const std::string &uuid,
		CondorError &err);

	void UsedFile(const std::string &checksum, const std::string &checksum_type,
		const std::string &tag);

private:
	class LogSentry {
	public:
		LogSentry(DataReuseDirectory &parent);
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
			uint64_t size)
		: m_size(size),
		m_checksum(checksum),
		m_checksum_type(checksum_type),
		m_tag(tag),
		m_parent(parent)
		{}

		uint64_t size() const {return m_size;}
		const std::string &checksum() const {return m_checksum;}
		const std::string &checksum_type() const {return m_checksum_type;}
		const std::string &tag() const {return m_tag;}
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

	bool ClearSpace(uint64_t size, LogSentry &sentry, CondorError &err);
	bool UpdateState(LogSentry &sentry, CondorError &err);
	bool HandleEvent(ULogEvent &event, CondorError &err);

	LogSentry &&LockLog(CondorError &err);
	bool UnlockLog(LogSentry sentry, CondorError &err);

	void CreatePaths();
	void Cleanup();

	bool m_owner{true};
	bool m_valid{false};

	uint64_t m_reserved_space{0};
	uint64_t m_allocated_space{0};

	std::string m_dirpath;
	MyString m_logname;

	WriteUserLog m_log;
	ReadUserLog m_rlog;

	std::vector<std::unique_ptr<FileEntry>> m_contents;
};

}
