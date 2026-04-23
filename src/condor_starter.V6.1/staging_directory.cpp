#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <filesystem>

#include "starter.h"
#include "safe_open.h"
#include "condor_uid.h"

#include "staging_directory.h"

//
// Declare the different staging directory implementations.
//

class HardlinkStagingDirectory : public StagingDirectory {

	public:

		static bool usable();

		virtual std::error_code create();
		virtual std::error_code modify();
		virtual std::error_code map( const std::filesystem::path & destination );

		virtual ~HardlinkStagingDirectory() = default;

	protected:

		HardlinkStagingDirectory(
			const std::filesystem::path & d,
			const std::string & c
		) : StagingDirectory(d, c) { }

		HardlinkStagingDirectory(
			const std::string & s
		) : StagingDirectory(s) { }


	friend class StagingDirectoryFactory;
};


class BindMountStagingDirectory : public StagingDirectory {

	public:

		static  bool usable();

		virtual std::error_code create();
		virtual std::error_code modify();
		virtual std::error_code  map( const std::filesystem::path & destination );

		virtual ~BindMountStagingDirectory() = default;

	protected:

		BindMountStagingDirectory(
			const std::filesystem::path & d,
			const std::string & c
		) : StagingDirectory(d, c) { }

		BindMountStagingDirectory(
			const std::string & s
		) : StagingDirectory(s) { }


	friend class StagingDirectoryFactory;
};


class CopyStagingDirectory : public StagingDirectory {

	public:

		static bool usable();

		virtual std::error_code create();
		virtual std::error_code modify();
		virtual std::error_code map( const std::filesystem::path & destination );

		virtual ~CopyStagingDirectory() = default;

	protected:

		CopyStagingDirectory(
			const std::filesystem::path & d,
			const std::string & c
		) : StagingDirectory(d, c) { }

		CopyStagingDirectory(
			const std::string & s
		) : StagingDirectory(s) { }


	friend class StagingDirectoryFactory;
};


//
// Implement the StagingDirectoryFactory(), which decides at runtime which
// concrete StagingDirectory subclass we'll use.
//


StagingDirectoryFactory::StagingDirectoryFactory() {
	if( HardlinkStagingDirectory::usable() ) {
		this->typeToUse = StagingDirectoryType::Hardlink;
	} else if( BindMountStagingDirectory::usable() ) {
		this->typeToUse = StagingDirectoryType::BindMount;
	} else if( CopyStagingDirectory::usable() ) {
		this->typeToUse = StagingDirectoryType::Copy;
	}
}


std::unique_ptr<StagingDirectory>
StagingDirectoryFactory::make(
	const std::filesystem::path & directory,
	const std::string & catalogName
) {
	switch( this->typeToUse ) {
		case StagingDirectoryType::Hardlink: {
			auto * p = new HardlinkStagingDirectory( directory, catalogName );
			return std::unique_ptr<HardlinkStagingDirectory>(p);
		} break;

		case StagingDirectoryType::BindMount: {
			auto * p = new BindMountStagingDirectory( directory, catalogName );
			return std::unique_ptr<BindMountStagingDirectory>(p);
		} break;

		case StagingDirectoryType::Copy: {
			auto * p = new CopyStagingDirectory( directory, catalogName );
			return std::unique_ptr<CopyStagingDirectory>(p);
		} break;

		case StagingDirectoryType::MIN:
		case StagingDirectoryType::MAX:
		case StagingDirectoryType::INVALID: {
			dprintf( D_ALWAYS, "Invalid type-to-use in StagingDirectoryFactory::make().\n" );
			return nullptr;
		} break;
	}

	// Why does the compiler think this is necessary?
	return nullptr;
}


