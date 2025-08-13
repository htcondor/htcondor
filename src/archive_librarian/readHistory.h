#pragma once

#include <string>
#include <vector>
#include "JobRecord.h"  // Defines JobRecord, EpochRecord, and FileInfo structs

// TODO: Replace with generic archive reader

/**
 * @brief Parses a HTCondor history file in reverse from the latest offset, extracting JobRecords.
 *
 * @param historyFile Path to the history file.
 * @param out Vector to populate with parsed JobRecords.
 * @param fileInfoOut File metadata (hash, inode, and last offset) used to verify and track file state.
 * @return The offset of the most recent job entry processed (eofOffset), or -1 if file mismatch.
 *
 * Notes:
 * - Skips jobs with offsets <= fileInfoOut.LastOffset.
 * - Verifies file identity before parsing.
 * - Does not write to the database — only reads and returns parsed data.
 */
long readHistoryIncremental(const char *historyFile,
                            std::vector<JobRecord> &out,
                            FileInfo &fileInfoOut);

/**
 * @brief Parses a HTCondor epoch history file in forward order, extracting only SPAWN-type EpochRecords.
 *
 * @param historyFilePath Path to the epoch history file.
 * @param out Vector to populate with parsed EpochRecords (only SPAWN banners are stored).
 * @param fileInfo File metadata (hash, inode, and last offset) used to verify and track file state.
 * @return The new offset reached after parsing, or -1 if file mismatch.
 *
 * Notes:
 * - Only records new entries with offset > fileInfo.LastOffset.
 * - Returns last processed offset to allow incremental updates.
 * - Skips non-SPAWN banners.
 */
long readEpochIncremental(const char *historyFilePath, 
                          std::vector<EpochRecord> &out, 
                          FileInfo &fileInfo);
