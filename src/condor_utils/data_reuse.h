
#include <string>

namespace htcondor {

class DataReuseDirectory {
public:
	DataReuseDirectory(const std::string &dirpath, bool owner);

	~DataReuseDirectory();

	DataReuseDirectory(const DataReuseDirectory&) = delete;
	DataReuseDirectory& operator=(const DataReuseDirectory*) = delete;

	const std::string &getDirectory() const {return m_dirpath;}

private:
	void CreatePaths();
	void Cleanup();

	bool m_owner{true};
	bool m_valid{false};
	std::string m_dirpath;
};

}