std::unique_ptr<StagingDirectory>
StagingDirectoryFactory::make(
	const std::string & stagingDirectory
) {
	switch( this->typeToUse ) {
		case StagingDirectoryType::Hardlink: {
			auto * p = new HardlinkStagingDirectory( stagingDirectory );
			return std::unique_ptr<HardlinkStagingDirectory>(p);
		} break;

		case StagingDirectoryType::BindMount: {
			auto * p = new BindMountStagingDirectory( stagingDirectory );
			return std::unique_ptr<BindMountStagingDirectory>(p);
		} break;

		case StagingDirectoryType::Copy: {
			auto * p = new CopyStagingDirectory( stagingDirectory );
			return std::unique_ptr<CopyStagingDirectory>(p);
		} break;

		case StagingDirectoryType::MIN:
		case StagingDirectoryType::MAX:
		case StagingDirectoryType::INVALID: {
			dprintf( D_ALWAYS, "Invalid type-to-use in StagingDirectoryFactory::make().\n" );
			return nullptr;
		} break;
	}

	// Why does the compiler think this is necessary?
	return nullptr;
}


const std::error_code ec_true =  std::error_code(0, std::system_category());
const std::error_code ec_false = std::error_code(1, std::system_category());


//
// The platform-specific code begins here.
//
#if defined(WINDOWS)

// The Windows-specific code ERROR_CALL_NOT_IMPLEMENTED would be appropriate
// here; we could also use ENOSYS ("function not implemented"), but since we
// should never be calling these functions anyway, let's not bother.

std::error_code
createStagingDirectory(
	const std::filesystem::path & parentDir,
	const std::filesystem::path & stagingDir
) {
	return ec_false;
}


std::error_code
convertToStagingDirectory(
	const std::filesystem::path & location
) {
	return ec_false;
}


std::error_code
mapContentsOfDirectoryInto(
	const std::filesystem::path & location,
	const std::filesystem::path & sandbox
) {
	return ec_false;
}


std::error_code
bindMountContentsOfDirectoryInto(
	const std::filesystem::path & location,
	const std::filesystem::path & sandbox
) {
	return ec_false;
}


std::error_code
copyContentsOfDirectoryInto(
	const std::filesystem::path & location,
	const std::filesystem::path & sandbox
) {
	return ec_false;
}


#else /* WINDOWS */


std::error_code
createStagingDirectory( const std::filesystem::path & parentDir, const std::filesystem::path & stagingDir ) {
	using std::filesystem::perms;

	// We could check STARTER_NESTED_SCRATCH to see if we needed
	// to create this directory as PRIV_CONDOR instead of PRIV_USER,
	// but since we'd need to escalate to root to chown afterwards
	// anyway, let's not duplicate code for now.
	TemporaryPrivSentry tps(PRIV_ROOT);

	std::error_code errorCode;
	std::filesystem::create_directories( stagingDir, errorCode );
	if( errorCode ) {
		dprintf( D_ALWAYS, "Unable to create staging directory '%s', aborting: %s (%d)\n", stagingDir.string().c_str(), errorCode.message().c_str(), errorCode.value() );
		return errorCode;
	}

	std::filesystem::permissions(
		parentDir,
		perms::owner_read | perms::owner_write | perms::owner_exec,
		errorCode
	);
	if( errorCode ) {
		dprintf( D_ALWAYS, "Unable to set permissions on directory %s, aborting: %s (%d).\n", parentDir.string().c_str(), errorCode.message().c_str(), errorCode.value() );
		return errorCode;
	}

	std::filesystem::permissions(
		stagingDir,
		perms::owner_read | perms::owner_write | perms::owner_exec,
		errorCode
	);
	if( errorCode ) {
		dprintf( D_ALWAYS, "Unable to set permissions on directory %s, aborting: %s (%d).\n", stagingDir.string().c_str(), errorCode.message().c_str(), errorCode.value() );
		return errorCode;
	}

	int rv = chown( parentDir.string().c_str(), get_user_uid(), get_user_gid() );
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "Unable to change owner of directory %s, aborting: %s (%d)\n", parentDir.string().c_str(), strerror(errno), errno );
		return std::error_code(errno, std::system_category());
	}

	rv = chown( stagingDir.string().c_str(), get_user_uid(), get_user_gid() );
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "Unable to change owner of directory %s, aborting: %s (%d)\n", stagingDir.string().c_str(), strerror(errno), errno );
		return std::error_code(errno, std::system_category());
	}

	return ec_true;
}


