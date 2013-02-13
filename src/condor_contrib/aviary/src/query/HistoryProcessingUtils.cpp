/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "CondorError.h"
#include "directory.h"
#include "stat_wrapper.h"

//local includes
#include "JobServerJobLogConsumer.h"
#include "Job.h"
#include "HistoryProcessingUtils.h"
#include "HistoryFile.h"
#include "AviaryUtils.h"
#include "Globals.h"

// platform includes
#include <libgen.h> // dirname

// c++ includes
#include <string>
#include <deque>
#include <set>

using namespace aviary::query;

#define HISTORY_INDEX_SUFFIX ".idx"
using namespace std;

typedef set<long unsigned int> HistoryFileListType;
static HistoryFileListType m_historyFiles;
MyString m_path;
bool force_reset=false;

// force a reset of history processing
void aviary::history::process_history_files() {
    if (force_reset) {
        dprintf ( D_FULLDEBUG, "global_reset triggered\n");
        global_reset();
    }
    processHistoryDirectory();
    processOrphanedIndices();
    processCurrentHistory();
}

// Processing jobs from history file must allow for
// duplicates, such as when current history file is renamed.
static
void
process ( const HistoryEntry &entry )
{
    MyString key;

    key.formatstr ( "%d.%d", entry.cluster, entry.proc );

    const char* key_cstr = key.StrDup();

    HistoryJobImpl *hji = new HistoryJobImpl (entry);

    JobCollectionType::const_iterator element = g_jobs.find ( key_cstr );
    if ( g_jobs.end() != element )
    {
	(*element).second->setImpl(hji);
	dprintf ( D_FULLDEBUG, "HistoryJobImpl added to '%s'\n", key_cstr );
    }
    else {
      // live job long gone...could be a restart
      Job* job = new Job(key_cstr);
      job->setImpl(hji);
      g_jobs[key_cstr] = job;
      dprintf ( D_FULLDEBUG, "HistoryJobImpl created for '%s'\n", key_cstr);
    }

	// // debug
	// fprintf ( stdout,
	//          "%ld %ld %s %d %d %d %d %d %s %s %s %s %s %s %s\n",
	//          entry.start, entry.stop, entry.id.c_str(),
	//          entry.cluster, entry.proc,
	//          entry.q_date, entry.status, entry.entered_status,
	//          entry.submission.c_str(),
	//          entry.owner.c_str(),
	//          entry.cmd.c_str(),
	//          entry.args1.c_str(),
	//          entry.args2.c_str(),
	//          entry.release_reason.c_str(),
	//          entry.hold_reason.c_str() );
}

/**
 * Process the history directory and maintain the history file map
 *
 * Only handle rotated history files, those history.* that are not an
 * index. For each one that is not in the history file map, create a
 * new HistoryFile, poll it for entries to process, and add it to the
 * map.
 */
void
aviary::history::processHistoryDirectory()
{
    const char *file = NULL;

    // each time through we rebuild our set of inodes
    if (force_reset) {
        m_historyFiles.clear();
    }

    Directory dir ( m_path.Value() );
    dir.Rewind();
    while ( ( file = dir.Next() ) )
    {
        // Skip all non-history files, e.g. history and history.*.idx
        if ( strncmp ( file, "history.", 8 ) ||
                !strncmp ( file + ( strlen ( file ) - 4 ), HISTORY_INDEX_SUFFIX, 4 ) ) continue;

        HistoryFile h_file ( ( m_path + DIR_DELIM_STRING + file ).Value() );
        CondorError errstack;
        if ( !h_file.init ( errstack ) )
        {
            dprintf ( D_ALWAYS, "%s\n", errstack.getFullText().c_str() );
            return;
        }
        errstack.clear();

        long unsigned int id;
        ASSERT ( h_file.getId ( id ) );
        HistoryFileListType::iterator entry = m_historyFiles.find ( id );
        if ( m_historyFiles.end() == entry )
        {
            HistoryFile::HistoryEntriesTypeIterators ij = h_file.poll ( errstack );
            for ( HistoryFile::HistoryEntriesTypeIterator i = ij.first;
                    i != ij.second;
                    i++ )
            {
                process ( ( *i ) );
            }

            m_historyFiles.insert ( id );
        }
    }
}

