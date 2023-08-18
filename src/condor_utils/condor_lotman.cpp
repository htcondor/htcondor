#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"

#ifndef WIN32
#include <dlfcn.h>
#endif

#include "condor_lotman.h"
// Getting the picojson headers from jwt-cpp
// versus importing directly, because doing so
// can cause errors by overriding how other files
// happen to use picojson
GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(cast-qual)
#include "jwt-cpp/jwt.h"
GCC_DIAG_ON(float-equal)
GCC_DIAG_ON(cast-qual)

#include <filesystem>


#ifdef HAVE_EXT_LOTMAN
#include <lotman/lotman.h>
#endif

namespace {

// Initialize function pointers for any LotMan functions that are needed 
static int (*lotman_lot_exists_ptr)(const char *lot_name, char **err_msg) = nullptr;
static int (*lotman_set_context_str_ptr)(const char *key, const char *value, char **err_msg) = nullptr;
static int (*lotman_add_lot_ptr)(const char *lotman_JSON_str, char **err_msg) = nullptr;
static int (*lotman_add_to_lot_ptr)(const char *lotman_JSON_str, char **err_msg) = nullptr;
static int (*lotman_rm_paths_from_lots_ptr)(const char *remove_dirs_JSON_str, char **err_msg) = nullptr;
static int (*lotman_update_lot_usage_by_dir_ptr)(const char *update_JSON_str, bool deltaMode, char **err_msg) = nullptr;

#define LIBLOTMAN_SO "libLotMan.so.0"

bool g_init_tried = false;
bool g_init_success = false;

int user_has_lot(const char *user) {
    dprintf( D_FULLDEBUG, "Checking if user %s has a lot\n", user );

    // We assume that the name of the lot is the same as the uid
    char *err_msg;
    int rc = lotman_lot_exists_ptr(user, &err_msg);
    if (rc < 0) {
        dprintf( D_ALWAYS, "Error while checking lot's existence: %s", err_msg );
        return -1;
    }

    return rc;
}

std::string
create_user_lot_json(const char *user) {

    // For now, we are initializing everything to 0, since Condor doesn't yet use these stats
    picojson::object base;
    base["lot_name"] = picojson::value(user);
    base["owner"] = picojson::value("condor");
    base["parents"] = picojson::value(picojson::array{picojson::value(user)});

    picojson::object management_policy_attrs;
    management_policy_attrs["dedicated_GB"] = picojson::value(static_cast<double>(0));
    management_policy_attrs["opportunistic_GB"] = picojson::value(static_cast<double>(0));
    management_policy_attrs["max_num_objects"] = picojson::value(static_cast<double>(0));
    management_policy_attrs["creation_time"] = picojson::value(static_cast<double>(0));
    management_policy_attrs["expiration_time"] = picojson::value(static_cast<double>(0));
    management_policy_attrs["deletion_time"] = picojson::value(static_cast<double>(0));

    base["management_policy_attrs"] = picojson::value(management_policy_attrs);

    return picojson::value(base).serialize();
}

bool create_user_lot(const char *user) {
    // Check for default lot first, which must be set before other lots

    // We'll need to figure out how to set context. Maybe this should be an input to the top-level function
    // For now, hard coding caller as the "condor" user, who is also being assigned owner to all the lots.
    char *err_msg;
    const char *caller = "condor";
    dprintf( D_FULLDEBUG, "Setting context for LotMan: caller=%s\n", caller );
    auto rc = lotman_set_context_str_ptr("caller", caller, &err_msg); 
    if (rc != 0) { // error setting context
        dprintf( D_ALWAYS, "Failed to set LotMan caller context for caller %s: %s\n", caller, err_msg );
        free(err_msg);
        return false;
    }

    rc = lotman_lot_exists_ptr("default", &err_msg); // 0 for false, 1 for true, negative for err
    if ( rc <0 ) {
        dprintf( D_ALWAYS, "Error while checking if default lot exists: %s\n", err_msg );
        free(err_msg);
        return false;
    }
    else if ( rc == 0 ) { // There's no default lot, we need to create one!
        dprintf( D_FULLDEBUG, "LotMan's default lot did not exist, creating now\n" );
        std::string default_lot = create_user_lot_json("default");
        rc = lotman_add_lot_ptr(default_lot.c_str(), &err_msg);
        if ( rc != 0 ) {
            dprintf( D_ALWAYS, "Failed to create default lot: %s\n", err_msg );
            free(err_msg);
            return false;
        }
        // Default lot now exists
    }
    else {
        dprintf( D_FULLDEBUG, "The default lot existed, moving on...\n" );
    }

    rc = lotman_lot_exists_ptr(user, &err_msg);
    if ( rc < 0 ) {
        dprintf( D_ALWAYS, "Error while checking if lot %s exists: %s\n", user, err_msg );
        free(err_msg);
        return false;
    }

    else if ( rc == 0 ) { // User does not have a lot, we need to create one
        dprintf( D_FULLDEBUG, "The user %s did not have a lot, attempting to create one now\n", user );

        std::string user_lot_json = create_user_lot_json(user);
        rc = lotman_add_lot_ptr(user_lot_json.c_str(), &err_msg);
        if ( rc != 0 ) {
            dprintf( D_ALWAYS, "Error while trying to add lot for user %s: %s\n", user, err_msg);
            dprintf( D_FULLDEBUG, "The lot's json input is %s\n", user_lot_json.c_str());
            free(err_msg);
            return false;
        }
    }
    else {
        dprintf( D_FULLDEBUG, "User %s already has a lot, moving on...\n", user );
    }

    return true;
}

std::string create_add_dirs_json(const char *lot_name, std::map<std::string, bool>  paths) {
    
    picojson::object base;
    base["paths"] = picojson::value(picojson::array());
    base["lot_name"] = picojson::value(lot_name);

    for (const auto& [key, value] : paths) {
        picojson::object path_obj;
        path_obj["path"] = picojson::value(key);
        path_obj["recursive"] = picojson::value(value);
        base["paths"].get<picojson::array>().push_back(picojson::value(path_obj));
    }

    return picojson::value(base).serialize();
}

} // anonymous namespace