//
// Make each file 0444 and each directory (including the root) 0500.
//
// This allows root to hardlink each file into place (root can traverse
// the directory tree even if the starters are running as different OS
// accounts).  Since the file permisisons are 0444, they can be read, but
// the source hardlink can not be deleted (because of ownership or that
// the source directory isn't writable).  Simultaneously, the starter
// will be able to clean up the destination hardlinks as normal.
//
std::error_code
convertToStagingDirectory(
	const std::filesystem::path & location
) {
	using std::filesystem::perms;
	std::error_code ec;


	TemporaryPrivSentry tps(PRIV_USER);

	dprintf( D_ZKM, "convertToStagingDirectory(): begin.\n" );

	if(! std::filesystem::is_directory( location, ec )) {
		dprintf( D_ALWAYS, "convertToStagingDirectory(): '%s' is not a directory, aborting.\n", location.string().c_str() );
		if( ec.value() != 0 ) { return ec; }
		return ec_false;
	}

	std::filesystem::permissions(
		location,
		perms::owner_read | perms::owner_exec,
		ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "convertToStagingDirectory(): Failed to set permissions on staging directory '%s': %s (%d)\n", location.string().c_str(), ec.message().c_str(), ec.value() );
		return ec;
	}

	std::filesystem::recursive_directory_iterator rdi(
		location, {}, ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "convertToStagingDirectory(): Failed to construct recursive_directory_iterator(%s): %s (%d)\n", location.string().c_str(), ec.message().c_str(), ec.value() );
		return ec;
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
				return ec;
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
			return ec;
		}
	}


	{
		TemporaryPrivSentry tpt(PRIV_ROOT);

		auto parentDir = location.parent_path();
		int rv = chown( parentDir.string().c_str(), get_condor_uid(), get_condor_gid() );
		if( rv != 0 ) {
			dprintf( D_ALWAYS, "Unable to change owner of directory %s, aborting: %s (%d)\n", parentDir.string().c_str(), strerror(errno), errno );
			return std::error_code(errno, std::system_category());
		}
	}


	dprintf( D_ZKM, "convertToStagingDirectory(): end.\n" );
	return ec_true;
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


std::error_code
mapContentsOfDirectoryInto(
	const std::filesystem::path & location,
	const std::filesystem::path & sandbox
) {
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
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' is not a directory, aborting.\n", sandbox.string().c_str() );
		if( ec.value() != 0 ) { return ec; }
		return ec_false;
	}

	if(! std::filesystem::is_directory( location, ec )) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' is not a directory, aborting.\n", location.string().c_str() );
		if( ec.value() != 0 ) { return ec; }
		return ec_false;
	}

	if(! check_permissions( location, perms::owner_read | perms::owner_exec )) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
		return ec_false;
	}

	// To be clear: this recurses into subdirectories for us.
	std::filesystem::recursive_directory_iterator rdi(
		location, {}, ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Failed to construct recursive_directory_iterator(%s): %s (%d)\n", location.string().c_str(), ec.message().c_str(), ec.value() );
		return ec;
	}

	for( const auto & entry : rdi ) {
		// dprintf( D_ZKM, "mapContentsOfDirectoryInto(): '%s'\n", entry.path().string().c_str() );
		auto relative_path = entry.path().lexically_relative(location);
		// dprintf( D_ZKM, "mapContentsOfDirectoryInto(): '%s'\n", relative_path.string().c_str() );
		if( entry.is_directory() ) {
			if(! check_permissions( entry, perms::owner_read | perms::owner_exec )) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
				return ec_false;
			}

			auto dir = sandbox / relative_path;
			std::filesystem::create_directory( dir, ec );
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Failed to create_directory(%s): %s (%d)\n", (sandbox/relative_path).string().c_str(), ec.message().c_str(), ec.value() );
				return ec;
			}
			dprintf( D_TEST, "Created mapped directory '%s'\n", relative_path.string().c_str() );

			int rv = chown( dir.string().c_str(), get_user_uid(), get_user_gid() );
			if( rv != 0 ) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Unable to change owner of common input directory, aborting: %s (%d)\n", strerror(errno), errno );
				return std::error_code(errno, std::system_category());
			}

			continue;
		} else {
			dprintf( D_ZKM, "mapContentsOfDirectoryInto(): hardlink(%s, %s)\n", (sandbox/relative_path).string().c_str(), entry.path().string().c_str() );

			if(! check_permissions( entry, perms::owner_read | perms::group_read | perms::others_read )) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
				return ec_false;
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
				return ec;
			}
			dprintf( D_TEST, "Mapped common file '%s'\n", relative_path.string().c_str() );

			int rv = chown( entry.path().string().c_str(), get_user_uid(), get_user_gid() );
			if( rv != 0 ) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): Unable to change owner of common input file hardlink, aborting: %s (%d)\n", strerror(errno), errno );
				return std::error_code(errno, std::system_category());
			}
		}
	}


	dprintf( D_ZKM, "mapContentsOfDirectoryInto(): end.\n" );
	return ec_true;
}