/**
 * Process orphaned index files, those that exist but do not have a
 * corresponding history file.
 *
 * Process all .idx files looking for the corresponding HistoryFile in
 * the history file map.
 */
void
aviary::history::processOrphanedIndices()
{
    const char *file = NULL;

    Directory dir ( m_path.Value() );
    dir.Rewind();
    while ( ( file = dir.Next() ) )
    {
        // Skip all non-history index files, e.g. history and history.*
        if ( strncmp ( file, "history.", 8 ) ||
                strncmp ( file + ( strlen ( file ) - 4 ), HISTORY_INDEX_SUFFIX, 4 ) ) continue;

        // XXX: This is ugly because it indicates we know details
        // of how HistoryFile implements index files.

        // The index file is "history.%ld.idx" where %ld is the id
        // of the history file the index is for.

        long unsigned int id;
        int count = sscanf ( file, "history.%ld.idx", &id );
        if ( 1 != count )
        {
            dprintf ( D_ALWAYS, "Error parsing %s, skipping.\n", file );
            continue;
        }

        HistoryFileListType::iterator entry = m_historyFiles.find ( id );
        if ( m_historyFiles.end() == entry )
        {
            // The index is dangling, remove it.
            if ( !dir.Remove_Current_File() )
            {
               dprintf ( D_ALWAYS, "Failed to remove: %s\n", file );
            }
        }
    }
}

/**
 * Process the current history file.
 *
 * 1) check to see if it is properly initialized, recording id (inode)
 * 2) stat the current history file
 * 3) poll for new entries and process them
 * 4) detect rotations
 */
void
aviary::history::processCurrentHistory()
{
    static MyString currentHistoryFilename = m_path + DIR_DELIM_STRING + "history";
    static HistoryFile currentHistory ( currentHistoryFilename.Value() );

    CondorError errstack;

    if (force_reset) {
       currentHistory.cleanup();
    }

	// (1)
    long unsigned int id;
    if ( !currentHistory.getId ( id ) || force_reset)
    {
        // at this point adjust the reset flag
        force_reset = false;
        if ( !currentHistory.init ( errstack ) )
        {
            dprintf ( D_ALWAYS, "%s\n", errstack.getFullText().c_str() );
            return;
        }
        ASSERT ( currentHistory.getId ( id ) );
        m_historyFiles.insert ( id );
    }


    // (2)
    // Stat before poll to handle race of: poll + write + rotate + stat
    StatWrapper stat_wrapper;
    if ( stat_wrapper.Stat ( currentHistoryFilename ) )
    {
        dprintf ( D_ALWAYS, "Failed to stat %s: %d (%s)\n",
                  currentHistoryFilename.Value(),
                  stat_wrapper.GetErrno(), strerror ( stat_wrapper.GetErrno() ) );
        return;
    }
    const StatStructType *stat = stat_wrapper.GetBuf();
    ASSERT ( currentHistory.getId ( id ) );

    // (3)
    errstack.clear();
    HistoryFile::HistoryEntriesTypeIterators poll = currentHistory.poll ( errstack );
    for ( HistoryFile::HistoryEntriesTypeIterator i = poll.first;
            i != poll.second;
            i++ )
    {
        process ( ( *i ) );
    }

    // (4)
    // If different the file has rotated
    if ( id != stat->st_ino )
    {
        currentHistory = HistoryFile ( currentHistoryFilename.Value() );
        if ( !currentHistory.init ( errstack ) )
        {
            dprintf ( D_ALWAYS, "%s\n", errstack.getFullText().c_str() );
            return;
        }
        ASSERT ( currentHistory.getId ( id ) );
        m_historyFiles.insert ( id );
        force_reset = true;
        return;
    }
}
