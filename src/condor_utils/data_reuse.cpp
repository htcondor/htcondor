
#include "data_reuse.h"

#include "directory.h"

using namespace htcondor;


DataReuseDirectory::~DataReuseDirectory()
{
	if (m_owner) {
		Cleanup();
	}
}

DataReuseDirectory::DataReuseDirectory(const std::string &dirpath, bool owner) :
	m_owner(owner),
	m_dirpath(dirpath)
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
