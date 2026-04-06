#ifndef   _CONDOR_STAGING_DIRECTORY_H
#define   _CONDOR_STAGING_DIRECTORY_H


int createStagingDirectory(
    const std::filesystem::path & parentDir,
    const std::filesystem::path & stagingDir
);
bool convertToStagingDirectory( const std::filesystem::path & stagingDir );
bool mapContentsOfDirectoryInto( const std::filesystem::path & location, const std::filesystem::path & sandbox );


#include <memory>
#include <filesystem>

class StagingDirectory {

	public:

        virtual bool create() = 0;
		virtual bool modify() = 0;
		virtual bool map( const std::filesystem::path & s ) = 0;

		virtual ~StagingDirectory() = default;

	protected:

		StagingDirectory( const std::filesystem::path & d ) : stagingDir(d) { }

        std::filesystem::path stagingDir;
};


class StagingDirectoryFactory {

	public:

		StagingDirectoryFactory();

		std::unique_ptr<StagingDirectory> make(
		    const std::filesystem::path & dir
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