std::error_code
copyContentsOfDirectoryInto(
	const std::filesystem::path & location,
	const std::filesystem::path & sandbox
) {
	using std::filesystem::perms;
	std::error_code ec;

	dprintf( D_ZKM, "CopyStagingDirectory::map(): begin.\n" );

	// We must be root (or the user the common files were transferred by) to
	// traverse into the staging directory, which includes listing its
	// contents, checking the permissions on its files, readin from those
	// files, or even seeing if the directory exists all (if we're
	// not running in STARTER_NESTED_SCRATCH mode).
	TemporaryPrivSentry tps(PRIV_ROOT);

	if(! std::filesystem::is_directory( sandbox, ec )) {
		dprintf( D_ALWAYS, "CopyStagingDirectory::map(): sandbox '%s' is not a directory, aborting.\n", sandbox.string().c_str() );
		if( ec.value() != 0 ) { return ec; }
		return ec_false;
	}

	if(! std::filesystem::is_directory( location, ec )) {
		dprintf( D_ALWAYS, "CopyStagingDirectory::map(): location '%s' is not a directory, aborting.\n", location.string().c_str() );
		if( ec.value() != 0 ) { return ec; }
		return ec_false;
	}

	if(! check_permissions( location, perms::owner_read | perms::owner_exec )) {
		dprintf( D_ALWAYS, "CopyStagingDirectory::map(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
		return ec_false;
	}


	std::filesystem::recursive_directory_iterator di(
		location, {}, ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "CopyStagingDirectory::map(): Failed to construct recursive_directory_iterator(%s): %s (%d)\n", location.string().c_str(), ec.message().c_str(), ec.value() );
		return ec;
	}

	for( const auto & entry : di ) {
		// dprintf( D_ZKM, "CopyStagingDirectory::map(): '%s'\n", entry.path().string().c_str() );
		auto relative_path = entry.path().lexically_relative(location);
		// dprintf( D_ZKM, "CopyStagingDirectory::map(): '%s'\n", relative_path.string().c_str() );

		//
		// For reasons I don't understand std::filesystem::copy() can't copy
		// nested directories whose write bit isn't set, so we have to do
		// all of this by hand, instead.
		//
		if( entry.is_directory() ) {
			if(! check_permissions( entry, perms::owner_read | perms::owner_exec )) {
				dprintf( D_ALWAYS, "CopyStagingDirectory::map(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
				return ec_false;
			}

			auto dir = sandbox / relative_path;
			std::filesystem::create_directory( dir, ec );
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "CopyStagingDirectory::map(): Failed to create_directory(%s): %s (%d)\n", (sandbox/relative_path).string().c_str(), ec.message().c_str(), ec.value() );
				return ec;
			}
			dprintf( D_TEST, "Created mapped directory '%s'\n", relative_path.string().c_str() );

			int rv = chown( dir.string().c_str(), get_user_uid(), get_user_gid() );
			if( rv != 0 ) {
				dprintf( D_ALWAYS, "CopyStagingDirectory::map(): Unable to change owner of common input directory, aborting: %s (%d)\n", strerror(errno), errno );
				return std::error_code(errno, std::system_category());
			}

			continue;
		} else {
			if(! check_permissions( entry, perms::owner_read | perms::group_read | perms::others_read )) {
				dprintf( D_ALWAYS, "CopyStagingDirectory::map(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
				return ec_false;
			}

			std::filesystem::copy(
				entry.path(), sandbox / relative_path,
				// The way common files are now implemented, this should
				// never be necessary, but it's better to be consistent
				// with the other StagingDirectory implementations so that
				// if we ever do go back to transferring uncommon files
				// while waiting for common files, the uncommon ones win.
				/* std::filesystem::copy_options::overwrite_existing | */
				std::filesystem::copy_options::copy_symlinks,
				ec
			);
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS,
					"CopyStagingDirectory::map(): Failed to copy %s to %s: %s (%d)\n",
					entry.path().string().c_str(), (sandbox / relative_path).string().c_str(),
					ec.message().c_str(), ec.value()
				);

				return ec;
			}

			dprintf( D_TEST, "Mapped common file '%s'\n", relative_path.string().c_str() );
		}
	}


	dprintf( D_ZKM, "CopyStagingDirectory::map(): end.\n" );
	return ec_true;
}