bool
condor_lotman::init_lotman()
{
	if (g_init_tried) {
		return g_init_success;
	}

#ifndef WIN32
#if defined(DLOPEN_LOTMAN)
	dlerror();
	void *dl_hdl = nullptr;
	if (
		!(dl_hdl = dlopen(LIBLOTMAN_SO, RTLD_LAZY)) ||
		!(lotman_lot_exists_ptr = (int (*)(const char *lot_name, char **err_msg))dlsym(dl_hdl, "lotman_lot_exists")) ||
		!(lotman_set_context_str_ptr = (int (*)(const char *key, const char *value, char **err_msg))dlsym(dl_hdl, "lotman_set_context_str")) ||
		!(lotman_add_lot_ptr = (int (*)(const char *lotman_JSON_str, char **err_msg))dlsym(dl_hdl, "lotman_add_lot")) ||
		!(lotman_add_to_lot_ptr = (int (*)(const char *lotman_JSON_str, char **err_msg))dlsym(dl_hdl, "lotman_add_to_lot")) ||
		!(lotman_rm_paths_from_lots_ptr = (int (*)(const char *remove_dirs_JSON_str, char **err_msg))dlsym(dl_hdl, "lotman_rm_paths_from_lots")) ||
		!(lotman_update_lot_usage_by_dir_ptr = (int (*)(const char *update_JSON_str, bool deltaMode, char **err_msg))dlsym(dl_hdl, "lotman_update_lot_usage_by_dir")) 
	) {
		const char *err_msg = dlerror();
		dprintf( D_ALWAYS, "Failed to dynamically open LotMan library: %s\n", err_msg ? err_msg : "(no error message available)" );
		g_init_success = false;
	} else {
		g_init_success = true;
	}
#else
#if defined(HAVE_EXT_LOTMAN)
    dprintf( D_FULLDEBUG, "Loading external lotman...");

    lotman_lot_exists_ptr = lotman_lot_exists;
    lotman_set_context_str_ptr = lotman_set_context_str;
    lotman_add_lot_ptr = lotman_add_lot;
    lotman_add_to_lot_ptr = lotman_add_to_lot;
    lotman_rm_paths_from_lots_ptr = lotman_rm_paths_from_lots;
    lotman_update_lot_usage_by_dir_ptr = lotman_update_lot_usage_by_dir;

	g_init_success = true;
#else
	dprintf( D_ALWAYS, "LotMan support is not compiled in.\n" );
	g_init_success = false;
#endif
#endif
#else
	dprintf( D_ALWAYS, "LotMan is not supported on Windows.\n" );
	g_init_success = false;
#endif
	g_init_tried = true;

    if (g_init_success) { // only set lot home if we succeeded initialization
        std::string lotdb_loc;
        param(lotdb_loc, "LOTMAN_DB");
        if (lotdb_loc == "auto") {
            if (!param(lotdb_loc, "RUN")) {
                param(lotdb_loc, "LOCK");
            }
            if (!lotdb_loc.empty()) {
                lotdb_loc += "/lot";
            }
        }
        if (!lotdb_loc.empty()) {
            dprintf( D_ALWAYS|D_VERBOSE, "Setting LotMan DB directory to %s\n", lotdb_loc.c_str() );
            char *err = nullptr;
            if (lotman_set_context_str_ptr("lot_home", lotdb_loc.c_str(), &err) < 0) {
                dprintf( D_ALWAYS, "Failed to set LotMan database directory to %s: %s\n", lotdb_loc.c_str(), err );
                free(err);
            }
        }
    }

	return g_init_success;
}




