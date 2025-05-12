#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "staging_directory.h"

#include "condor_uid.h"


class HardlinkStagingDirectory : public StagingDirectory {

	public:

		virtual bool create();
		virtual bool modify();
		virtual bool stage( const std::filesystem::path & s );

		virtual ~HardlinkStagingDirectory() = default;

	protected:

		HardlinkStagingDirectory( const std::filesystem::path & d ) : StagingDirectory(d) { }


	friend class StagingDirectoryFactory;
};


class CopyStagingDirectory : public StagingDirectory {

	public:

		virtual bool create();
		virtual bool modify();
		virtual bool stage( const std::filesystem::path & s );

		virtual ~CopyStagingDirectory() = default;

	protected:

		CopyStagingDirectory( const std::filesystem::path & d ) : StagingDirectory(d) { }


	friend class StagingDirectoryFactory;
};


// Which kind of staging directory we manufacture will, at some point, depend
// on which kind of job we're running, or other factors unrelated to the
// platform or HTCondor configuration; those factors should be added to the
// parameters here.
StagingDirectoryFactory::StagingDirectoryFactory() {
#if       defined(WINDOWS)
	this->typeToUse = StagingDirectoryType::Copy;
#else  /* defined(WINDOWS) */
	this->typeToUse = StagingDirectoryType::Hardlink;
#endif /* defined(WINDOWS) */
}


std::unique_ptr<StagingDirectory>
StagingDirectoryFactory::make( const std::filesystem::path & dir ) {
	switch (this->typeToUse) {
		case StagingDirectoryType::Copy: {
			auto * p = new CopyStagingDirectory(dir);
			return std::unique_ptr<CopyStagingDirectory>(p);
			} break;

		case StagingDirectoryType::Hardlink: {
			auto * p = new HardlinkStagingDirectory(dir);
			return std::unique_ptr<HardlinkStagingDirectory>(p);
			} break;

		default:
			dprintf( D_ERROR, "Invalid type-to-use in StagingDirectoryFactory::make().\n" );
			return nullptr;
			break;
	}
}


bool
HardlinkStagingDirectory::create() {
	using std::filesystem::perms;

	// We could check STARTER_NESTED_SCRATCH to see if we needed
	// to create this directory as PRIV_CONDOR instead of PRIV_USER,
	// but since we'd need to escalate to root to chown afterwards
	// anyway, let's not duplicate code for now.
	TemporaryPrivSentry tps(PRIV_ROOT);

	std::error_code errorCode;
	std::filesystem::create_directory( stagingDir, errorCode );
	if( errorCode ) {
		dprintf( D_ALWAYS, "Unable to create staging directory, aborting: %s (%d)\n", errorCode.message().c_str(), errorCode.value() );
		return false;
	}

	std::filesystem::permissions(
		stagingDir,
		perms::owner_read | perms::owner_write | perms::owner_exec,
		errorCode
	);
	if( errorCode ) {
		dprintf( D_ALWAYS, "Unable to set permissions on staging directory, aborting: %s (%d).\n", errorCode.message().c_str(), errorCode.value() );
		return false;
	}

	int rv = chown( stagingDir.string().c_str(), get_user_uid(), get_user_gid() );
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "Unable change owner of staging directory, aborting: %s (%d)\n", strerror(errno), errno );
		return false;
	}

	return true;
}


bool
HardlinkStagingDirectory::modify() {
	//
	// Assume ownership is correct, but make each file 0444 and each
	// directory (including the root) 0500.
	//
	// This allows root to hardlink each file into place (root can traverse
	// the directory tree even if the starters are running as different OS
	// acconts).  Since the file permisisons are 0444, they can be read, but
	// the source hardlink can not be deleted (because of ownership or that
	// the source directory isn't writable).  Simultaneously, the starter
	// will be able to clean up the destination hardlinks as normal.
	//

	using std::filesystem::perms;
	std::error_code ec;


	TemporaryPrivSentry tps(PRIV_USER);
	dprintf( D_ZKM, "convertToStagingDirectory(): begin.\n" );

	if(! std::filesystem::is_directory( stagingDir, ec )) {
		dprintf( D_ALWAYS, "convertToStagingDirectory(): '%s' not a directory, aborting.\n", stagingDir.string().c_str() );
		return false;
	}

	std::filesystem::permissions(
		stagingDir,
		perms::owner_read | perms::owner_exec,
		ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "convertToStagingDirectory(): Failed to set permissions on staging directory '%s': %s (%d)\n", stagingDir.string().c_str(), ec.message().c_str(), ec.value() );
		return false;
	}

	std::filesystem::recursive_directory_iterator rdi(
		stagingDir, {}, ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "convertToStagingDirectory(): Failed to construct recursive_directory_iterator(%s): %s (%d)\n", stagingDir.string().c_str(), ec.message().c_str(), ec.value() );
		return false;
	}

	for( const auto & entry : rdi ) {
		if( entry.is_directory() ) {
			std::filesystem::permissions(
				entry.path(),
				perms::owner_read | perms::owner_exec,
				ec
			);
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "convertToStagingDirectory(): Failed to set permissions(%s): %s (%d)\n", entry.path().string().c_str(), ec.message().c_str(), ec.value() );
				return false;
			}
			continue;
		}
		std::filesystem::permissions(
			entry.path(),
			perms::owner_read | perms::group_read | perms::others_read,
			ec
		);
		if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "convertToStagingDirectory(): Failed to set permissions(%s): %s (%d)\n", entry.path().string().c_str(), ec.message().c_str(), ec.value() );
		return false;
		}
	}


	dprintf( D_ZKM, "convertToStagingDirectory(): end.\n" );
	return true;
}


