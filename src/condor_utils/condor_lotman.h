#ifndef __CONDOR_LOTMAN_H_
#define __CONDOR_LOTMAN_H_

#include <vector>

#include "CondorError.h"
#include "condor_classad.h"

namespace condor_lotman {

// Initialize and handle linking of the LotMan library
// also called first by the functions below
bool init_lotman();

// Create a lot with name=lotname
bool create_lot(const char *lotname);

// Add a dir to the lot db, tied to lot named lotname.
// If there is no lot for the user's uid, this will also try to create one
bool add_dir(const char *lotname, const char *dir_path, CondorError &err);

// Removes a directory from the lot db and subtracts the dir's stats from lot usage stats,
// except for files passed as excluded_files (files we know we aren't tracking). At a minimum,
// the job's log file should be excluded, as we know we don't track this. 
bool rm_dir(const char *dir_path, std::vector<std::string> excluded_files);

// Update a dir's usage. If delta_mode is false, we're setting absolute stats for the dir, otherwise
// the values are considered deltas. If either create_lot_if_needed or should_assign_dir is true,
// the function MUST have a jobAd to determine the name of the lot. Consequently, this function should
// never be called with create_lot_if_needed=false and should_assign_dir=true because that implies
// we have an update that requires a new lot, but that we don't intend to add the update to the new lot.
bool update_usage(ClassAd *jobAd, const char *dir_path, int num_obj, int dir_size, bool delta_mode,
    bool create_lot_if_needed, bool should_assign_dir, CondorError &err);
} //namespace lotman

#endif // __CONDOR_LOTMAN_H_
