#ifndef   _CONDOR_STAGING_DIRECTORY_H
#define   _CONDOR_STAGING_DIRECTORY_H


#include <memory>
#include <filesystem>

//
// The staging starter calls create(), transfer the common files, and then
// calls modify().  The mapping starter calls map().  StagingDirectory
// provides a default implementation for create() and for modify() just to
// remove lines code.
//
// The map() method ...
//
// The path() method is an accessor (note that it returns a copy).
//

class StagingDirectory {

	public:

		static bool usable() { return false; }

		virtual std::error_code create();
		virtual std::error_code modify();
		virtual std::error_code map( const std::filesystem::path & d ) = 0;

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

		//
		// map_directory uses the Visitor pattern: it walks the staging
		// directory `location`, checking permissions as it goes, and
		// and calls entry_is_directory() or entry_is_file() as appropriate.
		//
		virtual std::error_code map_impl(
			const std::filesystem::path & location,
			const std::filesystem::path & sandbox,
			const std::string & log_prefix
		);
		virtual std::error_code entry_is_directory(
			const std::filesystem::path & sandbox,
			const std::filesystem::path & relative_path,
			const std::string & log_prefix
		) = 0;
		virtual std::error_code entry_is_file(
			const std::filesystem::path & sandbox,
			const std::filesystem::path & relative_path,
			const std::string & log_prefix
		) = 0;


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