bool
check_permissions(
	const std::filesystem::path & l,
	const std::filesystem::perms p
) {
	std::error_code ec;
	auto status = std::filesystem::status( l, ec );
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "check_permissions(): status(%s) failed: %s (%d)\n", l.string().c_str(), ec.message().c_str(), ec.value() );
		return false;
	}
	auto permissions = status.permissions();
	return permissions == p;
}


bool
HardlinkStagingDirectory::stage( const std::filesystem::path & sandbox ) {
	using std::filesystem::perms;
	std::error_code ec;

	dprintf( D_ZKM, "mapContentsOfDirectoryInto(): begin.\n" );

	// We must be root (or the user the common files were transferred by) to
	// traverse into the staging directory, which includes listing its
	// contents, checking the permissions on its files, creating hardlinks
	// to its files, or even seeing if the directory exists all (if we're
	// not running in STARTER_NESTED_SCRATCH mode).
	TemporaryPrivSentry tps(PRIV_ROOT);

	if(! std::filesystem::is_directory( sandbox, ec )) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' not a directory, aborting.\n", sandbox.string().c_str() );
		return false;
	}

	if(! std::filesystem::is_directory( stagingDir, ec )) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' not a directory, aborting.\n", stagingDir.string().c_str() );
		return false;
	}

	if(! check_permissions( stagingDir, perms::owner_read | perms::owner_exec )) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", stagingDir.string().c_str() );
		return false;
	}

	// To be clear: this recurses into subdirectories for us.
	std::filesystem::recursive_directory_iterator rdi(
		stagingDir, {}, ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Failed to construct recursive_directory_iterator(%s): %s (%d)\n", stagingDir.string().c_str(), ec.message().c_str(), ec.value() );
		return false;
	}

	for( const auto & entry : rdi ) {
		// dprintf( D_ZKM, "mapContentsOfDirectoryInto(): '%s'\n", entry.path().string().c_str() );
		auto relative_path = entry.path().lexically_relative(stagingDir);
		// dprintf( D_ZKM, "mapContentsOfDirectoryInto(): '%s'\n", relative_path.string().c_str() );
		if( entry.is_directory() ) {
			if(! check_permissions( entry, perms::owner_read | perms::owner_exec )) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", stagingDir.string().c_str() );
				return false;
			}

			auto dir = sandbox / relative_path;
			std::filesystem::create_directory( dir, ec );
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Failed to create_directory(%s): %s (%d)\n", (sandbox/relative_path).string().c_str(), ec.message().c_str(), ec.value() );
				return false;
			}

			int rv = chown( dir.string().c_str(), get_user_uid(), get_user_gid() );
			if( rv != 0 ) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Unable change owner of common input directory, aborting: %s (%d)\n", strerror(errno), errno );
				return false;
			}

			continue;
		} else {
			dprintf( D_ZKM, "mapContentsOfDirectoryInto(): hardlink(%s, %s)\n", (sandbox/relative_path).string().c_str(), entry.path().string().c_str() );

			if(! check_permissions( entry, perms::owner_read | perms::group_read | perms::others_read )) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", stagingDir.string().c_str() );
				return false;
			}

			// If this file already exists, it must have been written
			// there by (the proc-specific) input file transfer.  Since that
			// _should_ win, semantically, at some point we'll have to fix
			// this to ignore E_EXISTS.
			std::filesystem::create_hard_link(
				entry.path(), sandbox/relative_path, ec
			);
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Failed to create_hard_link(%s, %s): %s (%d)\n", entry.path().string().c_str(), (sandbox/relative_path).string().c_str(), ec.message().c_str(), ec.value() );
				return false;
			}

			int rv = chown( entry.path().string().c_str(), get_user_uid(), get_user_gid() );
			if( rv != 0 ) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Unable change owner of common input file hardlink, aborting: %s (%d)\n", strerror(errno), errno );
				return false;
			}
		}
	}


	dprintf( D_ZKM, "mapContentsOfDirectoryInto(): end.\n" );
	return true;
}


bool
CopyStagingDirectory::create() {
	return false;
}


bool
CopyStagingDirectory::modify() {
	return false;
}


bool
CopyStagingDirectory::stage( const std::filesystem::path & s ) {
	return false;
}
