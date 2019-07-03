
#include "condor_debug.h"
#include "data_reuse.h"

#include "CondorError.h"
#include "file_lock.h"
#include "directory.h"
#include "directory_util.h"

#include <stdio.h>

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
	m_log(dircat(m_dirpath.c_str(), "use.log", m_logname)),
	m_rlog(dircat(m_dirpath.c_str(), "use.log", m_logname))
{
	if (m_owner) {
		Cleanup();
		CreatePaths();
	}
}


void
DataReuseDirectory::CreatePaths()
{
	if (!mkdir_and_parents_if_needed(m_dirpath.c_str(), 0700, 0700, PRIV_CONDOR)) {
		m_valid = false;
		return;
	}
	MyString subdir, subdir2;
	auto name = dircat(m_dirpath.c_str(), "tmp", subdir);
	if (mkdir_and_parents_if_needed(name, 0700, 0700, PRIV_CONDOR)) {
		m_valid = false;
		return;
	}
	name = dircat(m_dirpath.c_str(), "sha256", subdir);
	char subdir_name[3];
	subdir_name[2] = '\0';
	for (int idx = 0; idx < 256; idx++) {
		sprintf(subdir_name, "%02X", idx);
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


DataReuseDirectory::LogSentry::LogSentry(DataReuseDirectory &parent)
	: m_parent(parent)
{
	m_lock = m_parent.m_log.getLock();
	if (m_lock == nullptr) {return;}

	m_acquired = m_lock->obtain(WRITE_LOCK);
}


DataReuseDirectory::LogSentry::~LogSentry()
{
	if (m_acquired) {
		m_lock->release();
	}
}


DataReuseDirectory::LogSentry &&
DataReuseDirectory::LockLog(CondorError &err)
{
	LogSentry sentry(*this);
	if (!sentry.acquired()) {
		err.push("DataReuse", 3, "Failed to acquire data reuse directory lockfile.");
	}
	return std::move(sentry);
}


std::string
DataReuseDirectory::FileEntry::fname() const
{
	MyString hash_dir;
	dircat(m_parent.m_dirpath.c_str(), m_checksum_type.c_str(), hash_dir);

	char hash_substring[3];
	hash_substring[2] = '\0';
	hash_substring[0] = m_checksum[0];
	hash_substring[1] = m_checksum[1];

	MyString file_dir;
	dircat(hash_dir.c_str(), hash_substring, file_dir);

	MyString fname;
	std::string hash_name(m_checksum.c_str() + 2, m_checksum.size()-2);
	hash_name += "." + m_tag;
	dircat(file_dir.c_str(), hash_name.c_str(), fname);

	return std::string(fname.c_str());
}


bool
DataReuseDirectory::ReserveSpace(uint64_t size, uint32_t time, std::string &tag,
	std::string &id, CondorError &err)
{
	LogSentry sentry = LockLog(err);
	if (!sentry.acquired()) {
		return false;
	}

	UpdateState(sentry, err);

	if ((m_reserved_space + size > m_allocated_space) && !ClearSpace(size, sentry, err))
	{
		err.push("DataReuse", 1, "Unable to allocate space");
		return false;
	}

	ReserveSpaceEvent event;
	auto now = std::chrono::system_clock::now();
	event.setExpirationTime(now + std::chrono::seconds(time));
	event.setReservedSpace(size);
	event.setTag(tag);
	std::string uuid_result = event.generateUUID();
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

	bool all_done = false;
	do {
		ULogEvent *event;
		auto outcome = m_rlog.readEventWithLock(event, *sentry.lock());

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

	std::sort(m_contents.begin(), m_contents.end(), [](std::unique_ptr<FileEntry> &left, std::unique_ptr<FileEntry> &right) {
		return left->last_use() < right->last_use();
	});
}

bool
DataReuseDirectory::HandleEvent(ULogEvent & /*event*/, CondorError & /*err*/)
{
	return false;
}
