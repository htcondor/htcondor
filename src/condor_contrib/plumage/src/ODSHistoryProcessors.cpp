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

// platform includes
#include <libgen.h> // dirname
#include "directory.h"
#include "stat_wrapper.h"

// local includes
#include "ODSHistoryProcessors.h"
#include "ODSDBNames.h"
#include "ODSUtils.h"
#include "ODSHistoryFile.h"

using namespace std;
using namespace mongo;
using namespace plumage::history;
using namespace plumage::util;

typedef set<long unsigned int> HistoryFileListType;
static HistoryFileListType m_historyFiles;
MyString m_path;

#define HISTORY_INDEX_SUFFIX ".idx"

// force a reset of history processing
void plumage::history::initHistoryFiles() {
    m_historyFiles.clear();
    processHistoryDirectory();
    processCurrentHistory(true);
}

/**
 * Process the history directory and maintain the history file map
 *
 * Only handle rotated history files. 
 * For each one that is not in the history file map, create a
 * new HistoryFile, poll it for entries to process, and add it to the
 * map.
 */
void
plumage::history::processHistoryDirectory()
{
    const char *file = NULL;

    Directory dir ( m_path.Value() );
    dir.Rewind();
    while ( ( file = dir.Next() ) )
    {
        // Skip all non-history files, e.g. history and history.*.idx
        if ( strncmp ( file, "history.", 8 ) ||
                !strncmp ( file + ( strlen ( file ) - 4 ), HISTORY_INDEX_SUFFIX, 4 ) ) continue;

        ODSHistoryFile h_file ( ( m_path + DIR_DELIM_STRING + file ).Value() );
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
            h_file.poll ( errstack );
            m_historyFiles.insert ( id );
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
plumage::history::processCurrentHistory(bool force_reset)
{
    static MyString currentHistoryFilename = m_path + DIR_DELIM_STRING + "history";
    static ODSHistoryFile currentHistory ( currentHistoryFilename.Value() );

    CondorError errstack;

    if (force_reset) {
       currentHistory.cleanup();
    }

    // (1)
    long unsigned int id;
    if ( !currentHistory.getId ( id ) || force_reset)
    {
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
    currentHistory.poll ( errstack );

    // (4)
    // If different the file has rotated
    if ( id != stat->st_ino )
    {
        currentHistory = ODSHistoryFile ( currentHistoryFilename.Value() );
        if ( !currentHistory.init ( errstack ) )
        {
            dprintf ( D_ALWAYS, "%s\n", errstack.getFullText().c_str() );
            return;
        }
        ASSERT ( currentHistory.getId ( id ) );
        m_historyFiles.insert ( id );
    }
}