bool condor_lotman::add_dir(const char *user, const char *dir_path, CondorError &err) {
    // Make sure the library is init'd
    if (!condor_lotman::init_lotman()) {
        err.pushf("LOTMAN", 1, "Failed to open LotMan library.");
        return false;
    }

    // Assuming lots are named after their user, we can check if the user
    // already has an assigned lot
    
    dprintf( D_ALWAYS, "Adding for user:path %s:%s\n", user, dir_path);

    char *err_msg;
    const char *caller = "condor";
    dprintf( D_FULLDEBUG, "Setting context for LotMan: caller=%s\n", caller );
    auto rc = lotman_set_context_str_ptr("caller", caller, &err_msg); 
    if (rc != 0) { // error setting context
        dprintf( D_ALWAYS, "Failed to set LotMan caller context for caller %s: %s\n", caller, err_msg );
        free(err_msg);
        return false;
    }
    std::map<std::string, bool> new_dir_map{{dir_path, true}};
    std::string new_dirs_json{create_add_dirs_json(user, new_dir_map)};
    rc = lotman_add_to_lot_ptr(new_dirs_json.c_str(), &err_msg);
    if ( rc != 0 ) {
        dprintf( D_ALWAYS, "Unable to add the directory %s to %s's lot: %s\n", dir_path, user, err_msg);
        dprintf( D_FULLDEBUG, "The dirs new_dirs_json was: %s\n", new_dirs_json.c_str());
        free(err_msg);
        return false;
    }


    dprintf( D_ALWAYS, "The directory %s was added to %s's lot\n", dir_path, user);
    return true;
}