#if defined(LINUX)
#include <sys/mount.h>


std::error_code
bindMountContentsOfDirectoryInto(
	const std::filesystem::path & location,
	const std::filesystem::path & sandbox
) {
	using std::filesystem::perms;
	std::error_code ec;

	dprintf( D_ZKM, "bindMountContentsOfDirectoryInto(): begin.\n" );

	// We must be root (or the user the common files were transferred by) to
	// traverse into the staging directory, which includes listing its
	// contents, checking the permissions on its files, creating hardlinks
	// to its files, or even seeing if the directory exists all (if we're
	// not running in STARTER_NESTED_SCRATCH mode).
	TemporaryPrivSentry tps(PRIV_ROOT);

	if(! std::filesystem::is_directory( sandbox, ec )) {
		dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): '%s' is not a directory, aborting.\n", sandbox.string().c_str() );
		if( ec.value() != 0 ) { return ec; }
		return ec_false;
	}

	if(! std::filesystem::is_directory( location, ec )) {
		dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): '%s' is not a directory, aborting.\n", location.string().c_str() );
		if( ec.value() != 0 ) { return ec; }
		return ec_false;
	}

	if(! check_permissions( location, perms::owner_read | perms::owner_exec )) {
		dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
		return ec_false;
	}


	// Like symlinks but not hardlinks, bind mounts can be directories, so
	// we only need to map the top level of the staging directory.
	std::filesystem::directory_iterator di(
		location, {}, ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): Failed to construct recursive_directory_iterator(%s): %s (%d)\n", location.string().c_str(), ec.message().c_str(), ec.value() );
		return ec;
	}

	for( const auto & entry : di ) {
		// dprintf( D_ZKM, "bindMountContentsOfDirectoryInto(): '%s'\n", entry.path().string().c_str() );
		auto relative_path = entry.path().lexically_relative(location);
		// dprintf( D_ZKM, "bindMountContentsOfDirectoryInto(): '%s'\n", relative_path.string().c_str() );

		auto target = sandbox / relative_path;
		if( std::filesystem::exists( target ) ) {
			dprintf( D_ZKM, "bindMountContentsOfDirectoryInto(): ignoring %s because it already exists in the target\n", relative_path.string().c_str() );
			continue;
		}

		// A bind mount is over a particular dirent, just like any other mount.
		if( entry.is_directory() ) {
			if(! check_permissions( entry, perms::owner_read | perms::owner_exec )) {
				dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
				return ec_false;
			}

			std::filesystem::create_directory( target, ec );
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): Failed to create_directory(%s): %s (%d)\n", target.string().c_str(), ec.message().c_str(), ec.value() );
				return ec;
			}

			dprintf( D_TEST, "Created mapped directory '%s'\n", relative_path.string().c_str() );
		} else {
			if(! check_permissions( entry, perms::owner_read | perms::group_read | perms::others_read )) {
				dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
				return ec_false;
			}

			int fd = safe_create_keep_if_exists(
				target.string().c_str(),
				O_RDONLY
			);
			if( fd == -1 ) {
				dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): Failed to touch(%s): %s (%d)\n", target.string().c_str(), strerror(errno), errno );
				return ec_false;
			}
			close(fd);

			dprintf( D_TEST, "Mapped common file '%s'\n", relative_path.string().c_str() );
		}


		//
		// For whatever reason, the man page says that bind mounts can't be
		// read-only for the first time around; you have to mount it and
		// then remount it.
		//

		int rv = mount(
			entry.path().string().c_str(),
			target.string().c_str(),
			NULL, MS_BIND, NULL
		);
		if( rv != 0 ) {
			dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): failed to mount( %s, %s, NULL, MS_BIND, NULL ): %s (%d)\n", entry.path().string().c_str(), target.string().c_str(), strerror(errno), errno );
			return std::error_code(errno, std::system_category());
		}

		rv = mount(
			entry.path().string().c_str(),
			target.string().c_str(),
			NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL
		);
		if( rv != 0 ) {
			dprintf( D_ALWAYS, "bindMountContentsOfDirectoryInto(): failed to mount( %s, %s, NULL, MS_REMOUNT | MS_BIND MS_READONLY, NULL ): %s (%d)\n", entry.path().string().c_str(), target.string().c_str(), strerror(errno), errno );
			return std::error_code(errno, std::system_category());
		}
	}


	dprintf( D_ZKM, "bindMountContentsOfDirectoryInto(): end.\n" );
	return ec_true;
}


