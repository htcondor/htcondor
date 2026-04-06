#ifndef   _CONDOR_STAGING_DIRECTORY_H
#define   _CONDOR_STAGING_DIRECTORY_H


int createStagingDirectory(
	const std::filesystem::path & parentDir,
	const std::filesystem::path & stagingDir
);
bool convertToStagingDirectory( const std::filesystem::path & stagingDir );
bool mapContentsOfDirectoryInto(
	const std::filesystem::path & stagingDir,
	const std::filesystem::path & sandbox
);


#include <memory>
#include <filesystem>

class StagingDirectory {

	public:

		virtual bool create() = 0;
		virtual bool modify() = 0;
		virtual bool map( const std::filesystem::path & d ) = 0;
		virtual std::filesystem::path path() const = 0;

		virtual ~StagingDirectory() = default;

	protected:

		StagingDirectory(
			const std::filesystem::path & d,
			const std::string & c
		) : parentDir(d), catalogName(c) { }

		std::filesystem::path parentDir;
		std::string catalogName;
};


class StagingDirectoryFactory {

	public:

		StagingDirectoryFactory();

		std::unique_ptr<StagingDirectory> make(
		    const std::filesystem::path & directory,
		    const std::string & catalogName
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
