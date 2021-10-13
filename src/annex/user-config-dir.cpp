#include "condor_common.h"

#include <string>
#include "user-config-dir.h"

#include "condor_config.h"
#include "filename_tools.h"
#include "directory.h"

bool
createUserConfigDir( std::string & directory ) {
	std::string userConfigName;
	std::string userConfigSource;
	param( userConfigName, "USER_CONFIG_FILE" );
	if(! userConfigName.empty()) {
		find_user_file( userConfigSource, userConfigName.c_str(), false, false );
		if(! userConfigSource.empty()) {
			// Create the containing directory if necessary, and only the
			// containing directory -- don't do anything stupid if the
			// user configuration directory is misconfigured.
			std::string dir, file;
			filename_split( userConfigSource.c_str(), dir, file );
			if(! IsDirectory( dir.c_str() )) {
				mkdir( dir.c_str(), 0755 );
			}

			directory = dir;
			return true;
		} else {
			fprintf( stderr, "Unable to locate your user configuration file.  " );
			return false;
		}
	} else {
		fprintf( stderr, "Your HTCondor installation is configured to ignore user configuration files.  Contact your system administrator.  " );
		return false;
	}
}

