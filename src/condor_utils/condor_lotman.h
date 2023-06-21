#ifndef __CONDOR_LOTMAN_H_
#define __CONDOR_LOTMAN_H_

#include <vector>

#include "CondorError.h"

namespace condor_lotman {

// Initialize and handle linking of the LotMan library
// also called first by the functions below
bool init_lotman();

// Add a dir to the lot db, tied to user's lot.
// If there is no lot for the user's uid, this will also try to create one
bool add_dir(const char *user, const char *dir_path, CondorError &err);

// Removes a directory from the lot db and subtracts the dir's stats from lot usage stats,
// except for files passed as excluded_files (files we know we aren't tracking). At a minimum,
// the job's log file should be excluded, as we know we don't track this. 
bool rm_dir(const char *dir_path, std::vector<std::string> excluded_files);

// Update a dir's usage. If delta_mode is false, we're setting absolute stats for the dir, otherwise
// the values are considered deltas. If create_lot_if_needed is true, we try to get the user's UID to
// create the lot if there isn't already one for that UID.
bool update_usage(const char *dir_path, int num_obj, int dir_size, bool delta_mode, bool create_lot_if_needed, CondorError &err);
} //namespace lotman

#endif // __CONDOR_LOTMAN_H_
