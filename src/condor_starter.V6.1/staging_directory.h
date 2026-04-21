#ifndef   _CONDOR_STAGING_DIRECTORY_H
#define   _CONDOR_STAGING_DIRECTORY_H


#include <memory>
#include <filesystem>

class StagingDirectory {

	public:

		static bool usable() { return false; }
		virtual bool create() = 0;
		virtual bool modify() = 0;
		virtual bool map( const std::filesystem::path & d ) = 0;
		virtual std::filesystem::path path() { return stagingDir; }

		virtual ~StagingDirectory() = default;

	protected:

		StagingDirectory(
			const std::filesystem::path & d,
			const std::string & c
		) : parentDir(d), catalogName(c) { stagingDir = d / c; }

		StagingDirectory(
			const std::string & s
		) : stagingDir(s) {
		    parentDir = stagingDir.parent_path();
		    catalogName = stagingDir.filename().string();
		}

		std::filesystem::path parentDir;
		std::string catalogName;
		std::filesystem::path stagingDir;
};


class StagingDirectoryFactory {

	public:

		StagingDirectoryFactory();

		std::unique_ptr<StagingDirectory> make(
		    const std::filesystem::path & directory,
		    const std::string & catalogName
		);

		std::unique_ptr<StagingDirectory> make(
		    const std::string & stagingDirectory
		);

	private:

		enum class StagingDirectoryType {
			MIN,
			INVALID,
			Hardlink,
			BindMount,
			Copy,
			MAX
		};

		StagingDirectoryType typeToUse = StagingDirectoryType::INVALID;
};


#endif /* _CONDOR_STAGING_DIRECTORY_H */
