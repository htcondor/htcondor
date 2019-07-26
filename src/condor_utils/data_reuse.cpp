
#include "condor_debug.h"
#include "data_reuse.h"

#include "CondorError.h"
#include "file_lock.h"
#include "directory.h"
#include "directory_util.h"

#include <stdio.h>

#include <algorithm>

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
		ULogEvent *event = nullptr;
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

	auto now = std::chrono::system_clock::now();
	auto iter = m_space_reservations.begin();
	while (iter != m_space_reservations.end()) {
		if (iter->second->getExpirationTime() < now) {
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
DataReuseDirectory::HandleEvent(ULogEvent &event, CondorError & /*err*/)
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
		} else if (iter->second->getTag() != resEvent.getTag()) {
			dprintf(D_FAILURE, "Duplicate space reservation with incorrect tag (%s)\n",
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
			return false;
		}
		m_reserved_space -= iter->second->getReservedSpace();
		m_space_reservations.erase(iter);
		return true;
	}
		break;
	case ULOG_FILE_COMPLETE: {
		auto comEvent = static_cast<FileCompleteEvent&>(event);
		auto iter = m_space_reservations.find(comEvent.getUUID());
		if (iter == m_space_reservations.end()) {
			dprintf(D_FAILURE, "File completed for non-existent space reservation %s.\n",
				comEvent.getUUID().c_str());
			return false;
		}
		if (iter->second->getReservedSpace() < comEvent.getSize()) {
			dprintf(D_FAILURE, "File completed with size %lu, which is larger than the space"
				" reservation size.\n", comEvent.getSize());
			return false;
		}
		auto event_time = std::chrono::system_clock::from_time_t(event.GetEventclock());
		if (iter->second->getExpirationTime() > event_time) {
			dprintf(D_FAILURE, "File completed after space reservation ended.\n");
			return false;
		}
		iter->second->setReservedSpace(iter->second->getReservedSpace() - comEvent.getSize());

		std::unique_ptr<FileEntry> entry(new FileEntry(*this,
			comEvent.getChecksum(),
			comEvent.getChecksumType(),
			iter->second->getTag(),
			comEvent.getSize()));
		m_contents.emplace_back(std::move(entry));
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
			(*iter)->update_last_use(event.GetEventclock());
			return true;
		}
		dprintf(D_ALWAYS, "File with checksum %s used - but file is unknown to our state.\n",
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
			dprintf(D_FAILURE, "File with checksum %s removed - but file is unknown to our state.\n",
				remEvent.getChecksum().c_str());
			return false;
		}
		m_contents.erase(iter);
		m_stored_space += remEvent.getSize();
	}
		break;
	default:
		dprintf(D_ALWAYS, "Unknown event in data reuse log.\n");
		return false;
	}
	dprintf(D_FAILURE, "Logic error - unhandled data reuse directory event.\n");
	return false;
}