bool condor_lotman::rm_dir(const char *dir_path, std::vector<std::string> excluded_files) {
    // Make sure the library is init'd
    CondorError err;
    if (!condor_lotman::init_lotman()) {
        err.pushf( "LOTMAN", 1, "Failed to open LotMan library." );
        return false;
    }

    dprintf( D_FULLDEBUG, "Attempting to delete directory %s from the lot db", dir_path );

    char *err_msg;
    const char *caller = "condor";
    dprintf( D_FULLDEBUG, "Setting context for LotMan: caller=%s\n", caller );
    auto rc = lotman_set_context_str_ptr("caller", caller, &err_msg); 
    if (rc != 0) { // error setting context
        dprintf( D_ALWAYS, "Failed to set LotMan caller context for caller %s: %s\n", caller, err_msg );
        free(err_msg);
        return false;
    }

    // Get the size and object count of the directory we're removing.
    // Wrapped in a try block because if the file doesn't exist or the condor
    // user doesn't have perms, weird things can happen
    try {
        std::uintmax_t dir_size = 0;
        size_t obj_count = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path)) {
            if (std::filesystem::is_regular_file(entry)) {
                std::string filename = entry.path().filename().string();
                if (std::find(excluded_files.begin(), excluded_files.end(), filename) == excluded_files.end()) {
                    dir_size += std::filesystem::file_size(entry);
                    ++obj_count;
                }
            }
        }

        dprintf( D_FULLDEBUG, "LotMan: Removing %lu objects of total size %lu from lot containing dir %s\n", obj_count, dir_size, dir_path );
        if (!update_usage(dir_path, -1*obj_count, -1*dir_size, true, false, err)) {
            dprintf( D_ALWAYS, "Failed to track removal in lot\n" );
        }
    }
    catch (std::exception &exc) {
        dprintf( D_ALWAYS, "There was an exception: %s\n)", exc.what() );
    }

    // Use the dir path to build the removal JSON
    picojson::object removal_JSON;
    removal_JSON["paths"] = picojson::value(picojson::array());
    removal_JSON["paths"].get<picojson::array>().push_back(picojson::value(dir_path));

    // Try to remove the dir from whichever lot claims it
    rc = lotman_rm_paths_from_lots_ptr(picojson::value(removal_JSON).serialize().c_str(), &err_msg);
    if (rc != 0) { // There was an error
        dprintf( D_ALWAYS, "Failed to remove the dir %s from the lot db\n", dir_path );
        free(err_msg);
        return false;

    }

    dprintf( D_ALWAYS, "The dir %s was removed from the lot db\n", dir_path );
    return true;
}

bool
condor_lotman::update_usage(const char *dir_path, int num_obj, 
                            int dir_size, bool delta_mode, 
                            bool create_lot_if_needed, CondorError &err) {
    // Make sure the library is init'd
    if (!condor_lotman::init_lotman()) {
        err.pushf( "LOTMAN", 1, "Failed to open LotMan library." );
        return false;
    }

    dprintf( D_ALWAYS, "Attempting to update spool usage for dir %s\n", dir_path );
    dprintf( D_FULLDEBUG, "Writing values num_obj:%i, dir_size:%i with delta_mode:%d\n", num_obj, dir_size, delta_mode );

    if (create_lot_if_needed) {
        // Get the user ID and check if they already have a lot
        uid_t uid = get_user_uid();
        std::string user = std::to_string(uid);
        dprintf( D_FULLDEBUG, "Checking whether lot needs to be created for user with uid %s\n", user.c_str());

        int rc = user_has_lot(user.c_str());
        if (rc < 0) {
            dprintf( D_ALWAYS, "Failed to check for user %s's lot.\n", user.c_str() );
            return false;
        }
        if (rc == 0) { // user does not have a lot
            dprintf( D_ALWAYS, "User %s did not have a lot, adding one...\n", user.c_str() );
            if (!create_user_lot(user.c_str())) {
                dprintf( D_ALWAYS, "Failed to add lot for user %s\n", user.c_str() );
                return false;
            }
        }

        // Assume the dir path has not been added before, and add it to user's lot
        if (!add_dir(user.c_str(), dir_path, err)) {
            dprintf( D_FULLDEBUG, "Failed to add path for user %s. It's probably already tied to the user!\n", user.c_str());
        }

    }

    picojson::object base;
    base["path"] = picojson::value(dir_path);
    base["num_obj"] = picojson::value(static_cast<double>(num_obj));
    base["includes_subdirs"] = picojson::value(false);
    base["size_GB"] = picojson::value(static_cast<double>(dir_size) / (1024.0 * 1024.0 * 1024.0));
    base["subdirs"] = picojson::value(picojson::array());

    picojson::array outer_array;
    outer_array.push_back(picojson::value(base));

    dprintf( D_FULLDEBUG, "I will attempt to update the lot with the following input:\n%s\n", picojson::value(outer_array).serialize().c_str() );

    char *err_msg;
    auto rv = lotman_update_lot_usage_by_dir_ptr(picojson::value(outer_array).serialize().c_str(), delta_mode, &err_msg);
    bool success = (rv == 0);

    if (!success) {
        dprintf( D_ALWAYS, "Failed to update usage for dir %s: %s\n", dir_path, err_msg );
        free(err_msg);
        return false;
    }

    dprintf( D_ALWAYS, "Successfully updated usage of lot who owns dir %s\n", dir_path );
    return true;
}