#else /* LINUX */


std::error_code
bindMountContentsOfDirectoryInto(
	const std::filesystem::path & location,
	const std::filesystem::path & sandbox
) {
	return ec_false;
}

#endif /* not LINUX */
#endif /* not (LINUX || DARWIN ) */
//
// The platform-specific code ends here.
//


//
// Define HardLinkStagingDirectory.
//

std::error_code
HardlinkStagingDirectory::create() {
	return createStagingDirectory( this->parentDir, this->path() );
}


std::error_code
HardlinkStagingDirectory::modify() {
	return convertToStagingDirectory( this->path() );
}


std::error_code
HardlinkStagingDirectory::map( const std::filesystem::path & destination ) {
	return mapContentsOfDirectoryInto( this->path(), destination );
}


bool
HardlinkStagingDirectory::usable() {
#if defined(WINDOWS)
	return false;
#endif /* WINDOWS */

	bool forbidden = param_boolean( "FORBID_HARDLINK_MAPPING", false );
	bool allowed = ! param_boolean( "STARTD_ENFORCE_DISK_LIMITS", false );

	return (! forbidden) && allowed;
}


//
// Define BindMountStagingDirectory.
//

std::error_code
BindMountStagingDirectory::create() {
    return createStagingDirectory( this->parentDir, this->path() );
}


std::error_code
BindMountStagingDirectory::modify() {
	return convertToStagingDirectory( this->path() );
}


std::error_code
BindMountStagingDirectory::map( const std::filesystem::path & destination ) {
	return bindMountContentsOfDirectoryInto( this->path(), destination );
}


bool
BindMountStagingDirectory::usable() {
#if defined(LINUX)
	bool forbidden = param_boolean( "FORBID_BINDMOUNT_MAPPING", false );
	bool allowed = param_boolean( "ALLOW_LVS_TO_BIND_MOUNT_COMMON_FILES", false );

	return (! forbidden) && allowed;
#else /* LINUX */
    return false;
#endif /* LINUX */
}


//
// Define CopyStagingDirectory.
//

std::error_code
CopyStagingDirectory::create() {
	return createStagingDirectory( this->parentDir, this->path() );
}


std::error_code
CopyStagingDirectory::modify() {
	return convertToStagingDirectory( this->path() );
}


std::error_code
CopyStagingDirectory::map( const std::filesystem::path & destination ) {
	return copyContentsOfDirectoryInto( this->path(), destination );
}


bool
CopyStagingDirectory::usable() {
#if defined(WINDOWS)
	return false;
#endif /* WINDOWS */
	bool forbidden = param_boolean( "FORBID_COPY_MAPPING", false );
	bool allowed = true;

	return (! forbidden) && allowed;
}
